set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Specify the cross compiler and tools
set(ARM_EXECUTABLE_VERSION_SUFFIX -12)
set(ARM_EXECUTABLE_PREFIX /usr/bin/arm-linux-gnueabihf-)

# set(CMAKE_AR                    ${ARM_EXECUTABLE_PREFIX}ar)
# set(CMAKE_ASM_COMPILER          ${ARM_EXECUTABLE_PREFIX}gcc
set(CMAKE_C_COMPILER            ${ARM_EXECUTABLE_PREFIX}gcc${ARM_EXECUTABLE_VERSION_SUFFIX})
set(CMAKE_CXX_COMPILER          ${ARM_EXECUTABLE_PREFIX}g++${ARM_EXECUTABLE_VERSION_SUFFIX})
# set(CMAKE_LINKER                ${ARM_EXECUTABLE_PREFIX}ld)
# set(CMAKE_OBJCOPY               ${ARM_EXECUTABLE_PREFIX}objcopy)
# set(CMAKE_RANLIB                ${ARM_EXECUTABLE_PREFIX}ranlib)
# set(CMAKE_SIZE                  ${ARM_EXECUTABLE_PREFIX}size)
# set(CMAKE_STRIP                 ${ARM_EXECUTABLE_PREFIX}strip)

# Where is the target environment\
set(CMAKE_SYSROOT /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH /usr/bin)

if(COBISOLV_BUILD_STATIC)
  SET(COBISOLV_EXTRA_LINK_OPTS "-static")
endif()

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH} ${COBISOLV_EXTRA_LINK_OPTS}")

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")
SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")


# Search for programs only in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers only in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
