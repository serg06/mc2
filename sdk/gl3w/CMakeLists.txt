cmake_minimum_required(VERSION 3.17.0)

# vars
set(LIB_NAME gl3w)
set(sources gl3w.c)
set(headers include/GL/gl3w.h include/GL/glcorearb.h include/KHR/khrplatform.h)

# set the project info
# TODO: Language
project(${LIB_NAME}
	DESCRIPTION "${LIB_NAME}"
	LANGUAGES C CXX)

# add source files (so they compile) and data (so we see it in IDE)
add_library(${LIB_NAME} STATIC ${sources} ${headers})

# specify include directories
target_include_directories(${LIB_NAME} PUBLIC include)
