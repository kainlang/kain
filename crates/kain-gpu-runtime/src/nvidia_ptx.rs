use crate::bindings::{
    GpuBindingAccess, GpuDescriptorKind, GpuDispatchBinding, GpuDispatchRequest, GpuDispatchResult,
};
use crate::executor::ComputeExecutorError;
use kain_core::{shader_artifact_bundle_from_json, ShaderArtifactFormat};
use serde::Deserialize;
use std::ffi::{c_char, c_void, CStr, CString};
use std::fs;
use std::path::Path;
use std::ptr;

type CUdevice = i32;
type CUcontext = *mut c_void;
type CUmodule = *mut c_void;
type CUfunction = *mut c_void;
type CUstream = *mut c_void;
type CUdeviceptr = u64;
type CUresult = i32;
type CUjitOption = i32;

type CuInit = unsafe extern "system" fn(u32) -> CUresult;
type CuDeviceGet = unsafe extern "system" fn(*mut CUdevice, i32) -> CUresult;
type CuCtxCreate = unsafe extern "system" fn(*mut CUcontext, u32, CUdevice) -> CUresult;
type CuCtxDestroy = unsafe extern "system" fn(CUcontext) -> CUresult;
type CuCtxSynchronize = unsafe extern "system" fn() -> CUresult;
type CuModuleLoadDataEx = unsafe extern "system" fn(
    *mut CUmodule,
    *const c_void,
    u32,
    *mut CUjitOption,
    *mut *mut c_void,
) -> CUresult;
type CuModuleUnload = unsafe extern "system" fn(CUmodule) -> CUresult;
type CuModuleGetFunction =
    unsafe extern "system" fn(*mut CUfunction, CUmodule, *const c_char) -> CUresult;
type CuMemAlloc = unsafe extern "system" fn(*mut CUdeviceptr, usize) -> CUresult;
type CuMemFree = unsafe extern "system" fn(CUdeviceptr) -> CUresult;
type CuMemcpyHtoD = unsafe extern "system" fn(CUdeviceptr, *const c_void, usize) -> CUresult;
type CuMemcpyDtoH = unsafe extern "system" fn(*mut c_void, CUdeviceptr, usize) -> CUresult;
type CuLaunchKernel = unsafe extern "system" fn(
    CUfunction,
    u32,
    u32,
    u32,
    u32,
    u32,
    u32,
    u32,
    CUstream,
    *mut *mut c_void,
    *mut *mut c_void,
) -> CUresult;
type CuGetErrorString = unsafe extern "system" fn(CUresult, *mut *const c_char) -> CUresult;

pub struct NvidiaPtxExecutor {
    driver: CudaDriver,
    context: CUcontext,
}

impl NvidiaPtxExecutor {
    pub fn try_new() -> Result<Self, ComputeExecutorError> {
        unsafe {
            let driver = CudaDriver::load()?;
            driver.check((driver.cu_init)(0), "cuInit")?;
            let mut device = 0;
            driver.check((driver.cu_device_get)(&mut device, 0), "cuDeviceGet")?;
            let mut context = ptr::null_mut();
            driver.check(
                (driver.cu_ctx_create)(&mut context, 0, device),
                "cuCtxCreate_v2",
            )?;
            Ok(Self { driver, context })
        }
    }

    pub fn dispatch_from_sidecars(
        &self,
        shader_bundle_path: &Path,
        compute_residency_path: &Path,
        compute_key: &str,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        let (ptx, request) = ptx_dispatch_request_from_sidecars(
            shader_bundle_path,
            compute_residency_path,
            compute_key,
        )?;
        self.run_dispatch_request(&ptx, &request)
    }

