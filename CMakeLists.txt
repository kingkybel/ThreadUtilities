cmake_minimum_required(VERSION 3.29)
project(ThreadUtilities
        VERSION 2.0.0
        DESCRIPTION "A library for thread utilities"
        HOMEPAGE_URL "https://github.com/kingkybel/ThreadUtilities")

include(GNUInstallDirs)
include(CTest)
enable_testing()

include(cmake-common/cmake/DkybBuildSettings.cmake)
dkyb_apply_common_settings(CXX_STANDARD 23)

execute_process(
        COMMAND bash -c "printf \"%s\" $(cat /etc/os-release | grep -E \"^ID=[^\\n]*|^VERSION_ID=[^\\n]*\" | cut -f2 -d= | tr -d '\"')"
        OUTPUT_VARIABLE LINUX_VERSION
)
message("Linux version is '${LINUX_VERSION}'")

set(CMAKE_FIND_LIBRARY_PREFIXES lib)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

include_directories(
        BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/include
        system
)
link_directories(
        /usr/local/lib
        system
)

add_library(DirectedGraphs INTERFACE)
# Adding the install interface generator expression makes sure that the include
# files are installed to the proper location (provided by GNUInstallDirs)
target_include_directories(
        DirectedGraphs
        INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(DirectedGraphs INTERFACE cxx_std_23)

add_subdirectory(src)
add_subdirectory(test)

find_program(GCOVR_EXECUTABLE gcovr)
if (GCOVR_EXECUTABLE)
    add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${GCOVR_EXECUTABLE}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude ${CMAKE_SOURCE_DIR}/test
                    --exclude ${CMAKE_SOURCE_DIR}/build
                    --txt
            COMMAND ${GCOVR_EXECUTABLE}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude ${CMAKE_SOURCE_DIR}/test
                    --exclude ${CMAKE_SOURCE_DIR}/build
                    --xml-pretty
                    --output ${CMAKE_BINARY_DIR}/coverage/coverage.xml
            COMMAND ${GCOVR_EXECUTABLE}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude ${CMAKE_SOURCE_DIR}/test
                    --exclude ${CMAKE_SOURCE_DIR}/build
                    --html-details
                    --output ${CMAKE_BINARY_DIR}/coverage/coverage.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run tests and generate coverage reports (text/xml/html) with gcovr"
            VERBATIM
    )
else ()
    message(STATUS "gcovr not found - coverage target will not be available")
endif ()

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=* -p ${CMAKE_SOURCE_DIR}/build")

file(GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/*.h)

# Install the header files directly under 'include/dkyb'
install(FILES ${HEADER_FILES}
        DESTINATION include/dkyb)
