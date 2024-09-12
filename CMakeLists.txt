cmake_minimum_required(VERSION 3.10)

project(std-process)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

add_library(std-process
  src/unix.cpp
  src/windows.cpp
)
set(INC_HEADERS src/process.hpp)

target_include_directories(std-process PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include/std-process)
set_target_properties(std-process PROPERTIES
  PUBLIC_HEADER "${INC_HEADERS}"
)

include(CMakePackageConfigHelpers)
set(CONFIG_EXPORT_NAME std-process-config)
set(PROJECT_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_EXPORT_NAME}.cmake)

install(TARGETS std-process EXPORT ${CONFIG_EXPORT_NAME}
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/std-process)
export(TARGETS std-process
       NAMESPACE std-process::
       FILE ${PROJECT_CONFIG})

add_executable(main
  main.cpp
)
target_link_libraries(main
  std-process
)

add_subdirectory(test)
