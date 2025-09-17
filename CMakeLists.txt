cmake_minimum_required(VERSION 3.20)

# ---- Project ----

project(sw_renderer)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

# ---- Create standalone executable ----

# add your source files here
add_executable(${PROJECT_NAME} ${sources} ${headers})
# add your public include directories here
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# setup your target's properties
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
