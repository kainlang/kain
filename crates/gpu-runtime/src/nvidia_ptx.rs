use crate::bindings::{
    GpuBindingAccess, GpuDescriptorKind, GpuDispatchBinding, GpuDispatchRequest, GpuDispatchResult,
};
use crate::executor::{
    c_string_arg, empty_dispatch_result, populate_dispatch_result, write_result_message,
    ComputeExecutorError, GpuRuntimeDispatchRequest, GpuRuntimeDispatchResult,
};
use kain_core::{shader_artifact_bundle_from_json, ShaderArtifactFormat};
use kain_interop::{
    shared_buffer_gpu_binding_view, GpuBindingAccess as InteropAccess,
    GpuDescriptorKind as InteropDescriptorKind, KainSharedBuffer, SharedBufferMetadata,
};
use serde::Deserialize;
use std::collections::HashSet;
use std::ffi::{c_char, c_void, CStr, CString};
use std::fs;
use std::path::{Path, PathBuf};
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
type CuDeviceGetName = unsafe extern "system" fn(*mut c_char, i32, CUdevice) -> CUresult;
type CuDeviceComputeCapability =
    unsafe extern "system" fn(*mut i32, *mut i32, CUdevice) -> CUresult;
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

#[derive(Debug, Clone, PartialEq, Eq)]
struct NvidiaPtxDeviceInfo {
    ordinal: i32,
    name: String,
    compute_capability_major: u32,
    compute_capability_minor: u32,
}

impl NvidiaPtxDeviceInfo {
    fn sm_arch_rank(&self) -> u32 {
        self.compute_capability_major.saturating_mul(10) + self.compute_capability_minor
    }

    fn capability_label(&self) -> String {
        format!(
            "{}.{}",
            self.compute_capability_major, self.compute_capability_minor
        )
    }

    fn sm_arch_label(&self) -> String {
        format_sm_arch(self.sm_arch_rank())
    }

    fn describe(&self) -> String {
        format!(
            "{} (device {}, cc {}, {})",
            self.name,
            self.ordinal,
            self.capability_label(),
            self.sm_arch_label()
        )
    }
}

pub struct NvidiaPtxExecutor {
    driver: CudaDriver,
    context: CUcontext,
    device_info: NvidiaPtxDeviceInfo,
}

impl NvidiaPtxExecutor {
    pub fn try_new() -> Result<Self, ComputeExecutorError> {
        unsafe {
            let driver = CudaDriver::load()?;
            driver.check((driver.cu_init)(0), "cuInit")?;
            let (device, device_info) = driver.primary_device_info(0)?;
            let mut context = ptr::null_mut();
            driver.check(
                (driver.cu_ctx_create)(&mut context, 0, device),
                "cuCtxCreate_v2",
            )?;
            Ok(Self {
                driver,
                context,
                device_info,
            })
        }
    }

    pub fn dispatch_from_sidecars(
        &self,
        shader_bundle_path: &Path,
        compute_residency_path: &Path,
        compute_key: &str,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        let plan = ptx_dispatch_plan_from_sidecars(
            shader_bundle_path,
            compute_residency_path,
            compute_key,
        )?;
        self.run_dispatch_request(&plan.ptx, &plan.request)
    }

    pub fn dispatch_from_sidecars_persisted(
        &self,
        shader_bundle_path: &Path,
        compute_residency_path: &Path,
        compute_key: &str,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        let plan = ptx_dispatch_plan_from_sidecars(
            shader_bundle_path,
            compute_residency_path,
            compute_key,
        )?;
        let result = self.run_dispatch_request(&plan.ptx, &plan.request)?;
        persist_output_bindings_to_sidecars(&plan.output_targets, &result.output_bindings)?;
        Ok(result)
    }

