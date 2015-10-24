if(WIN32)
  SET(handmade_SRCS win32_handmade.cpp)
  # SET(SYSTEM_LIBS dinput.lib)

  add_executable(handmade WIN32 ${handmade_SRCS})
else()
  SET(handmade_SRCS linux_handmade.cpp)
  SET(SYSTEM_LIBS libSDL2.a pthread dl)

  add_executable(handmade ${handmade_SRCS})
  target_link_libraries(handmade ${SYSTEM_LIBS})
endif()

# message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
install(TARGETS handmade RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
