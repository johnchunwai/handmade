if(use_sdl)
  # sdl2 library
  find_package(SDL2 REQUIRED)
  # find_package(SDL2_ttf REQUIRED)
  # find_package(SDL2_image REQUIRED)

  message("SDL2_INCLUDE_DIR: " ${SDL2_INCLUDE_DIR})
  message("SDL2_LIBRARY: " ${SDL2_LIBRARY})


  include_directories(${SDL2_INCLUDE_DIR}
    # ${SDL2_IMAGE_INCLUDE_DIR}
    # ${SDL2_TTF_INCLUDE_DIR}
    )
endif(use_sdl)

if(WIN32)

  if (use_sdl)
    set(handmade_SRCS win32_sdl_handmade.cpp)
    add_executable(handmade ${handmade_SRCS})
  else()
    set(handmade_SRCS win32_handmade.cpp)
    add_executable(handmade WIN32 ${handmade_SRCS})
  endif(use_sdl)
  # SET(SYSTEM_LIBS dinput.lib)

else()
  
  set(handmade_SRCS linux_handmade.cpp)

  add_executable(handmade ${handmade_SRCS})

endif()

if(use_sdl)
  target_link_libraries(handmade ${SDL2_LIBRARY}
    # ${SDL2_IMAGE_LIBRARIES}
    # ${SDL2_TTF_LIBRARIES}
    )
endif(use_sdl)


# message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
# install(TARGETS handmade RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
