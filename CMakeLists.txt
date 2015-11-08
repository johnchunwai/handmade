cmake_minimum_required(VERSION 3.3 FATAL_ERROR)

# Maps to a solution file (handmade.sln). The solution will
# have all targets (exe, lib, dll) as projects (.vcproj)
project(handmade)

set(CMAKE_VERBOSE_MAKEFILE on)

set(CMAKE_CXX_STANDARD 11)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
# message("output_path: " ${EXECUTABLE_OUTPUT_PATH})
# message("proj src dir: " ${PROJECT_SOURCE_DIR})

option(use_sdl "use_sdl" off)
message("use_sdl: " ${use_sdl})
option(diagnostic "diagnotic" off)
message("diagnostic: " ${diagnostic})
option(dev_build "dev_build" off)
message("dev_build: " ${dev_build})

if(diagnostic)
  add_definitions(-DHANDMADE_DIAGNOSTIC)
endif()
if(dev_build)
  add_definitions(-DHANDMADE_DEV_BUILD)
endif()

if(WIN32)
  if(use_sdl)
    # extra source (eg. for finding sdl2 lib)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_win32")
  endif(use_sdl)
  
  add_definitions(-D_UNICODE -DUNICODE)
  # Set the compiler flags and options.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
  if(use_sdl)
    # extra source (eg. for finding sdl2 lib)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_linux")
  endif(use_sdl)
  
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Output info to console for display errors, and debugging
message("cxx Flags: " ${CMAKE_CXX_FLAGS})

# Subdirectory where CMakeLists.txt exists
add_subdirectory(${PROJECT_SOURCE_DIR}/code)
add_subdirectory(${PROJECT_SOURCE_DIR}/data)
