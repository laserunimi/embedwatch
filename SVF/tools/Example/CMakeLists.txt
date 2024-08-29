
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool( svf-ex svf-ex.cpp test.cpp test.h )
else()
    add_executable( svf-ex svf-ex.cpp svf-ex.h visits.cpp visits.h trace.cpp trace.h)

    target_link_libraries( svf-ex Svf ${llvm_libs} )

    set_target_properties( svf-ex PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
