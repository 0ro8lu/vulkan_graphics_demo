cmake_minimum_required(VERSION 3.0...3.25)
project(
    Vulkan_Engine
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the file that defines target_treat_all_warnings_as_errors
# (I do this because I reuse this function across many projects and
# want to keep this main file easy to read).
include(utils.cmake)

file(GLOB_RECURSE COMMON_SOURCES src/engine/*.cpp src/engine/*.h)
# file(GLOB_RECURSE EXAMPLE_SOURCES src/examples/*.cpp src/examples/*.h)

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/glm)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator-3.1.0 SYSTEM)

# Compilation flags for assimp. The demo only needs the GLTF loader and nothing else
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/assimp SYSTEM)

option(VULKAN_STEALTH "Use the local Vulkan SDK bundled in deps/" OFF)

if(VULKAN_STEALTH)
    set(VULKAN_LOCAL_DIR "${CMAKE_SOURCE_DIR}/deps/vulkan")

    add_library(vulkan-local SHARED IMPORTED)
    set_target_properties(vulkan-local PROPERTIES
        IMPORTED_LOCATION "${VULKAN_LOCAL_DIR}/lib/libvulkan.1.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_LOCAL_DIR}/include"
    )
    set(VULKAN_TARGET vulkan-local)
else()
    find_package(Vulkan REQUIRED)
    set(VULKAN_TARGET Vulkan::Vulkan)
endif()

# Get the filename without extension to use as the target name
# get_filename_component(PROJECT_NAME ${EXAMPLE_FILE} NAME_WE)

# Set project name to smt
# set(${PROJECT_NAME} "Vulkan Example")

# Create the executable including common sources
add_executable(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    ${COMMON_SOURCES}
)

# Set C++17 standard
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/"
    "${CMAKE_SOURCE_DIR}/deps/glm"
    "${CMAKE_SOURCE_DIR}/deps/stb_image"
    "${CMAKE_SOURCE_DIR}/deps/ktx/include"
    "${CMAKE_SOURCE_DIR}/deps/ktx/other_include"
    "${CMAKE_SOURCE_DIR}/deps/cute_sound"
    "${CMAKE_SOURCE_DIR}/deps/assimp-5.4.3"
)

# Xcode puts the executable in a directory which is 1 layer deep with
# respect to when i use make (i'm forced to use Xcode for GPU debugging,
# thakns Apple). That throws off all the resource loading, and instead of
# painstakingly modfying them and reverrting them, i decided to just export
# a variable and call it a day.
if(CMAKE_GENERATOR STREQUAL "Xcode")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TEXTURE_PATH="../../textures/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="../../shaders/built/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MODEL_PATH="../../models/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOUND_PATH="../../sounds/")
elseif(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TEXTURE_PATH="../textures/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="../shaders/built/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MODEL_PATH="../models/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOUND_PATH="../sounds/")
elseif(CMAKE_GENERATOR STREQUAL "Ninja")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TEXTURE_PATH="../textures/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="../shaders/built/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MODEL_PATH="../models/")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOUND_PATH="../sounds/")
endif()

# these are constants defined for all platforms
# TODO: also add maximum number of spotlights and pointlights.
target_compile_definitions(${PROJECT_NAME} PRIVATE SPOT_POINT_ATLAS_SIZE=4096)
target_compile_definitions(${PROJECT_NAME} PRIVATE DIRECTIONAL_ATLAS_SIZE=4096)
target_compile_definitions(${PROJECT_NAME} PRIVATE SPOT_POINT_ATLAS_TILES=4)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)

if(VULKAN_STEALTH)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VULKAN_STEALTH
        VULKAN_ICD_FILEPATH="${VULKAN_LOCAL_DIR}/icd.d/MoltenVK_icd.json"
        VULKAN_LAYER_PATH="${VULKAN_LOCAL_DIR}/explicit_layer.d"
    )

    # Embed the rpath so the executable finds libvulkan.1.dylib at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${VULKAN_LOCAL_DIR}/lib"
    )
endif()

if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation"
    )
elseif(WIN32)
     target_link_libraries(${PROJECT_NAME}
        dsound
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    glfw
    ${VULKAN_TARGET}
    GPUOpen::VulkanMemoryAllocator
    assimp
)