    pub fn run_dispatch_request(
        &self,
        ptx_source: &str,
        request: &GpuDispatchRequest,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        let ptx = CString::new(ptx_source).map_err(|_| ComputeExecutorError::PtxContainsNul)?;
        let entry = CString::new(request.entry_point.as_str())
            .map_err(|_| ComputeExecutorError::InvalidPtxEntryName)?;

        unsafe {
            let mut module = ptr::null_mut();
            self.driver.check(
                (self.driver.cu_module_load_data_ex)(
                    &mut module,
                    ptx.as_ptr() as *const c_void,
                    0,
                    ptr::null_mut(),
                    ptr::null_mut(),
                ),
                "cuModuleLoadDataEx",
            )?;
            let module = CudaModule {
                driver: &self.driver,
                module,
            };

            let mut function = ptr::null_mut();
            self.driver.check(
                (self.driver.cu_module_get_function)(&mut function, module.module, entry.as_ptr()),
                "cuModuleGetFunction",
            )?;

            let mut sorted_bindings = request.bindings.iter().collect::<Vec<_>>();
            sorted_bindings.sort_by_key(|binding| binding.binding_slot);
            let buffers = sorted_bindings
                .iter()
                .enumerate()
                .map(|(index, binding)| CudaDeviceBuffer::upload(&self.driver, index, binding))
                .collect::<Result<Vec<_>, _>>()?;

            let mut device_ptrs = buffers
                .iter()
                .map(|buffer| buffer.device_ptr)
                .collect::<Vec<_>>();
            let mut kernel_params = device_ptrs
                .iter_mut()
                .map(|ptr| ptr as *mut CUdeviceptr as *mut c_void)
                .collect::<Vec<_>>();

            self.driver.check(
                (self.driver.cu_launch_kernel)(
                    function,
                    dispatch_group_count(request.dispatch_size[0], request.workgroup_size[0]),
                    dispatch_group_count(request.dispatch_size[1], request.workgroup_size[1]),
                    dispatch_group_count(request.dispatch_size[2], request.workgroup_size[2]),
                    request.workgroup_size[0].max(1),
                    request.workgroup_size[1].max(1),
                    request.workgroup_size[2].max(1),
                    0,
                    ptr::null_mut(),
                    kernel_params.as_mut_ptr(),
                    ptr::null_mut(),
                ),
                "cuLaunchKernel",
            )?;
            self.driver
                .check((self.driver.cu_ctx_synchronize)(), "cuCtxSynchronize")?;

            let mut output_bindings = Vec::new();
            for (binding, buffer) in sorted_bindings.iter().zip(buffers.iter()) {
                if binding.access.is_output() {
                    output_bindings
                        .push((binding.binding_slot, buffer.download(binding.bytes.len())?));
                }
            }

            Ok(GpuDispatchResult {
                dispatch_invocations: request
                    .dispatch_size
                    .iter()
                    .map(|value| *value as u64)
                    .product(),
                output_bindings,
                tensor_binding_count: request.tensor_binding_count,
                stream_binding_count: request.stream_binding_count,
                neural_node_count: request.neural_node_count,
            })
        }
    }
}

impl Drop for NvidiaPtxExecutor {
    fn drop(&mut self) {
        unsafe {
            let _ = (self.driver.cu_ctx_destroy)(self.context);
        }
    }
}

struct CudaModule<'a> {
    driver: &'a CudaDriver,
    module: CUmodule,
}

impl Drop for CudaModule<'_> {
    fn drop(&mut self) {
        unsafe {
            let _ = (self.driver.cu_module_unload)(self.module);
        }
    }
}

struct CudaDeviceBuffer<'a> {
    driver: &'a CudaDriver,
    device_ptr: CUdeviceptr,
}

impl<'a> CudaDeviceBuffer<'a> {
    unsafe fn upload(
        driver: &'a CudaDriver,
        binding_index: usize,
        binding: &GpuDispatchBinding,
    ) -> Result<Self, ComputeExecutorError> {
        if binding.bytes.is_empty() {
            return Err(ComputeExecutorError::EmptyBindingPayload {
                binding: binding_index,
            });
        }
        if binding.descriptor_kind != GpuDescriptorKind::StorageBuffer {
            return Err(ComputeExecutorError::UnsupportedPtxBinding {
                binding: binding.binding_slot,
                kind: binding.descriptor_kind.as_str().to_string(),
            });
        }

        let mut device_ptr = 0;
        driver.check(
            (driver.cu_mem_alloc)(&mut device_ptr, binding.bytes.len()),
            "cuMemAlloc_v2",
        )?;
        let buffer = Self { driver, device_ptr };
        driver.check(
            (driver.cu_memcpy_htod)(
                buffer.device_ptr,
                binding.bytes.as_ptr() as *const c_void,
                binding.bytes.len(),
            ),
            "cuMemcpyHtoD_v2",
        )?;
        Ok(buffer)
    }

