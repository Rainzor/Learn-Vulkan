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
    add_executable (${SRC_NAME} ${SRC_NAME}.cpp)
    set_target_properties (${SRC_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${SRC_NAME})
    set_target_properties (${SRC_NAME} PROPERTIES CXX_STANDARD 17)
    target_link_libraries (${SRC_NAME} Vulkan::Vulkan glfw)
    target_include_directories (${SRC_NAME} PRIVATE ${STB_INCLUDEDIR})
    target_include_directories (${SRC_NAME} PRIVATE ${VULKAN_INCLUDE_DIR})

    if (DEFINED SRC_SHADER)
        set (SRC_SHADER_TARGET ${SRC_NAME}_shader)
        file (GLOB SHADER_SOURCES ${SRC_SHADER}.frag ${SRC_SHADER}.vert ${SRC_SHADER}.comp)
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