cmake_minimum_required(VERSION 3.17.0)

if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
	set(BUILDING_64_BIT TRUE)
	message("Building 64-bit")
else()
	set(BUILDING_64_BIT FALSE)
	message("Building 32-bit")
endif()


# vars
set(SOLUTION_NAME mc2)
set(PROJECT_NAME game)
set(TEST_PROJECT_NAME test)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
file(GLOB_RECURSE shaders CONFIGURE_DEPENDS bin/shaders/*.glsl)
file(GLOB_RECURSE tests CONFIGURE_DEPENDS test/*.cpp test/*.h)

# set the project info
project(${SOLUTION_NAME}
	DESCRIPTION "Minecraft V2"
	LANGUAGES C CXX)  # Need C for GLFW?

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

# Add GLFW
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(GLFW_USE_HYBRID_HPG ON) # Prefer dedicated graphics
add_subdirectory("extern/glfw-3.3")

# include gl3w in compilation
# TODO: Create my own CMakeLists.txt for this, move to extern, do add_subdirectory
add_library(gl3w src/gl3w.c)

# include fastnoise in compilation
# TODO: Create my own CMakeLists.txt for this, move to extern, do add_subdirectory
add_library(fastnoise include/FastNoise/FastNoise.cpp)

# add source files (so they compile) and data (so we see it in IDE)
source_group(Shaders FILES ${shaders})
add_executable(${PROJECT_NAME} WIN32 ${sources} ${shaders})

set_property(TARGET ${PROJECT_NAME} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em

# link executable to libraries
target_link_libraries(${PROJECT_NAME} gl3w fastnoise glfw)

# specify include directories
include_directories(include)
include_directories(extern/glfw-3.3/include)

# hack precompile header into include
# TODO: Get this outta here!
list(GET CMAKE_CONFIGURATION_TYPES 0 FIRST_CONFIG)
include_directories("build/CMakeFiles/${SOLUTION_NAME}.dir/${FIRST_CONFIG}")

# set VS startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

# set VS working directory to same place as binary files, so that relative file reading/writing has the same effects in debug mode
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# set to C++ 17 (we doin this hardcore)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# precompile header
target_precompile_headers(${PROJECT_NAME}
	PRIVATE include/FastNoise/FastNoise.h
	PRIVATE include/GL/gl3w.h
	PRIVATE include/GL/glcorearb.h
	PRIVATE include/KHR/khrplatform.h
	PRIVATE include/stb_image.h
	PRIVATE include/vmath.h
	PRIVATE extern/glfw-3.3/include/GLFW/glfw3.h
)

if (WIN32)
	target_precompile_headers(${PROJECT_NAME}
		PRIVATE <windows.h>
	)	
endif (WIN32)
