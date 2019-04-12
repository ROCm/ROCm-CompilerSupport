cmake_minimum_required(VERSION 3.0)
project(Hostcall)

if (NOT UNIX)
    message(FATAL_ERROR "No Windows support due to lack of signals")
endif()

find_path(HSA_HEADER hsa/hsa.h PATHS /opt/rocm/include)
if (NOT EXISTS ${HSA_HEADER})
    message(FATAL_ERROR "Cannot find HSA headers. Please check the CMAKE_PREFIX_PATH")
endif()
message(STATUS "Using HSA headers from ${HSA_HEADER}")
include_directories(${HSA_HEADER})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

file(GLOB SOURCES "src/*.cpp")
add_library(amd_hostcall SHARED ${SOURCES})
set_target_properties(amd_hostcall PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED Yes
    CXX_EXTENSIONS No)

set(AMD_HOSTCALL_PRIVATE_COMPILE_OPTIONS)
set(AMD_HOSTCALL_PRIVATE_COMPILE_DEFINITIONS)

list(APPEND AMD_HOSTCALL_PRIVATE_COMPILE_OPTIONS
    -fvisibility=hidden -fno-exceptions -fno-rtti -Wall -Werror)
list(APPEND AMD_HOSTCALL_PRIVATE_COMPILE_DEFINITIONS AMD_HOSTCALL_EXPORT)

target_compile_options(amd_hostcall
  PRIVATE "${AMD_HOSTCALL_PRIVATE_COMPILE_OPTIONS}")
target_compile_definitions(amd_hostcall
  PRIVATE "${AMD_HOSTCALL_PRIVATE_COMPILE_DEFINITIONS}")

set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(SHARE_INSTALL_DIR share)
set(CONFIG_INSTALL_DIR ${LIB_INSTALL_DIR}/cmake/amd_hostcall)

install(TARGETS amd_hostcall
    EXPORT amd_hostcall_targets
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR})

export(EXPORT amd_hostcall_targets)

install(EXPORT amd_hostcall_targets
    DESTINATION ${CONFIG_INSTALL_DIR})

install(FILES
    "include/amd_hostcall.h"
    DESTINATION ${INCLUDE_INSTALL_DIR})

install(FILES
    "README.md"
    "LICENSE.txt"
    DESTINATION ${SHARE_INSTALL_DIR}/amd_hostcall)

set(AMD_HOSTCALL_VERSION_MAJOR "1")
set(AMD_HOSTCALL_VERSION_MINOR "0")
set(AMD_HOSTCALL_VERSION_STRING "${AMD_HOSTCALL_VERSION_MAJOR}.${AMD_HOSTCALL_VERSION_MINOR}")

include(CMakePackageConfigHelpers)

configure_package_config_file(amd_hostcall-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/amd_hostcall-config.cmake
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
    PATH_VARS LIB_INSTALL_DIR INCLUDE_INSTALL_DIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/amd_hostcall-config-version.cmake
    VERSION "${AMD_HOSTCALL_VERSION_MAJOR}.${AMD_HOSTCALL_VERSION_MINOR}"
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/amd_hostcall-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/amd_hostcall-config-version.cmake
    DESTINATION
    ${CONFIG_INSTALL_DIR})

enable_testing()
add_custom_target(build-tests)
add_dependencies(build-tests amd_hostcall)
add_subdirectory(tests)

set(CPACK_GENERATOR "DEB;RPM")

# Add packaging directives for amd_hostcall
set(CPACK_PACKAGE_NAME hostcall)
set(CPACK_PACKAGE_VENDOR "AMD")
set(CPACK_PACKAGE_VERSION_MAJOR ${AMD_HOSTCALL_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${AMD_HOSTCALL_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Library to provide support functions")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

# Debian package specific variables
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
  "https://github.com/RadeonOpenCompute/ROCm-CompilerSupport")

# RPM package specific variables
if(DEFINED CPACK_PACKAGING_INSTALL_PREFIX)
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "${CPACK_PACKAGING_INSTALL_PREFIX}")
endif()

if (NOT CPack_CMake_INCLUDED)
  include(CPack)
endif()