    pub fn run_dispatch_request(
        &self,
        ptx_source: &str,
        request: &GpuDispatchRequest,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        ensure_device_supports_ptx_target(&self.device_info, ptx_source)?;
        validate_ptx_dispatch_request(request)?;
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

#[no_mangle]
pub extern "C" fn kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted(
    request: *const GpuRuntimeDispatchRequest,
    out_result: *mut GpuRuntimeDispatchResult,
) -> i32 {
    let Some(result) = (unsafe { out_result.as_mut() }) else {
        return -1;
    };
    *result = empty_dispatch_result();

    if request.is_null() {
        write_result_message(result, "dispatch request was null");
        return -1;
    }

    let request = unsafe { &*request };
    let shader_bundle_path = match c_string_arg(request.shader_bundle_path) {
        Ok(value) => PathBuf::from(value),
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };
    let compute_residency_path = match c_string_arg(request.compute_residency_path) {
        Ok(value) => PathBuf::from(value),
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };
    let compute_key = match c_string_arg(request.compute_key) {
        Ok(value) => value,
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };

    let executor = match NvidiaPtxExecutor::try_new() {
        Ok(executor) => executor,
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };

    match executor.dispatch_from_sidecars_persisted(
        &shader_bundle_path,
        &compute_residency_path,
        &compute_key,
    ) {
        Ok(dispatch) => {
            populate_dispatch_result(
                result,
                &dispatch,
                &format!(
                    "nvidia ptx dispatch ok on {}",
                    executor.device_info.describe()
                ),
            );
            0
        }
        Err(err) => {
            result.status_code = -1;
            write_result_message(result, &err.to_string());
            -1
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
    cu_device_get_name: CuDeviceGetName,
    cu_device_compute_capability: CuDeviceComputeCapability,
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
            cu_device_get_name: library.symbol("cuDeviceGetName")?,
            cu_device_compute_capability: library.symbol("cuDeviceComputeCapability")?,
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

    unsafe fn primary_device_info(
        &self,
        ordinal: i32,
    ) -> Result<(CUdevice, NvidiaPtxDeviceInfo), ComputeExecutorError> {
        let mut device = 0;
        self.check((self.cu_device_get)(&mut device, ordinal), "cuDeviceGet")?;

        let mut name_buffer = [0 as c_char; 256];
        self.check(
            (self.cu_device_get_name)(name_buffer.as_mut_ptr(), name_buffer.len() as i32, device),
            "cuDeviceGetName",
        )?;

        let mut major = 0;
        let mut minor = 0;
        self.check(
            (self.cu_device_compute_capability)(&mut major, &mut minor, device),
            "cuDeviceComputeCapability",
        )?;

        let name = CStr::from_ptr(name_buffer.as_ptr())
            .to_string_lossy()
            .trim()
            .to_string();
        let device_info = NvidiaPtxDeviceInfo {
            ordinal,
            name: if name.is_empty() {
                format!("NVIDIA device {ordinal}")
            } else {
                name
            },
            compute_capability_major: major.max(0) as u32,
            compute_capability_minor: minor.max(0) as u32,
        };
        Ok((device, device_info))
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
    #[serde(default)]
    shader: String,
    module_name: String,
    #[serde(default)]
    stage: String,
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
    #[serde(default)]
    residency_role: String,
    slot: u32,
    #[serde(default)]
    byte_length: usize,
    payload_file: String,
}

#[derive(Debug)]
struct OutputBindingTarget {
    slot: u32,
    payload_path: PathBuf,
    byte_length: usize,
}

#[derive(Debug)]
struct PtxSidecarDispatchPlan {
    ptx: String,
    request: GpuDispatchRequest,
    output_targets: Vec<OutputBindingTarget>,
}

fn ptx_dispatch_plan_from_sidecars(
    shader_bundle_path: &Path,
    compute_residency_path: &Path,
    compute_key: &str,
) -> Result<PtxSidecarDispatchPlan, ComputeExecutorError> {
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
    let (resolved_module_name, resolved_entry_point) =
        resolve_ptx_shader_entry(&shader_bundle, entry);
    let ptx = shader_bundle
        .derived_outputs
        .iter()
        .find(|artifact| {
            artifact.format == ShaderArtifactFormat::Ptx
                && artifact.module_name == resolved_module_name
        })
        .or_else(|| {
            let mut ptx_artifacts = shader_bundle
                .derived_outputs
                .iter()
                .filter(|artifact| artifact.format == ShaderArtifactFormat::Ptx);
            let first = ptx_artifacts.next()?;
            ptx_artifacts.next().is_none().then_some(first)
        })
        .map(|artifact| artifact.contents.clone())
        .ok_or_else(|| ComputeExecutorError::MissingPtxModule {
            module_name: resolved_module_name.to_string(),
            path: shader_bundle_path.display().to_string(),
        })?;

    let residency_root = compute_residency_path
        .parent()
        .unwrap_or_else(|| Path::new("."));
    let mut bindings = Vec::with_capacity(entry.bindings.len());
    let mut output_targets = Vec::new();
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
        if binding.byte_length != 0 && bytes.len() != binding.byte_length {
            return Err(ComputeExecutorError::BindingPayloadLengthMismatch {
                binding: binding.key.clone(),
                expected: binding.byte_length,
                actual: bytes.len(),
                path: payload_path.display().to_string(),
            });
        }
        let descriptor_kind = parse_descriptor_kind(&binding.descriptor_kind)?;
        let access = parse_access_mode(&binding.access_mode)?;
        validate_residency_role(&binding.key, &binding.residency_role, access)?;
        validate_shared_buffer_binding(
            &residency.target,
            binding,
            &bytes,
            descriptor_kind,
            access,
        )?;
        if access.is_output() {
            output_targets.push(OutputBindingTarget {
                slot: binding.slot,
                payload_path: payload_path.clone(),
                byte_length: binding.byte_length.max(bytes.len()),
            });
        }
        bindings.push(GpuDispatchBinding {
            key: binding.key.clone(),
            binding_slot: binding.slot,
            descriptor_kind,
            access,
            bytes,
            element_type: binding.element_type.clone(),
            shape: binding.shape.clone(),
            strides: binding.strides.clone(),
        });
    }

    Ok(PtxSidecarDispatchPlan {
        ptx,
        request: GpuDispatchRequest {
            module_name: resolved_module_name.to_string(),
            entry_point: resolved_entry_point.to_string(),
            workgroup_size: entry.workgroup_size.unwrap_or([8, 1, 1]),
            dispatch_size: entry.dispatch_size.unwrap_or([1, 1, 1]),
            bindings,
            tensor_binding_count: entry.tensor_binding_count,
            stream_binding_count: entry.stream_binding_count,
            neural_node_count: entry.neural_node_count,
        },
        output_targets,
    })
}

fn resolve_ptx_shader_entry<'a>(
    shader_bundle: &'a kain_core::ShaderArtifactBundle,
    entry: &'a ComputeResidencyEntry,
) -> (&'a str, &'a str) {
    let entry_stage = if entry.stage.is_empty() {
        "compute"
    } else {
        entry.stage.as_str()
    };
    let entry_shader = if entry.shader.is_empty() {
        entry.module_name.as_str()
    } else {
        entry.shader.as_str()
    };
    if let Some(bundle_entry) = shader_bundle.entry_points.iter().find(|bundle_entry| {
        bundle_entry.stage.eq_ignore_ascii_case(entry_stage) && bundle_entry.shader == entry_shader
    }) {
        return (
            bundle_entry.module_name.as_str(),
            bundle_entry.entry_point.as_str(),
        );
    }
    if let Some(bundle_entry) = shader_bundle.entry_points.iter().find(|bundle_entry| {
        bundle_entry.stage.eq_ignore_ascii_case(entry_stage)
            && bundle_entry.module_name == entry.module_name
    }) {
        return (
            bundle_entry.module_name.as_str(),
            bundle_entry.entry_point.as_str(),
        );
    }
    let fallback_entry_point = if entry.entry_point == "main" {
        entry_shader
    } else {
        entry.entry_point.as_str()
    };
    (entry.module_name.as_str(), fallback_entry_point)
}

fn persist_output_bindings_to_sidecars(
    output_targets: &[OutputBindingTarget],
    output_bindings: &[(u32, Vec<u8>)],
) -> Result<(), ComputeExecutorError> {
    for target in output_targets {
        let bytes = output_bindings
            .iter()
            .find(|(slot, _)| *slot == target.slot)
            .map(|(_, bytes)| bytes)
            .ok_or(ComputeExecutorError::MissingOutputBinding {
                binding: target.slot,
            })?;
        if bytes.len() != target.byte_length {
            return Err(ComputeExecutorError::OutputBindingLengthMismatch {
                binding: target.slot,
                expected: target.byte_length,
                actual: bytes.len(),
                path: target.payload_path.display().to_string(),
            });
        }
        fs::write(&target.payload_path, bytes).map_err(|err| ComputeExecutorError::WriteFile {
            path: target.payload_path.display().to_string(),
            message: err.to_string(),
        })?;
    }
    Ok(())
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct PtxTargetArch {
    label: String,
    rank: u32,
}

fn ensure_device_supports_ptx_target(
    device_info: &NvidiaPtxDeviceInfo,
    ptx_source: &str,
) -> Result<PtxTargetArch, ComputeExecutorError> {
    let target = parse_ptx_target_arch(ptx_source)?;
    if device_info.sm_arch_rank() < target.rank {
        return Err(ComputeExecutorError::UnsupportedPtxTargetForDevice {
            required: target.label.clone(),
            device_name: device_info.name.clone(),
            device: format!(
                "cc {} / {}",
                device_info.capability_label(),
                device_info.sm_arch_label()
            ),
        });
    }
    Ok(target)
}

fn parse_ptx_target_arch(ptx_source: &str) -> Result<PtxTargetArch, ComputeExecutorError> {
    for line in ptx_source.lines() {
        let directive = line.split("//").next().unwrap_or("").trim();
        if !directive.starts_with(".target") {
            continue;
        }
        let rest = directive.trim_start_matches(".target").trim();
        let arch_token = rest
            .split(',')
            .next()
            .unwrap_or("")
            .split_whitespace()
            .next()
            .unwrap_or("");
        return parse_sm_arch_token(arch_token).ok_or_else(|| {
            ComputeExecutorError::InvalidPtxTargetArch {
                directive: directive.to_string(),
            }
        });
    }
    Err(ComputeExecutorError::MissingPtxTargetArch)
}

fn parse_sm_arch_token(token: &str) -> Option<PtxTargetArch> {
    let trimmed = token.trim();
    let suffix = trimmed.strip_prefix("sm_")?;
    let digits = suffix
        .chars()
        .take_while(|ch| ch.is_ascii_digit())
        .collect::<String>();
    if digits.is_empty() {
        return None;
    }
    Some(PtxTargetArch {
        label: trimmed.to_string(),
        rank: digits.parse().ok()?,
    })
}

fn validate_ptx_dispatch_request(request: &GpuDispatchRequest) -> Result<(), ComputeExecutorError> {
    let mut seen = HashSet::with_capacity(request.bindings.len());
    for binding in &request.bindings {
        if !seen.insert(binding.binding_slot) {
            return Err(ComputeExecutorError::DuplicateBindingSlot {
                binding: binding.key.clone(),
                slot: binding.binding_slot,
            });
        }
    }
    Ok(())
}

fn validate_residency_role(
    binding_key: &str,
    residency_role: &str,
    access: GpuBindingAccess,
) -> Result<(), ComputeExecutorError> {
    let normalized = residency_role.trim().to_ascii_lowercase();
    if normalized.is_empty() {
        return Ok(());
    }

    let valid = match normalized.as_str() {
        "input" | "required_input" => access == GpuBindingAccess::Read,
        "output" | "required_output" => access.is_output(),
        "scratch" | "scratch_state" => access == GpuBindingAccess::ReadWrite,
        other => {
            return Err(ComputeExecutorError::UnsupportedResidencyRole {
                binding: binding_key.to_string(),
                value: other.to_string(),
            })
        }
    };

    if valid {
        Ok(())
    } else {
        Err(ComputeExecutorError::InvalidResidencyRoleForAccess {
            binding: binding_key.to_string(),
            role: residency_role.to_string(),
            access: access.as_str().to_string(),
        })
    }
}

fn validate_shared_buffer_binding(
    residency_target: &str,
    binding: &ComputeResidencyBinding,
    bytes: &[u8],
    descriptor_kind: GpuDescriptorKind,
    access: GpuBindingAccess,
) -> Result<(), ComputeExecutorError> {
    let metadata = SharedBufferMetadata {
        element_type: binding.element_type.clone(),
        element_size: infer_element_size(&binding.element_type),
        shape: binding.shape.clone(),
        strides: binding.strides.clone(),
        format: Some(binding.element_type.clone()),
        mime_type: Some("application/octet-stream".to_string()),
        source_runtime: "compute-residency".to_string(),
        source_backend: Some(residency_target.to_string()),
        ownership: "owned".to_string(),
        labels: vec![binding.key.clone()],
    };
    let shared = KainSharedBuffer::owned(metadata, bytes.to_vec());
    let _view = shared_buffer_gpu_binding_view(
        &shared,
        match descriptor_kind {
            GpuDescriptorKind::StorageBuffer => InteropDescriptorKind::StorageBuffer,
            GpuDescriptorKind::UniformBuffer => InteropDescriptorKind::UniformBuffer,
        },
        match access {
            GpuBindingAccess::Read => InteropAccess::Read,
            GpuBindingAccess::Write => InteropAccess::Write,
            GpuBindingAccess::ReadWrite => InteropAccess::ReadWrite,
        },
    )
    .map_err(|err| ComputeExecutorError::InvalidSharedBufferBinding {
        binding: binding.key.clone(),
        message: err.to_string(),
    })?;
    Ok(())
}

fn is_ptx_compatible_residency_target(target: &str) -> bool {
    matches!(
        target.trim().to_ascii_lowercase().as_str(),
        "spirv" | "spv" | "gpu" | "cuda" | "ptx" | "nvptx" | "llvm"
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

fn infer_element_size(element_type: &str) -> i64 {
    match element_type {
        "u8" | "i8" | "bool" => 1,
        "u16" | "i16" => 2,
        "u32" | "i32" | "f32" => 4,
        "u64" | "i64" | "f64" => 8,
        "vec2<f32>" | "vec2<i32>" | "vec2<u32>" => 8,
        "vec3<f32>" | "vec3<i32>" | "vec3<u32>" => 12,
        "vec4<f32>" | "vec4<i32>" | "vec4<u32>" => 16,
        _ => 4,
    }
}

fn format_sm_arch(rank: u32) -> String {
    format!("sm_{rank}")
}

fn dispatch_group_count(dispatch: u32, workgroup: u32) -> u32 {
    let safe_dispatch = dispatch.max(1);
    let safe_workgroup = workgroup.max(1);
    ((safe_dispatch - 1) / safe_workgroup) + 1
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn sample_device_info(major: u32, minor: u32) -> NvidiaPtxDeviceInfo {
        NvidiaPtxDeviceInfo {
            ordinal: 0,
            name: "Unit Test GPU".to_string(),
            compute_capability_major: major,
            compute_capability_minor: minor,
        }
    }

    fn write_sample_ptx_bundle(
        path: &Path,
        ptx_contents: &str,
    ) -> Result<(), Box<dyn std::error::Error>> {
        fs::write(
            path,
            serde_json::to_string_pretty(&json!({
                "schema_version": 1,
                "canonical_native_payload": "spirv",
                "spirv_modules": [],
                "reflection": {
                    "emitted": true,
                    "shaders": [],
                    "notes": []
                },
                "resource_layouts": [],
                "entry_points": [
                    {
                        "shader": "CudaFieldKernel",
                        "module_name": "CudaFieldKernel__CudaBlurKernel__CudaColorizeKernel",
                        "entry_point": "CudaFieldKernel",
                        "stage": "compute"
                    }
                ],
                "stage_metadata": [],
                "specialization_constants": [],
                "debug": {
                    "source_map": [],
                    "notes": []
                },
                "derived_outputs": [
                    {
                        "format": "ptx",
                        "module_name": "CudaFieldKernel__CudaBlurKernel__CudaColorizeKernel",
                        "contents": ptx_contents
                    }
                ]
            }))?,
        )?;
        Ok(())
    }

    fn write_sample_ptx_residency(
        path: &Path,
        bindings: serde_json::Value,
    ) -> Result<(), Box<dyn std::error::Error>> {
        fs::write(
            path,
            serde_json::to_string_pretty(&json!({
                "schema_version": 1,
                "target": "llvm",
                "compute_shader_count": 1,
                "compute_shaders": [
                    {
                        "key": "shader::CudaFieldKernel::compute",
                        "shader": "CudaFieldKernel",
                        "module_name": "CudaFieldKernel",
                        "stage": "compute",
                        "entry_point": "main",
                        "source": "kain-core",
                        "execution_domain": "tensor-stream",
                        "workgroup_size": [8, 8, 1],
                        "dispatch_size": [8, 8, 1],
                        "resource_binding_count": 1,
                        "tensor_binding_count": 1,
                        "stream_binding_count": 1,
                        "neural_node_count": 0,
                        "bindings": bindings
                    }
                ]
            }))?,
        )?;
        Ok(())
    }

    #[test]
    fn ptx_dispatch_group_count_rounds_up() {
        assert_eq!(dispatch_group_count(1, 8), 1);
        assert_eq!(dispatch_group_count(8, 8), 1);
        assert_eq!(dispatch_group_count(9, 8), 2);
        assert_eq!(dispatch_group_count(0, 0), 1);
    }

    #[test]
    fn parse_ptx_target_arch_accepts_arch_suffixes() {
        let target = parse_ptx_target_arch(".version 8.0\n.target sm_90a, texmode_independent\n")
            .expect("ptx target");

        assert_eq!(
            target,
            PtxTargetArch {
                label: "sm_90a".to_string(),
                rank: 90,
            }
        );
    }

    #[test]
    fn ensure_device_supports_ptx_target_rejects_newer_arch() {
        let err = ensure_device_supports_ptx_target(
            &sample_device_info(7, 5),
            ".version 7.8\n.target sm_80\n.address_size 64\n",
        )
        .expect_err("sm_80 should not run on a 7.5 device");

        let message = err.to_string();
        assert!(message.contains("sm_80"));
        assert!(message.contains("Unit Test GPU"));
        assert!(message.contains("7.5"));
    }

    #[test]
    fn nvidia_ptx_executor_can_launch_tiny_kernel_when_driver_is_available() {
        let Ok(executor) = NvidiaPtxExecutor::try_new() else {
            eprintln!("skipping NVIDIA PTX launch smoke because CUDA Driver API is unavailable");
            return;
        };

        assert!(!executor.device_info.name.is_empty());
        assert!(executor.device_info.sm_arch_rank() >= 50);

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

    #[test]
    fn persist_output_bindings_to_sidecars_writes_expected_payloads() {
        let temp = tempfile::TempDir::new().expect("temp dir");
        let output_a = temp.path().join("binding_a.bin");
        let output_b = temp.path().join("binding_b.bin");

        persist_output_bindings_to_sidecars(
            &[
                OutputBindingTarget {
                    slot: 7,
                    payload_path: output_b.clone(),
                    byte_length: 3,
                },
                OutputBindingTarget {
                    slot: 3,
                    payload_path: output_a.clone(),
                    byte_length: 4,
                },
            ],
            &[(3, vec![1, 2, 3, 4]), (7, vec![9, 8, 7])],
        )
        .expect("payload files should persist");

        assert_eq!(fs::read(&output_a).expect("binding_a"), vec![1, 2, 3, 4]);
        assert_eq!(fs::read(&output_b).expect("binding_b"), vec![9, 8, 7]);
    }

    #[test]
    fn ptx_dispatch_plan_resolves_bundle_entry_point_and_single_ptx_artifact() {
        let temp = tempfile::TempDir::new().expect("temp dir");
        let shader_bundle_path = temp.path().join("bundle.json");
        let compute_residency_path = temp.path().join("residency.json");
        let payload_path = temp.path().join("payload.bin");

        fs::write(&payload_path, [1u8, 2, 3, 4]).expect("payload");
        write_sample_ptx_bundle(
            &shader_bundle_path,
            ".visible .entry CudaFieldKernel() { ret; }",
        )
        .expect("write bundle");
        write_sample_ptx_residency(
            &compute_residency_path,
            json!([
                {
                    "key": "field",
                    "contract": "kain.shared.buffer",
                    "descriptor_kind": "storage_buffer",
                    "element_type": "u32",
                    "shape": [1],
                    "strides": [1],
                    "access_mode": "write",
                    "residency_role": "output",
                    "slot": 1,
                    "byte_length": 4,
                    "payload_file": "payload.bin"
                }
            ]),
        )
        .expect("write residency");

        let plan = ptx_dispatch_plan_from_sidecars(
            &shader_bundle_path,
            &compute_residency_path,
            "shader::CudaFieldKernel::compute",
        )
        .expect("ptx dispatch plan");

        assert_eq!(
            plan.request.module_name,
            "CudaFieldKernel__CudaBlurKernel__CudaColorizeKernel"
        );
        assert_eq!(plan.request.entry_point, "CudaFieldKernel");
        assert_eq!(plan.ptx, ".visible .entry CudaFieldKernel() { ret; }");
    }

    #[test]
    fn ptx_dispatch_plan_rejects_declared_byte_length_mismatch() {
        let temp = tempfile::TempDir::new().expect("temp dir");
        let shader_bundle_path = temp.path().join("bundle.json");
        let compute_residency_path = temp.path().join("residency.json");
        let payload_path = temp.path().join("payload.bin");

        fs::write(&payload_path, [1u8, 2, 3, 4]).expect("payload");
        write_sample_ptx_bundle(
            &shader_bundle_path,
            ".visible .entry CudaFieldKernel() { ret; }",
        )
        .expect("write bundle");
        write_sample_ptx_residency(
            &compute_residency_path,
            json!([
                {
                    "key": "field",
                    "contract": "kain.shared.buffer",
                    "descriptor_kind": "storage_buffer",
                    "element_type": "u32",
                    "shape": [1],
                    "strides": [1],
                    "access_mode": "write",
                    "residency_role": "required_output",
                    "slot": 1,
                    "byte_length": 8,
                    "payload_file": "payload.bin"
                }
            ]),
        )
        .expect("write residency");

        let err = ptx_dispatch_plan_from_sidecars(
            &shader_bundle_path,
            &compute_residency_path,
            "shader::CudaFieldKernel::compute",
        )
        .expect_err("payload length mismatch should fail");

        assert!(err.to_string().contains("declared 8 bytes"));
    }

    #[test]
    fn ptx_dispatch_request_rejects_duplicate_binding_slots() {
        let err = validate_ptx_dispatch_request(&GpuDispatchRequest {
            module_name: "dup".to_string(),
            entry_point: "dup".to_string(),
            workgroup_size: [1, 1, 1],
            dispatch_size: [1, 1, 1],
            bindings: vec![
                GpuDispatchBinding {
                    key: "a".to_string(),
                    binding_slot: 3,
                    descriptor_kind: GpuDescriptorKind::StorageBuffer,
                    access: GpuBindingAccess::Read,
                    bytes: vec![1, 2, 3, 4],
                    element_type: "u32".to_string(),
                    shape: vec![1],
                    strides: vec![1],
                },
                GpuDispatchBinding {
                    key: "b".to_string(),
                    binding_slot: 3,
                    descriptor_kind: GpuDescriptorKind::StorageBuffer,
                    access: GpuBindingAccess::Write,
                    bytes: vec![0, 0, 0, 0],
                    element_type: "u32".to_string(),
                    shape: vec![1],
                    strides: vec![1],
                },
            ],
            tensor_binding_count: 0,
            stream_binding_count: 0,
            neural_node_count: 0,
        })
        .expect_err("duplicate slots should fail");

        assert!(err.to_string().contains("reuses slot @3"));
    }
}
