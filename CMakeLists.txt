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
  #   /EHsc - enable C++ exception which is fine but not SEH which sucks
  #   
  # added myself:
  #   /W4 warning level 4
  #   /wd4201 ignore warning 4201 (nonstandard: nameless struct in union)
  #   /Oi enable intrinsic even for debug build so behavior won't change
  #   /GR- disable RTTI (including dynamic_cast)
  #   /MT or /MTd to statically link to c++ runtime library, so no need to
  #     include distributable
  #   /SUBSYSTEM:{CONSOLE|WINDOWS|...}[,major[.minor]] - CONSOLE to force cmd
  #     so we have stdio and stderr (call winmain from main); major,minor
  #     helps us target certain subsystems (eg. 5.00 (x86), 5.02 (x64), 1.0
  #     boot app))
  #   /WX treat warnings as errors
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /Oi /W4 /wd4201")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
  # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  # set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
  # set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")

  # linker flags
  # /MAP - generate map file
  # /MACHINE:{ARM|EBC|IA64|MIPS|MIPS16|MIPSFPU|MIPSFPU16|SH4|THUMB|X64|X86} -
  #   optional, specify target platform; usually that comes from obj file (see
  #   /SUBSYSTEM compiler option)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP")
else()
  if(use_sdl)
    # extra source (eg. for finding sdl2 lib)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_linux")
  endif(use_sdl)

  # Set the compiler flags and options.
  #
  # added by cmake:
  #   c++ exception handling is enabled by default, no need to enable
  #   
  # added myself:
  #   -Werror treat warnings as errors
  #   -Wno-<warning> disable <warning>
  #   -Wno-error=<err> keep warning for <err> while not treat as error
  #   -Weverything - really enable all warnings
  #   -fno-rtti - disable rtti
  #   -m32 - cross compile 32bit version (apt-get install gcc-multilib
  #     g++-multilib libsdl2<version>:i386)
  #   -v - verbose
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -v -fno-rtti -Weverything -Werror \
-Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c++98-compat \
-Wno-c++98-compat-pedantic \
-Wno-error=unused-variable -Wno-error=undef \
-Wno-error=padded -Wno-error=documentation")

  if(internal_build)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")
  endif(internal_build)
endif()

# Subdirectory where CMakeLists.txt exists
add_subdirectory(${PROJECT_SOURCE_DIR}/code)
add_subdirectory(${PROJECT_SOURCE_DIR}/data)
