cmake_minimum_required(VERSION 3.17.0)

# vars
set(SOLUTION_NAME mc2)
set(TARGET_NAME ${SOLUTION_NAME})

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
file(GLOB_RECURSE shaders CONFIGURE_DEPENDS bin/shaders/*.glsl)

# set the project info
project(${SOLUTION_NAME}
	DESCRIPTION "Minecraft V2"
	LANGUAGES C CXX)  # Need C for GLFW?

# set libraries dir
link_directories(${CMAKE_SOURCE_DIR}/lib)

# set CMake's build directories (archive dir = CMake artifacts dir?)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# set build directories for each of VS's build types (Debug, Release, etc.)
# TODO: Don't do this.
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# add libraries into an `sdk` solution folder
add_subdirectory(sdk)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
foreach (project IN ITEMS gl3w FastNoise glfw vmath_ stb_)
	set_target_properties(${project} PROPERTIES FOLDER sdk)
endforeach(project)

# add source files (so they compile) and data (so we see it in IDE)
# TODO: Add it as resources instead
source_group(Shaders FILES ${shaders})
add_executable(${TARGET_NAME} WIN32 ${sources} ${shaders})

# TODO: Try without this
set_property(TARGET ${TARGET_NAME} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em
foreach (project IN ITEMS gl3w FastNoise glfw vmath_ stb_)
	set_property(TARGET ${project} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em
endforeach(project)

# link executable to libraries
target_link_libraries(${TARGET_NAME} gl3w FastNoise glfw vmath stb)

# set VS startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

# set VS working directory to same place as binary files, so that relative file reading/writing has the same effects in debug mode
set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# set to C++20 (we doin this hardcore)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
