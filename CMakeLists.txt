cmake_minimum_required(VERSION 3.14...3.25)
project(
    Vulkan_Engine
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json"
      "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)

# Fetch Dependencies
include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# 3. VulkanMemoryAllocator
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# 4. Assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# 5. stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_MakeAvailable(stb)

# Vulkan System
find_package(Vulkan REQUIRED)

# Sources
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS "src/engine/*.cpp" "src/engine/*.h")

# Automatic Shader Compilation
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS "shaders/src/*.vert" "shaders/src/*.frag")
set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/built")
file(MAKE_DIRECTORY ${SPV_OUTPUT_DIR})

set(COMPILED_SHADERS "")
foreach(SHADER IN LISTS SHADER_SOURCES)
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/shaders/src" ${SHADER})
    get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)
    get_filename_component(FILE_NAME_WE ${REL_PATH} NAME_WE)
    get_filename_component(EXT ${REL_PATH} EXT)
    string(SUBSTRING ${EXT} 1 -1 STAGE)

    set(TARGET_DIR "${SPV_OUTPUT_DIR}/${REL_DIR}")
    file(MAKE_DIRECTORY ${TARGET_DIR})
    set(SPV_FILE "${TARGET_DIR}/${FILE_NAME_WE}_${STAGE}.spv")

    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TARGET_DIR}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${REL_PATH} -> ${FILE_NAME_WE}_${STAGE}.spv"
    )
    list(APPEND COMPILED_SHADERS ${SPV_FILE})
endforeach()

add_custom_target(compile_shaders DEPENDS ${COMPILED_SHADERS})

# Main executable
add_executable(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    ${COMMON_SOURCES}
)

# Ensure shaders are compiled before the main target
add_dependencies(${PROJECT_NAME} compile_shaders)

# Warning flags
include(utils.cmake)
target_treat_all_warnings_as_errors(${PROJECT_NAME})

# Suppress warnings from VMA headers (header-only lib included in our TUs)
get_target_property(VMA_INCLUDE_DIRS GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${VMA_INCLUDE_DIRS})
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-nullability-extension)
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/"
    "${stb_SOURCE_DIR}"
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Absolute paths for assets (avoids Xcode vs Makefiles issues during development)
    TEXTURE_PATH="${CMAKE_SOURCE_DIR}/textures/"
    SHADER_PATH="${SPV_OUTPUT_DIR}/"
    MODEL_PATH="${CMAKE_SOURCE_DIR}/models/"
    SOUND_PATH="${CMAKE_SOURCE_DIR}/sounds/"
    # Engine constants
    SPOT_POINT_ATLAS_SIZE=4096
    DIRECTIONAL_ATLAS_SIZE=4096
    SPOT_POINT_ATLAS_TILES=4
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation"
    )
elseif(WIN32)
     target_link_libraries(${PROJECT_NAME} PRIVATE
        dsound
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    assimp
)
