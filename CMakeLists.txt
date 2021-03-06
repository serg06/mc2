cmake_minimum_required(VERSION 3.17.0)

# vars
set(SOLUTION_NAME mc2)
set(TARGET_NAME ${SOLUTION_NAME})

set(HEADER_ONLY_LIBS vmath stb cppzmq)
set(OTHER_LIBS gl3w FastNoise glfw libzmq-static imgui)

# hyper-optimize Release build (so should stick to RelWithDebInfo when debugging)
set(MSVC_COMPILER_FLAGS_RELEASE /MT /Oi /Ot /GL) # static linking, speed
set(MSVC_LINKER_FLAGS_RELEASE /LTCG) # speed
set(MSVC_COMPILER_FLAGS
	/Zc:__cplusplus  # temporary workaround since zmq.hpp doesn't include `vcruntime.h` before checking _HAS_CXX17
	/MP)

set(ALL_LIBS ${HEADER_ONLY_LIBS} ${OTHER_LIBS})

set(LIB_TARGETS ${HEADER_ONLY_LIBS})
list(TRANSFORM LIB_TARGETS APPEND _)
list(APPEND LIB_TARGETS ${OTHER_LIBS})

set(ALL_TARGETS ${TARGET_NAME} ${LIB_TARGETS})

# set the project info
project(${SOLUTION_NAME}
	DESCRIPTION "Minecraft V2"
	LANGUAGES C CXX)

# set compiler/linker flags
if (MSVC)
	foreach (FLAG IN LISTS MSVC_COMPILER_FLAGS_RELEASE)
		add_compile_options("$<$<CONFIG:Release>:${FLAG}>")
	endforeach(FLAG)
	foreach (FLAG IN LISTS MSVC_COMPILER_FLAGS)
		add_compile_options("${FLAG}")
	endforeach(FLAG)
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${MSVC_LINKER_FLAGS_RELEASE}")
endif()

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
foreach (TARGET IN LISTS LIB_TARGETS)
	set_target_properties(${TARGET} PROPERTIES FOLDER sdk)
endforeach(TARGET)

# Find source files
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS src/*.h src/*.hpp)
file(GLOB_RECURSE shaders CONFIGURE_DEPENDS bin/shaders/*.glsl)

# add source files (so they compile) and data (so we see it in IDE)
# TODO: Add it as resources instead
source_group(Shaders FILES ${shaders})
add_executable(${TARGET_NAME} WIN32 ${sources} ${headers} ${shaders})

# TODO: Try without this
set_property(TARGET ${TARGET_NAME} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em
foreach (TARGET IN LISTS LIB_TARGETS)
	set_property(TARGET ${TARGET} PROPERTY DEBUG_POSTFIX _d) # _dab on 'em
endforeach(TARGET)

# link executable to libraries
target_link_libraries(${TARGET_NAME} ${ALL_LIBS})

# set VS startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

# set VS working directory to same place as binary files, so that relative file reading/writing has the same effects in debug mode
set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# specify include directories
target_include_directories(${TARGET_NAME} PUBLIC src)

# set to C++20 (we doin this hardcore)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
