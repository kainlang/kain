/// Integration test to verify C++ backend includes low-level helper documentation
///
/// This test validates Task 4.5: C++ backend clarity improvements
use kain_core::types::TypedProgram;
use kain_sys_codegen::codegen_cpp;

#[test]
fn test_cpp_backend_includes_helper_documentation() {
    // Create a minimal typed program
    let program = TypedProgram { items: vec![] };

    // Generate C++ code
    let result = codegen_cpp::generate(&program);
    assert!(result.is_ok(), "C++ generation should succeed");

    let cpp_code = result.unwrap();

    // Verify the documentation header is present
    assert!(
        cpp_code.contains("KAIN Low-Level Memory Helper ABI - C++ Backend"),
        "Generated C++ should include helper ABI documentation header"
    );

    // Verify supported helpers are documented
    assert!(
        cpp_code.contains("SUPPORTED HELPERS:"),
        "Generated C++ should document supported helpers"
    );
    assert!(
        cpp_code.contains("__kain_union_wrap, __kain_union_get, __kain_union_set"),
        "Generated C++ should list union operations as supported"
    );
    assert!(
        cpp_code.contains("__kain_bitfield_get, __kain_bitfield_set"),
        "Generated C++ should list bitfield operations as supported"
    );

    // Verify unsupported helpers are documented
    assert!(
        cpp_code.contains("UNSUPPORTED HELPERS"),
        "Generated C++ should document unsupported helpers"
    );
    assert!(
        cpp_code.contains("__kain_bind_local"),
        "Generated C++ should list bind_local as unsupported"
    );
    assert!(
        cpp_code.contains("__kain_ptr_offset"),
        "Generated C++ should list ptr_offset as unsupported"
    );
    assert!(
        cpp_code.contains("__kain_mem_load"),
        "Generated C++ should list mem_load as unsupported"
    );

    // Verify forward declarations are present
    assert!(
        cpp_code.contains("template<typename T> T* __kain_bind_local(T* ptr);"),
        "Generated C++ should include forward declaration for bind_local"
    );
    assert!(
        cpp_code.contains(
            "template<typename T> T* __kain_ptr_offset(T* ptr, int64_t offset, int64_t stride);"
        ),
        "Generated C++ should include forward declaration for ptr_offset"
    );

    // Verify reference to canonical ABI
    assert!(
        cpp_code.contains("runtime/native/include/memory.h"),
        "Generated C++ should reference canonical ABI header"
    );

    // Verify status explanation
    assert!(
        cpp_code.contains("The C++ backend currently generates inline code"),
        "Generated C++ should explain why partial support exists"
    );

    // Verify supported helper implementations are present
    assert!(
        cpp_code.contains("template<typename TObject, typename TValue> TObject __kain_union_wrap"),
        "Generated C++ should include union_wrap implementation"
    );
    assert!(
        cpp_code.contains("template<typename TObject> long long __kain_bitfield_get"),
        "Generated C++ should include bitfield_get implementation"
    );
    assert!(
        cpp_code.contains("inline void* __kain_alloc(size_t size, size_t stride, int zeroed)"),
        "Generated C++ should include alloc implementation"
    );
}

#[test]
fn test_cpp_backend_helper_organization() {
    // Create a minimal typed program
    let program = TypedProgram { items: vec![] };

    // Generate C++ code
    let cpp_code = codegen_cpp::generate(&program).unwrap();

    // Verify helpers are organized by category
    let allocation_pos = cpp_code
        .find("Allocation helpers")
        .expect("Should have allocation section");
    let union_pos = cpp_code
        .find("Union operations (SUPPORTED)")
        .expect("Should have union section");
    let bitfield_pos = cpp_code
        .find("Bitfield operations (SUPPORTED)")
        .expect("Should have bitfield section");

    // Verify order: allocation < union < bitfield
    assert!(
        allocation_pos < union_pos,
        "Allocation helpers should come before union operations"
    );
    assert!(
        union_pos < bitfield_pos,
        "Union operations should come before bitfield operations"
    );
}
