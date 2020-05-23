cmake_minimum_required(VERSION 3.17.0)

# set the project info
project(SDK
	DESCRIPTION "SDK"
	LANGUAGES C CXX)  # Need C for GLFW?

# Add GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW docs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "GLFW installation project" FORCE)
set(GLFW_USE_HYBRID_HPG ON CACHE BOOL "GLFW prefer dedicated graphics card" FORCE)
add_subdirectory("glfw-3.3")

# Add gl3w
add_subdirectory("gl3w")

# Add fastnoise
add_subdirectory("FastNoise")

# Add stb
add_subdirectory("stb")

# Add vmath
add_subdirectory("vmath")
