
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool( svf-uaf svf-uaf.cpp test.cpp test.h )
else()
    add_executable( svf-uaf svf-uaf.cpp svf-uaf.h visits.cpp visits.h)

    target_link_libraries( svf-uaf Svf ${llvm_libs} )

    set_target_properties( svf-uaf PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