    unsafe fn download(&self, byte_len: usize) -> Result<Vec<u8>, ComputeExecutorError> {
        let mut bytes = vec![0u8; byte_len];
        self.driver.check(
            (self.driver.cu_memcpy_dtoh)(
                bytes.as_mut_ptr() as *mut c_void,
                self.device_ptr,
                byte_len,
            ),
            "cuMemcpyDtoH_v2",
        )?;
        Ok(bytes)
    }
}

impl Drop for CudaDeviceBuffer<'_> {
    fn drop(&mut self) {
        unsafe {
            let _ = (self.driver.cu_mem_free)(self.device_ptr);
        }
    }
}

struct CudaDriver {
    _library: DynamicLibrary,
    cu_init: CuInit,
    cu_device_get: CuDeviceGet,
    cu_ctx_create: CuCtxCreate,
    cu_ctx_destroy: CuCtxDestroy,
    cu_ctx_synchronize: CuCtxSynchronize,
    cu_module_load_data_ex: CuModuleLoadDataEx,
    cu_module_unload: CuModuleUnload,
    cu_module_get_function: CuModuleGetFunction,
    cu_mem_alloc: CuMemAlloc,
    cu_mem_free: CuMemFree,
    cu_memcpy_htod: CuMemcpyHtoD,
    cu_memcpy_dtoh: CuMemcpyDtoH,
    cu_launch_kernel: CuLaunchKernel,
    cu_get_error_string: CuGetErrorString,
}

impl CudaDriver {
    unsafe fn load() -> Result<Self, ComputeExecutorError> {
        let library = DynamicLibrary::open()?;
        Ok(Self {
            cu_init: library.symbol("cuInit")?,
            cu_device_get: library.symbol("cuDeviceGet")?,
            cu_ctx_create: library.symbol("cuCtxCreate_v2")?,
            cu_ctx_destroy: library.symbol("cuCtxDestroy_v2")?,
            cu_ctx_synchronize: library.symbol("cuCtxSynchronize")?,
            cu_module_load_data_ex: library.symbol("cuModuleLoadDataEx")?,
            cu_module_unload: library.symbol("cuModuleUnload")?,
            cu_module_get_function: library.symbol("cuModuleGetFunction")?,
            cu_mem_alloc: library.symbol("cuMemAlloc_v2")?,
            cu_mem_free: library.symbol("cuMemFree_v2")?,
            cu_memcpy_htod: library.symbol("cuMemcpyHtoD_v2")?,
            cu_memcpy_dtoh: library.symbol("cuMemcpyDtoH_v2")?,
            cu_launch_kernel: library.symbol("cuLaunchKernel")?,
            cu_get_error_string: library.symbol("cuGetErrorString")?,
            _library: library,
        })
    }

    unsafe fn check(
        &self,
        result: CUresult,
        call: &'static str,
    ) -> Result<(), ComputeExecutorError> {
        if result == 0 {
            return Ok(());
        }
        let mut raw = ptr::null();
        let message = if (self.cu_get_error_string)(result, &mut raw) == 0 && !raw.is_null() {
            CStr::from_ptr(raw).to_string_lossy().to_string()
        } else {
            "unknown CUDA driver error".to_string()
        };
        Err(ComputeExecutorError::CudaDriverCallFailed {
            call,
            code: result,
            message,
        })
    }
}

#[cfg(windows)]
struct DynamicLibrary {
    handle: *mut c_void,
}

#[cfg(windows)]
impl DynamicLibrary {
    unsafe fn open() -> Result<Self, ComputeExecutorError> {
        let name = CString::new("nvcuda.dll").expect("static dll name");
        let handle = LoadLibraryA(name.as_ptr());
        if handle.is_null() {
            return Err(ComputeExecutorError::CudaDriverUnavailable {
                message: "nvcuda.dll could not be loaded from the installed NVIDIA driver"
                    .to_string(),
            });
        }
        Ok(Self { handle })
    }

    unsafe fn symbol<T: Copy>(&self, name: &'static str) -> Result<T, ComputeExecutorError> {
        let c_name = CString::new(name).expect("static symbol name");
        let ptr = GetProcAddress(self.handle, c_name.as_ptr());
        if ptr.is_null() {
            return Err(ComputeExecutorError::CudaDriverSymbolMissing { symbol: name });
        }
        Ok(std::mem::transmute_copy(&ptr))
    }
}

#[cfg(windows)]
impl Drop for DynamicLibrary {
    fn drop(&mut self) {
        unsafe {
            let _ = FreeLibrary(self.handle);
        }
    }
}

