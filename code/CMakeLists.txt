if(WIN32)
  SET(handmade_SRCS win32_handmade.cpp)
  # SET(SYSTEM_LIBS dinput.lib)

  add_executable(handmade WIN32 ${handmade_SRCS})
else()
  # sdl2 library
  find_package(SDL2 REQUIRED)
  # find_package(SDL2_ttf REQUIRED)
  # find_package(SDL2_image REQUIRED)

  include_directories(${SDL2_INCLUDE_DIR}
    # ${SDL2_IMAGE_INCLUDE_DIR}
    # ${SDL2_TTF_INCLUDE_DIR}
    )
  
  SET(handmade_SRCS linux_handmade.cpp)

  add_executable(handmade ${handmade_SRCS})

  target_link_libraries(handmade ${SDL2_LIBRARY}
    # ${SDL2_IMAGE_LIBRARIES}
    # ${SDL2_TTF_LIBRARIES}
    )
endif()

# message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
# install(TARGETS handmade RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
