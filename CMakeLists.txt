cmake_minimum_required(VERSION 2.8)

option(COBISOLV_CROSS_COMPILE "Cross compile for Raspberry Pi." OFF)
option(COBISOLV_BUILD_STATIC "Build static executable." ON)
option(COBISOLV_BUILD_CMD "Build compiled command line interface." ON)
option(COBISOLV_BUILD_TESTS "Build c library unit tests." OFF)

if(COBISOLV_CROSS_COMPILE)
  set(CMAKE_TOOLCHAIN_FILE ./Toolchain-RasberryPi.cmake)
endif()

project(cobisolv)

# Set compiler flags for gcc and clang
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
endif(MSVC)

if(CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set(CMAKE_C_FLAGS         "${CMAKE_C_FLAGS} -Wall -Wextra -std=gnu99 -fopenmp")
    set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=gnu++11 -fopenmp")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

# location of header files
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/cmd)

# Object Library
add_library(solver OBJECT src/solver.cc src/util.cc src/cobi.cc)

# static library
add_library(libcobisolv STATIC $<TARGET_OBJECTS:solver> src/util.cc src/cobi.cc)
set_target_properties(libcobisolv PROPERTIES PREFIX "")

if(COBISOLV_BUILD_CMD)
    # compile main executable
    add_executable(cobisolv cmd/main.c cmd/readqubo.c)

    # link library
    target_link_libraries(cobisolv libcobisolv)
endif()

if(COBISOLV_BUILD_TESTS)
    # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                     ${CMAKE_BINARY_DIR}/googletest-build
                     EXCLUDE_FROM_ALL)

    # The gtest/gtest_main targets carry header search path
    # dependencies automatically when using CMake 2.8.11 or
    # later. Otherwise we have to add them here ourselves.
    if (CMAKE_VERSION VERSION_LESS 2.8.11)
        include_directories("${gtest_SOURCE_DIR}/include")
    endif()

    enable_testing()
    add_subdirectory(tests)
endif()
