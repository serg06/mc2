cmake_minimum_required(VERSION 3.17.0)

# vars
set(LIB_NAME imgui)
set(sources
	# Main part
	imgui.cpp
	imgui_draw.cpp
	imgui_widgets.cpp

	# Example backend implementation
	examples/imgui_impl_opengl3.cpp
	examples/imgui_impl_glfw.cpp

	# Example code for reference
	imgui_demo.cpp
)
set(headers
	# Main part
	imgui.h

	# Example backend implementation
	examples/imgui_impl_opengl3.h
	examples/imgui_impl_glfw.h
)
set(natvis misc/natvis/imgui.natvis)

# set the project info
project(${LIB_NAME}
	DESCRIPTION "${LIB_NAME}"
	LANGUAGES C CXX)

# add source files (so they compile) and data (so we see it in IDE)
add_library(${LIB_NAME} STATIC ${sources} ${headers} ${natvis})

# specify include directories
target_include_directories(${LIB_NAME} PUBLIC .)

# set C++17 (Note: Might work with earlier versions, not sure)
set_property(TARGET ${LIB_NAME} PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
