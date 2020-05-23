cmake_minimum_required(VERSION 3.17.0)

# vars
set(SOLUTION_NAME stb)
set(PROJECT_NAME stb)

set(headers include/stb_image.h)

# set the project info
project(${SOLUTION_NAME}
	DESCRIPTION "stb"
	LANGUAGES C CXX)

# add source files (so they compile) and data (so we see it in IDE)
add_library(${PROJECT_NAME} INTERFACE)

# specify include directories
target_include_directories(${PROJECT_NAME} INTERFACE include)
