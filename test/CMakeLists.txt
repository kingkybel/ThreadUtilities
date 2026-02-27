add_executable(run_tests
        run_tests.cc
        threadutil_tests.cc
)

target_link_libraries(run_tests
        dkthreadutil
        gtest
        gtest_main
)

if(CMAKE_CXX_FLAGS MATCHES "--coverage")
    target_compile_options(run_tests PRIVATE --coverage)
    target_link_options(run_tests PRIVATE --coverage)
endif()

add_test(NAME run_tests COMMAND run_tests)
