cmake_minimum_required(VERSION 3.0...3.25)
project(
    VulkanTest
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the file that defines target_treat_all_warnings_as_errors
# (I do this because I reuse this function across many projects and
# want to keep this main file easy to read).
include(utils.cmake)

file(GLOB_RECURSE COMMON_SOURCES src/engine/*.cpp src/engine/*.h)
file(GLOB_RECURSE EXAMPLE_SOURCES src/examples/*.cpp src/examples/*.h)

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator-3.1.0 SYSTEM)

find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)

foreach(EXAMPLE_FILE ${EXAMPLE_SOURCES})
    # Get the filename without extension to use as the target name
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
    
    # Create the executable including common sources
    add_executable(${EXAMPLE_NAME}
        ${EXAMPLE_FILE}
        ${COMMON_SOURCES}
    )
    
    # Set C++17 standard
    set_target_properties(${EXAMPLE_NAME} PROPERTIES CXX_STANDARD 17)
    
    # Include directories
    target_include_directories(${EXAMPLE_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/"
        "${CMAKE_SOURCE_DIR}/deps/glm"
        "${CMAKE_SOURCE_DIR}/deps/stb_image"
    )
    
    # Link libraries
    target_link_libraries(${EXAMPLE_NAME}
        glfw
        glm::glm
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
    )
    
    # Optionally, treat all warnings as errors
    # target_treat_all_warnings_as_errors(${EXAMPLE_NAME})
endforeach()
