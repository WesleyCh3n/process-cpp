cmake_minimum_required(VERSION 3.10)

project(process)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build tests" ON)

add_library(${CMAKE_PROJECT_NAME}
  src/unix.cpp
  src/windows.cpp
)
set(INC_HEADERS src/process.hpp)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include/${CMAKE_PROJECT_NAME})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  PUBLIC_HEADER "${INC_HEADERS}"
)

include(CMakePackageConfigHelpers)
set(CONFIG_EXPORT_NAME ${CMAKE_PROJECT_NAME}-config)
set(PROJECT_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_EXPORT_NAME}.cmake)

install(TARGETS ${CMAKE_PROJECT_NAME} EXPORT ${CONFIG_EXPORT_NAME}
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/${CMAKE_PROJECT_NAME})
export(TARGETS ${CMAKE_PROJECT_NAME}
       NAMESPACE ${CMAKE_PROJECT_NAME}::
       FILE ${PROJECT_CONFIG})

if (BUILD_EXAMPLES)
add_subdirectory(example)
endif()

if (BUILD_TESTS)
add_subdirectory(test)
endif()
