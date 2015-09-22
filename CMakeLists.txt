cmake_minimum_required(VERSION 3.3)

# Maps to a solution file (handmade.sln). The solution will
# have all targets (exe, lib, dll) as projects (.vcproj)
project(handmade)

add_definitions(-D_UNICODE -DUNICODE)

# Set the compiler flags and options.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")

# Output info to console for display errors, and debugging
message("cxx Flags: " ${CMAKE_CXX_FLAGS})

# Subdirectory where CMakeLists.txt exists
add_subdirectory(code)