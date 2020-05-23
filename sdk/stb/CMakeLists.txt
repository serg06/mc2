cmake_minimum_required(VERSION 3.17.0)

# vars
set(LIB_NAME stb)
set(headers include/stb_image.h)

# set the project info
project(${LIB_NAME}
	DESCRIPTION "${LIB_NAME}"
	LANGUAGES C CXX)

# add source files (so they compile) and data (so we see it in IDE)
add_library(${LIB_NAME} INTERFACE)

# hack to show header-only project in VS
add_custom_target(${LIB_NAME}_ SOURCES ${headers})

# specify include directories
target_include_directories(${LIB_NAME} INTERFACE include)
