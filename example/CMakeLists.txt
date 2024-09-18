file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(file ${files})
  get_filename_component(name ${file} NAME_WE)
  set(example_name "example_${name}")
  add_executable(
    ${example_name}
    ${file}
  )
  target_link_libraries(
    ${example_name}
    std-process
  )
endforeach()