#[cfg(windows)]
#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const c_char) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const c_char) -> *mut c_void;
    fn FreeLibrary(module: *mut c_void) -> i32;
}

#[cfg(not(windows))]
struct DynamicLibrary;

#[cfg(not(windows))]
impl DynamicLibrary {
    unsafe fn open() -> Result<Self, ComputeExecutorError> {
        Err(ComputeExecutorError::CudaDriverUnavailable {
            message: "CUDA Driver API dynamic loading is implemented for Windows nvcuda.dll in v1"
                .to_string(),
        })
    }

    unsafe fn symbol<T: Copy>(&self, name: &'static str) -> Result<T, ComputeExecutorError> {
        Err(ComputeExecutorError::CudaDriverSymbolMissing { symbol: name })
    }
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyBundle {
    target: String,
    compute_shaders: Vec<ComputeResidencyEntry>,
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyEntry {
    key: String,
    module_name: String,
    entry_point: String,
    workgroup_size: Option<[u32; 3]>,
    dispatch_size: Option<[u32; 3]>,
    tensor_binding_count: usize,
    stream_binding_count: usize,
    neural_node_count: usize,
    bindings: Vec<ComputeResidencyBinding>,
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyBinding {
    key: String,
    contract: String,
    descriptor_kind: String,
    element_type: String,
    shape: Vec<i64>,
    strides: Vec<i64>,
    access_mode: String,
    slot: u32,
    payload_file: String,
}

fn ptx_dispatch_request_from_sidecars(
    shader_bundle_path: &Path,
    compute_residency_path: &Path,
    compute_key: &str,
) -> Result<(String, GpuDispatchRequest), ComputeExecutorError> {
    let shader_bundle_json =
        fs::read_to_string(shader_bundle_path).map_err(|err| ComputeExecutorError::ReadFile {
            path: shader_bundle_path.display().to_string(),
            message: err.to_string(),
        })?;
    let shader_bundle = shader_artifact_bundle_from_json(&shader_bundle_json).map_err(|err| {
        ComputeExecutorError::ParseShaderBundle {
            path: shader_bundle_path.display().to_string(),
            message: err.to_string(),
        }
    })?;

    let residency_json = fs::read_to_string(compute_residency_path).map_err(|err| {
        ComputeExecutorError::ReadFile {
            path: compute_residency_path.display().to_string(),
            message: err.to_string(),
        }
    })?;
    let residency: ComputeResidencyBundle =
        serde_json::from_str(&residency_json).map_err(|err| {
            ComputeExecutorError::ParseComputeResidency {
                path: compute_residency_path.display().to_string(),
                message: err.to_string(),
            }
        })?;
    if !is_ptx_compatible_residency_target(&residency.target) {
        return Err(ComputeExecutorError::UnsupportedComputeResidencyTarget {
            value: residency.target,
        });
    }
    let entry = residency
        .compute_shaders
        .iter()
        .find(|entry| entry.key == compute_key)
        .ok_or_else(|| ComputeExecutorError::MissingComputeKey {
            compute_key: compute_key.to_string(),
            path: compute_residency_path.display().to_string(),
        })?;
    let ptx = shader_bundle
        .derived_outputs
        .iter()
        .find(|artifact| {
            artifact.format == ShaderArtifactFormat::Ptx
                && artifact.module_name == entry.module_name
        })
        .map(|artifact| artifact.contents.clone())
        .ok_or_else(|| ComputeExecutorError::MissingPtxModule {
            module_name: entry.module_name.clone(),
            path: shader_bundle_path.display().to_string(),
        })?;

    let residency_root = compute_residency_path
        .parent()
        .unwrap_or_else(|| Path::new("."));
    let mut bindings = Vec::with_capacity(entry.bindings.len());
    for binding in &entry.bindings {
        if binding.contract != "kain.shared.buffer" {
            return Err(ComputeExecutorError::UnsupportedSharedBufferContract {
                binding: binding.key.clone(),
                value: binding.contract.clone(),
            });
        }
        let payload_path = residency_root.join(&binding.payload_file);
        let bytes = fs::read(&payload_path).map_err(|err| ComputeExecutorError::ReadFile {
            path: payload_path.display().to_string(),
            message: err.to_string(),
        })?;
        bindings.push(GpuDispatchBinding {
            key: binding.key.clone(),
            binding_slot: binding.slot,
            descriptor_kind: parse_descriptor_kind(&binding.descriptor_kind)?,
            access: parse_access_mode(&binding.access_mode)?,
            bytes,
            element_type: binding.element_type.clone(),
            shape: binding.shape.clone(),
            strides: binding.strides.clone(),
        });
    }

    Ok((
        ptx,
        GpuDispatchRequest {
            module_name: entry.module_name.clone(),
            entry_point: entry.entry_point.clone(),
            workgroup_size: entry.workgroup_size.unwrap_or([8, 1, 1]),
            dispatch_size: entry.dispatch_size.unwrap_or([1, 1, 1]),
            bindings,
            tensor_binding_count: entry.tensor_binding_count,
            stream_binding_count: entry.stream_binding_count,
            neural_node_count: entry.neural_node_count,
        },
    ))
}

fn is_ptx_compatible_residency_target(target: &str) -> bool {
    matches!(
        target.trim().to_ascii_lowercase().as_str(),
        "spirv" | "spv" | "gpu" | "cuda" | "ptx" | "nvptx"
    )
}

fn parse_descriptor_kind(value: &str) -> Result<GpuDescriptorKind, ComputeExecutorError> {
    match value {
        "storage_buffer" => Ok(GpuDescriptorKind::StorageBuffer),
        "uniform_buffer" => Ok(GpuDescriptorKind::UniformBuffer),
        other => Err(ComputeExecutorError::UnsupportedDescriptorKind {
            value: other.to_string(),
        }),
    }
}

fn parse_access_mode(value: &str) -> Result<GpuBindingAccess, ComputeExecutorError> {
    match value {
        "read" => Ok(GpuBindingAccess::Read),
        "write" => Ok(GpuBindingAccess::Write),
        "read_write" => Ok(GpuBindingAccess::ReadWrite),
        other => Err(ComputeExecutorError::UnsupportedAccessMode {
            value: other.to_string(),
        }),
    }
}

fn dispatch_group_count(dispatch: u32, workgroup: u32) -> u32 {
    let safe_dispatch = dispatch.max(1);
    let safe_workgroup = workgroup.max(1);
    ((safe_dispatch - 1) / safe_workgroup) + 1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ptx_dispatch_group_count_rounds_up() {
        assert_eq!(dispatch_group_count(1, 8), 1);
        assert_eq!(dispatch_group_count(8, 8), 1);
        assert_eq!(dispatch_group_count(9, 8), 2);
        assert_eq!(dispatch_group_count(0, 0), 1);
    }

    #[test]
    fn nvidia_ptx_executor_can_launch_tiny_kernel_when_driver_is_available() {
        let Ok(executor) = NvidiaPtxExecutor::try_new() else {
            eprintln!("skipping NVIDIA PTX launch smoke because CUDA Driver API is unavailable");
            return;
        };

        let ptx = r#"
.version 7.8
.target sm_50
.address_size 64

.visible .entry write_one(
    .param .u64 _kain_param_dst
)
{
    .reg .u32 %r<2>;
    .reg .u64 %rd<2>;
    ld.param.u64 %rd1, [_kain_param_dst];
    mov.u32 %r1, 42;
    st.global.u32 [%rd1], %r1;
    ret;
}
"#;

        let result = executor
            .run_dispatch_request(
                ptx,
                &GpuDispatchRequest {
                    module_name: "write_one".to_string(),
                    entry_point: "write_one".to_string(),
                    workgroup_size: [1, 1, 1],
                    dispatch_size: [1, 1, 1],
                    bindings: vec![GpuDispatchBinding {
                        key: "dst".to_string(),
                        binding_slot: 0,
                        descriptor_kind: GpuDescriptorKind::StorageBuffer,
                        access: GpuBindingAccess::Write,
                        bytes: vec![0, 0, 0, 0],
                        element_type: "UInt".to_string(),
                        shape: vec![1],
                        strides: vec![4],
                    }],
                    tensor_binding_count: 0,
                    stream_binding_count: 0,
                    neural_node_count: 0,
                },
            )
            .expect("tiny PTX kernel should launch through the NVIDIA driver");

        assert_eq!(result.dispatch_invocations, 1);
        assert_eq!(
            result.output_bindings,
            vec![(0, 42u32.to_le_bytes().to_vec())]
        );
    }
}
