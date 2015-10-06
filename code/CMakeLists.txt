SET(handmade_SRCS win32_handmade.cpp)
# SET(SYSTEM_LIBS Xinput.lib)

add_executable(handmade WIN32 ${handmade_SRCS})

# target_link_libraries(handmade ${SYSTEM_LIBS})

# message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
install(TARGETS handmade RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
