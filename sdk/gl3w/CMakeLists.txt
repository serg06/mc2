cmake_minimum_required(VERSION 3.17.0)

# vars
set(SOLUTION_NAME gl3w)
set(PROJECT_NAME gl3w)

set(sources gl3w.c)
set(headers include/GL/gl3w.h include/GL/glcorearb.h include/KHR/khrplatform.h)

# set the project info
# TODO: Language
project(${SOLUTION_NAME}
	DESCRIPTION "gl3w"
	LANGUAGES C CXX)

# add source files (so they compile) and data (so we see it in IDE)
add_library(${PROJECT_NAME} STATIC ${sources} ${headers})

# specify include directories
target_include_directories(${PROJECT_NAME} PUBLIC include)

# set to C++ 17 (we doin this hardcore)
# TODO: Or not, if not required
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
