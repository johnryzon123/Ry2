cmake_minimum_required(VERSION 3.20)
project(Ry2)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Important for plugins
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(middleend/include backend/include vm/include modules/native backend/include/platform misc/include)

# Use GLOB_RECURSE (singular GLOB, plural RECURSE)
file(GLOB_RECURSE BACKEND_SOURCES "backend/src/*.cpp")
file(GLOB_RECURSE INTERP_SOURCES "vm/src/*.cpp")
file(GLOB_RECURSE NATIVE_SOURCES "modules/native/*.cpp")
file(GLOB_RECURSE MIDDLEEND_SOURCES "middleend/src/*.cpp")
file(GLOB_RECURSE MISC_SOURCES "misc/src/*.cpp")

# Add the library first
add_library(ry_core STATIC 
    ${BACKEND_SOURCES} 
    ${INTERP_SOURCES} 
    ${MIDDLEEND_SOURCES}
    ${NATIVE_SOURCES}
    ${MISC_SOURCES}
)

# NOW define the 'ry' target so set_target_properties can find it
add_executable(ry main.cpp) 

# Now you can link and set properties
target_link_libraries(ry PRIVATE ry_core)
if (UNIX)
    target_link_libraries(ry_core PUBLIC dl)
endif()

set_target_properties(ry PROPERTIES OUTPUT_NAME "ry")

# Tell the linker to export symbols from the executable
# This allows the .so to find the 'Interpreter' class inside 'ry'
set_target_properties(ry PROPERTIES ENABLE_EXPORTS ON)

# The Standard library
add_library(ry_string SHARED modules/lib_cpp/string.cpp)
add_library(ry_file SHARED modules/lib_cpp/file.cpp)
target_link_libraries(ry_string ry_core)
target_link_libraries(ry_file ry_core)