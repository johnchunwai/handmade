if(NOT WIN32)
  set(data_out_DIR ${EXECUTABLE_OUTPUT_PATH}/data)
    
  if(NOT TARGET copy_data)
    add_custom_target(copy_data ALL COMMENT "copying data files")
  endif(NOT TARGET copy_data)
  add_custom_command(TARGET copy_data
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${data_out_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${data_out_DIR}
    COMMENT "copy_data from ${CMAKE_CURRENT_SOURCE_DIR} to ${data_out_DIR}")
endif()
