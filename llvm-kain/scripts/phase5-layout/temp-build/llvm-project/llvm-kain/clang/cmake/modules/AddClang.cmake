# Minimal AddClang.cmake for llvm-kain sandbox build
# Defines add_clang_library as a wrapper around add_llvm_library
# with clang-specific defaults.

function(add_clang_library name)
  cmake_parse_arguments(ARG
    "SHARED;STATIC;MODULE;BUILDTREE_ONLY;NO_INSTALL_RPATH"
    "OUTPUT_NAME"
    "DEPENDS;LINK_COMPONENTS;LINK_LIBS;OBJLIBS"
    ${ARGN})

  # Default to add_llvm_library with clang prefix handling
  if(ARG_SHARED)
    add_llvm_library(${name} SHARED ${ARG_UNPARSED_ARGUMENTS})
  elseif(ARG_MODULE)
    add_llvm_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})
  else()
    add_llvm_library(${name} ${ARG_UNPARSED_ARGUMENTS})
  endif()

  if(ARG_DEPENDS)
    add_dependencies(${name} ${ARG_DEPENDS})
  endif()

  if(ARG_LINK_COMPONENTS)
    target_link_libraries(${name} PRIVATE ${ARG_LINK_COMPONENTS})
  endif()

  if(ARG_LINK_LIBS)
    target_link_libraries(${name} PRIVATE ${ARG_LINK_LIBS})
  endif()
endfunction()

# clang_tablegen: minimal stub for tablegen operations
macro(clang_tablegen output)
  cmake_parse_arguments(ARG "" "" "SOURCE;TARGET" ${ARGN})
  
  # In the sandbox, tablegen is not fully supported.
  # This stub creates an empty file so builds don't fail.
  set(full_path "${CMAKE_CURRENT_BINARY_DIR}/${output}")
  if(NOT EXISTS "${full_path}")
    file(WRITE "${full_path}" "// Generated stub for llvm-kain sandbox\n")
  endif()
  
  # Create a custom target with sanitized name (no slashes)
  string(REPLACE "/" "_" target_name "gen-${output}")
  string(REPLACE "." "_" target_name "${target_name}")
  if(NOT TARGET "${target_name}")
    add_custom_target("${target_name}")
  endif()
  
  if(ARG_TARGET)
    set(TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT} ${full_path} PARENT_SCOPE)
  endif()
endmacro()
