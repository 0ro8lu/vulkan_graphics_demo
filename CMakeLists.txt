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

file(GLOB_RECURSE sources src/*.cpp src/*.h)

# Create the App executable target with one source file
add_executable(App
    ${sources}
)

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator-3.1.0 SYSTEM)

target_include_directories(App PRIVATE "${CMAKE_SOURCE_DIR}/deps/glm")
target_include_directories(App PRIVATE "${CMAKE_SOURCE_DIR}/deps/stb_image")

# Use C++17
set_target_properties(App PROPERTIES CXX_STANDARD 17)

# Turn all warnings on and treat them as errors so that are not
# tempted to ignore them (See utils.cmake for details).
# target_treat_all_warnings_as_errors(App)

find_package(Vulkan REQUIRED)
# find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(App glfw glm::glm Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
