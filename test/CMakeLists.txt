include(FetchContent)
FetchContent_Declare(
  googletest
  Boost
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(file ${files})
  # Get the filename without extension
  get_filename_component(name ${file} NAME_WE)
  set(test_name "test_${name}")
  add_executable(
    ${test_name}
    ${file}
  )
  target_link_libraries(
    ${test_name}
    GTest::gtest_main
    std-process
  )
  gtest_discover_tests(${test_name})
endforeach()

