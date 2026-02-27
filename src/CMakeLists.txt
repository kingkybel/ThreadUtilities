add_library(dkthreadutil STATIC threadutil.cc)

if(CMAKE_CXX_FLAGS MATCHES "--coverage")
    target_compile_options(dkthreadutil PRIVATE --coverage)
    target_link_options(dkthreadutil PRIVATE --coverage)
endif()
