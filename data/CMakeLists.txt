if(use_sdl)
  set(data_out_DIR ${EXECUTABLE_OUTPUT_PATH}/data)
  set(data_FILES "sdl_gamecontroller_db/gamecontrollerdb.txt;sdl_gamecontroller_db/README.md")
  
  if(NOT TARGET copy_data)
    add_custom_target(copy_data ALL COMMENT "copying data files")
  endif(NOT TARGET copy_data)

  add_custom_command(TARGET copy_data
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${data_out_DIR})

  foreach(data_file ${data_FILES})
    add_custom_command(TARGET copy_data
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E
      copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${data_file} ${data_out_DIR}/${data_file})
  endforeach()
endif()
