cmake_minimum_required(VERSION 3.16)

# Native RPC Configuration
project(nativerpc)
set(NATIVERPC_VERSION 1.1.2)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install")
set(NLOHMANN_INCL "${CMAKE_CURRENT_SOURCE_DIR}/src/nlohmann")
include(CMakePackageConfigHelpers)
set(PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Native RPC Library
add_library(
  nativerpc
  SHARED
  ${PROJECT_DIR}/src/nativerpc/main.cpp
  ${PROJECT_DIR}/src/nativerpc/common.cpp
  ${PROJECT_DIR}/src/nativerpc/extension.cpp
)
target_compile_definitions(nativerpc PRIVATE NATIVERPC_EXPORTS)
target_include_directories(
  nativerpc
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_DIR}/src>
)
set_target_properties(
  nativerpc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
  OUTPUT_NAME "nativerpc-lib"
)
target_link_libraries(nativerpc PUBLIC ws2_32)
add_custom_command(
  TARGET nativerpc
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E env CLICOLOR_FORCE=1 
  ${CMAKE_COMMAND} -E cmake_echo_color --no-newline "Status: " --green "Success"
)

# Native RPC CLI
add_executable(
  cli
  ${PROJECT_DIR}/src/nativerpc/cli.cpp
)
target_include_directories(
  cli
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_DIR}/src>
)
set_target_properties(
  cli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
  OUTPUT_NAME "nativerpc"
)
target_link_libraries(cli PUBLIC nativerpc ws2_32)
add_custom_command(
  TARGET cli
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E env CLICOLOR_FORCE=1 
  ${CMAKE_COMMAND} -E cmake_echo_color --no-newline "Status: " --green "Success"
)

# Native RPC Install
configure_package_config_file(
  ${PROJECT_DIR}/src/nativerpc/main.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/nativerpcConfig.cmake 
  INSTALL_DESTINATION cmake
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/nativerpcConfigVersion.cmake
  VERSION ${NATIVERPC_VERSION}
  COMPATIBILITY SameMajorVersion 
)
install(
  TARGETS nativerpc cli
  EXPORT nativerpcTargets 
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/nativerpc
)
install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/nativerpcConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/nativerpcConfigVersion.cmake
  DESTINATION cmake
)
install(
  FILES
  ${PROJECT_DIR}/src/nativerpc/extension.h
  ${PROJECT_DIR}/src/nativerpc/common.h
  ${PROJECT_DIR}/src/nativerpc/main.h
  DESTINATION include/nativerpc
)
install(
  FILES
  ${PROJECT_DIR}/src/nlohmann/json.hpp
  ${PROJECT_DIR}/src/nlohmann/json_fwd.hpp
  DESTINATION include/nlohmann
)
install(
  EXPORT nativerpcTargets
  DESTINATION cmake
  NAMESPACE nativerpc::
)
install(
    CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E env CLICOLOR_FORCE=1 \"${CMAKE_COMMAND}\" -E cmake_echo_color --no-newline \"Status: \" --green \"Success\")"
)

# Native RPC Info
add_custom_target(
    info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Settings:"
    COMMAND ${CMAKE_COMMAND} -E echo "    Build Mode: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "    C++ Standard: ${CMAKE_CXX_STANDARD}"
    COMMAND ${CMAKE_COMMAND} -E echo "    C++ Standard Required: ${CMAKE_CXX_STANDARD_REQUIRED}"
    COMMAND ${CMAKE_COMMAND} -E echo "    C++ Extensions: ${CMAKE_CXX_EXTENSIONS}"
    COMMAND ${CMAKE_COMMAND} -E echo "    External Dependencies: nlohmann::nlohmann"
    COMMAND ${CMAKE_COMMAND} -E echo "    External Includes: ${NLOHMANN_INCL}"
    COMMAND ${CMAKE_COMMAND} -E echo "    Build Tool: ${CMAKE_BUILD_TOOL}"
    COMMAND ${CMAKE_COMMAND} -E echo "    Lib Tool: ${CMAKE_AR}"
    COMMAND ${CMAKE_COMMAND} -E echo "    Source Dir: ${PROJECT_SOURCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "    Build Dir: ${CMAKE_BINARY_DIR}"
    VERBATIM
)
