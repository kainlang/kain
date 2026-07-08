# Minimal AddLLD.cmake for llvm-kain sandbox build
# Defines add_lld_library as a wrapper around add_llvm_library
# with LLD-specific defaults.

function(add_lld_library name)
  cmake_parse_arguments(ARG
    "SHARED;STATIC;MODULE;BUILDTREE_ONLY"
    "OUTPUT_NAME"
    "DEPENDS;LINK_COMPONENTS;LINK_LIBS"
    ${ARGN})

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
endfunction()
