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

# Shader
add_executable (glslang::validator IMPORTED)
find_program (GLSLANG_VALIDATOR "glslangValidator" HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
set_property (TARGET glslang::validator PROPERTY IMPORTED_LOCATION "${GLSLANG_VALIDATOR}")

function (add_shaders_target TARGET)
  cmake_parse_arguments ("SHADER" "" "SRC_NAME" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${SHADER_SRC_NAME}/shaders)
  add_custom_command (
    OUTPUT ${SHADERS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
  set (SHADERS ${SHADERS_DIR}/frag.spv ${SHADERS_DIR}/vert.spv)
  # Some chapters may have compute shaders in addition to vertex and fragment shaders,
  # so we conditionally check this and add them to the target
  string(FIND "${SHADER_SOURCES}" "${CHAPTER_SHADER}.comp" COMPUTE_SHADER_INDEX)
  if (${COMPUTE_SHADER_INDEX} GREATER -1)  
    set (SHADERS ${SHADERS} ${SHADERS_DIR}/comp.spv)
  endif()
  add_custom_command (
    OUTPUT ${SHADERS}
    COMMAND glslang::validator
    ARGS --target-env vulkan1.0 ${SHADER_SOURCES} --quiet
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "Compiling Shaders"
    VERBATIM
    )
  add_custom_target (${TARGET} DEPENDS ${SHADERS})
endfunction ()

function (add_src SRC_NAME)
    cmake_parse_arguments (SRC "" "SHADER" "LIBS;TEXTURES;MODELS" ${ARGN})
    add_executable (${SRC_NAME} src/${SRC_NAME}.cpp)
    set_target_properties (${SRC_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${SRC_NAME})
    set_target_properties (${SRC_NAME} PROPERTIES CXX_STANDARD 17)
    target_link_libraries (${SRC_NAME} Vulkan::Vulkan glfw)
    target_include_directories (${SRC_NAME} PRIVATE ${STB_INCLUDEDIR})
    target_include_directories (${SRC_NAME} PRIVATE ${VULKAN_INCLUDE_DIR})

    if (DEFINED SRC_SHADER)
        set (SRC_SHADER_TARGET ${SRC_NAME}_shader)
        file (GLOB SHADER_SOURCES shaders/${SRC_SHADER}.frag shaders/${SRC_SHADER}.vert shaders/${SRC_SHADER}.comp)
        add_shaders_target (${SRC_SHADER_TARGET} SRC_NAME ${SRC_NAME} SOURCES ${SHADER_SOURCES})
        add_dependencies (${SRC_NAME} ${SRC_SHADER_TARGET})
    endif ()

    if (DEFINED SRC_LIBS)
        target_link_libraries (${SRC_NAME} ${SRC_LIBS})
    endif ()

    if (DEFINED SRC_MODELS)
        file (COPY ${SRC_MODELS} DESTINATION ${CMAKE_BINARY_DIR}/${SRC_NAME}/models)
    endif ()

    if (DEFINED SRC_TEXTURES)
        file (COPY ${SRC_TEXTURES} DESTINATION ${CMAKE_BINARY_DIR}/${SRC_NAME}/textures)
    endif ()

endfunction ()

add_src(00_base_code)
add_src(01_instance_creation)
add_src(02_validation_layers)
add_src(03_physical_device_selection)
add_src(04_logical_device)
add_src(05_window_surface)
add_src(06_swap_chain_creation)
add_src(07_image_views)
# add_src(08_graphics_pipeline)
add_src(09_shader_modules SHADER 09_shader_base)
add_src(10_fixed_functions SHADER 09_shader_base)
add_src(11_render_passes SHADER 09_shader_base)
add_src(12_graphics_pipeline_complete SHADER 09_shader_base)
add_src(13_framebuffers SHADER 09_shader_base)
add_src(14_command_buffers SHADER 09_shader_base)
add_src(15_hello_triangle SHADER 09_shader_base)
add_src(16_frames_in_flight SHADER 09_shader_base)
add_src(17_swap_chain_recreation SHADER 09_shader_base)

add_src(18_vertex_buffers SHADER 18_shader_vertexbuffer LIBS glm::glm)

add_src(19_staging_buffer SHADER 18_shader_vertexbuffer LIBS glm::glm)

add_src(20_index_buffer SHADER 18_shader_vertexbuffer LIBS glm::glm)

add_src(20_index_buffer_plus SHADER 18_shader_vertexbuffer LIBS glm::glm)

add_src(21_descriptor_sets SHADER 21_shader_ubo LIBS glm::glm)

add_src(22_texture_mapping 
        SHADER 22_shader_textures 
        TEXTURES images/texture.jpg 
        LIBS glm::glm)

add_src(23_depth_buffering 
        SHADER 23_shader_depth 
        TEXTURES images/texture.jpg 
        LIBS glm::glm)

add_src(24_model_loading
        SHADER 23_shader_depth
        MODELS resources/viking_room.obj
        TEXTURES resources/viking_room.png
        LIBS glm::glm tinyobjloader::tinyobjloader)

add_src(25_mipmapping
        SHADER 23_shader_depth
        MODELS resources/viking_room.obj
        TEXTURES resources/viking_room.png
        LIBS glm::glm tinyobjloader::tinyobjloader)

add_src(26_multisampling
        SHADER 23_shader_depth
        MODELS resources/viking_room.obj
        TEXTURES resources/viking_room.png
        LIBS glm::glm tinyobjloader::tinyobjloader)

add_src(27_compute_shader
      SHADER 27_shader_compute
      LIBS glm::glm)