# idk if it works for <3.14.7
cmake_minimum_required(VERSION 3.14.7)

# vars
set(SOLUTION_NAME mc2)
set(PROJECT_NAME game)

file(GLOB_RECURSE sources src/*.cpp src/*.h)
file(GLOB_RECURSE data src/*.glsl)

# set the project name
project(${SOLUTION_NAME})

# set libraries dir
link_directories( ${CMAKE_SOURCE_DIR}/lib )

# set CMake's build directories (archive dir = CMake artifacts dir?)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# set build directories for each of VS's build types (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# include gl3w in compilation
add_library(gl3w src/gl3w.c)

# include fastnoise in compilation
add_library(fastnoise include/FastNoise/FastNoise.cpp)

# include kenp's shitty code in compilation -- just keeping it here 'til I better understand noise and decide on an implementation
add_library(kenp_noise src/kenp_noise.h src/kenp_noise.cpp)

# add source files (so they compile) and data (so we see it in IDE)
add_executable(${PROJECT_NAME} WIN32 ${sources} ${data})

# link executable to libraries
set_property(TARGET ${PROJECT_NAME} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em
target_link_libraries(${PROJECT_NAME} gl3w optimized glfw3 debug glfw3_d fastnoise kenp_noise)

# specify include directories
include_directories(include)
include_directories(extern/glfw-3.3/include)

# set VS startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

# set to C++ 17 (we doin this hardcore)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

