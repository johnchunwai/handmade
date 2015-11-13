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

  set(handmade_SRCS sdl_handmade.cpp)
  add_executable(handmade ${handmade_SRCS})

  target_link_libraries(handmade ${SDL2_LIBRARY}
    # ${SDL2_IMAGE_LIBRARIES}
    # ${SDL2_TTF_LIBRARIES}
    )

elseif(WIN32)

  set(handmade_SRCS win32_handmade.cpp)

  if(diagnostic)
    add_executable(handmade ${handmade_SRCS})
  else()
    add_executable(handmade WIN32 ${handmade_SRCS})
  endif(diagnostic)

else()

  message(FATAL_ERROR "Unidentified platform. Exit build.")
  
endif(use_sdl)

# Output info to console for display errors, and debugging
message("CMAKE_CXX_COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID})
message("CMAKE_CXX_STANDARD: " ${CMAKE_CXX_STANDARD})
message("CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS})
message("CMAKE_COMPILER_IS_GNUCXX: " ${CMAKE_COMPILER_IS_GNUCXX})
message("CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER})
message("CMAKE_CXX_FLAGS_DEBUG: " ${CMAKE_CXX_FLAGS_DEBUG})
message("CMAKE_CXX_FLAGS_RELEASE: " ${CMAKE_CXX_FLAGS_RELEASE})
message("MAKE_CXX_FLAGS_RELWITHDEBINFO: " ${MAKE_CXX_FLAGS_RELWITHDEBINFO})


# message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
# install(TARGETS handmade RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
