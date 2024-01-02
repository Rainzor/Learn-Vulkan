cmake_minimum_required (VERSION 3.8)

# Setting
project ("LearnVK")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package (tinyobjloader REQUIRED)
find_package (PkgConfig)
pkg_get_variable (STB_INCLUDEDIR stb includedir)
if (NOT STB_INCLUDEDIR)
  unset (STB_INCLUDEDIR)
  find_path (STB_INCLUDEDIR stb_image.h PATH_SUFFIXES stb)
endif ()
if (NOT STB_INCLUDEDIR)
  message (FATAL_ERROR "stb_image.h not found")
endif ()

add_subdirectory(src)
