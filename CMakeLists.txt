cmake_minimum_required(VERSION 3.3 FATAL_ERROR)

# Maps to a solution file (handmade.sln). The solution will
# have all targets (exe, lib, dll) as projects (.vcproj)
project(handmade)

set(CMAKE_VERBOSE_MAKEFILE on)

set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_STANDARD 11)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
# message("output_path: " ${EXECUTABLE_OUTPUT_PATH})
# message("proj src dir: " ${PROJECT_SOURCE_DIR})

option(use_sdl "use_sdl" off)
message("use_sdl: " ${use_sdl})
option(diagnostic "diagnotic" off)
message("diagnostic: " ${diagnostic})
option(internal_build "internal_build" off)
message("internal_build: " ${internal_build})

if(diagnostic)
  add_definitions(-DHANDMADE_DIAGNOSTIC)
endif()
if(internal_build)
  add_definitions(-DHANDMADE_INTERNAL_BUILD)
endif()

if(WIN32)
  if(use_sdl)
    # extra source (eg. for finding sdl2 lib)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_win32")
  endif(use_sdl)
  
  add_definitions(-D_UNICODE -DUNICODE)
  # Set the compiler flags and options.
  #
  # added by cmake:
  #   /Zi pdb; show full path on warnings/errors
  #     consider /Z7 (google /Zi vs /Z7)
  #   /O2 for release (implies /Oi enable intrinsic)
  #   /EHsc - enables C++ exception (synchronous) but not async ones (structured)
  #
  # added myself:
  #   /W4 warning level 4
  #   /wd4201 ignore warning 4201 (nonstandard: nameless struct in union)
  #   /Oi enable intrinsic even for debug build so behavior won't change
  #   /GR- disable RTTI (including dynamic_cast)
  #   /MT or /MTd to statically link to c++ runtime library, so no need to
  #     include distributable
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /Oi /W4 /wd4201")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  # set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
  # set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")

  # linker flags
  # /MAP - generate map file
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP")
else()
  if(use_sdl)
    # extra source (eg. for finding sdl2 lib)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_linux")
  endif(use_sdl)
  
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Subdirectory where CMakeLists.txt exists
add_subdirectory(${PROJECT_SOURCE_DIR}/code)
add_subdirectory(${PROJECT_SOURCE_DIR}/data)
