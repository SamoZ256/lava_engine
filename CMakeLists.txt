cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME lava_engine)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

add_compile_options(
    -O3
    -Wno-deprecated-volatile
)

option(BACKEND_VULKAN "Build using vulkan backend" OFF)
option(BACKEND_METAL "Build using metal backend" OFF)

set(BACKEND)

if (BACKEND_VULKAN)
    message("Building using vulkan backend")

    set(BACKEND vulkan)

    include_directories(
        "/Users/samuliak/Documents/VMA/include/VMA"
        "/Users/samuliak/Documents/lava_core/1.3/include/vulkan"
    )

    if (APPLE)
        include_directories(
            "/Users/samuliak/VulkanSDK/1.3.236.0/macOS/include"
        )
    elseif (WIN32)
        find_package(Vulkan REQUIRED)
    endif ()
elseif (BACKEND_METAL)
    message("Building using metal backend")

    set(BACKEND metal)

    include_directories(
        "/Users/samuliak/Documents/metal-cpp"
        "/Users/samuliak/Documents/lava_core/1.3/include/metal"
    )
endif ()

file(GLOB src
    "src/editor/*.cpp"
    "src/scene/*.cpp"
    "src/math/*.cpp"
    "/Users/samuliak/Documents/imgui/*.cpp"
    "/Users/samuliak/Documents/imgui/misc/cpp/*.cpp"
    "/Users/samuliak/Documents/ImGuizmo/include/ImGuizmo/*.cpp"
    "/Users/samuliak/Documents/imgui/used_backends/imgui_impl_lvnd.cpp"
    "/Users/samuliak/Documents/imgui/used_backends/imgui_impl_osx.mm"
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    ${src}
)

include_directories(
    "/Users/samuliak/Documents/lvnd/include"
    "/opt/homebrew/Cellar/glm/0.9.9.8/include"
    "/Users/samuliak/Documents/stb/include"
    "/opt/homebrew/Cellar/assimp/5.2.5/include"
    "/Users/samuliak/Documents/imgui"
    "/Users/samuliak/Documents/imgui/used_backends"
    "/Users/samuliak/Documents/imgui/misc/cpp"
    "/Users/samuliak/Documents/ImGuizmo/include"
    "/Users/samuliak/Documents/entt/include"
    "/Users/samuliak/Documents/json/include"
    "/Users/samuliak/Documents/nativefiledialog/src/include"
    "/Users/samuliak/Documents/ImGuizmo/include"
    "/Users/samuliak/Documents/lava_core/1.3/include"
    "/Users/samuliak/Documents/lava_utils/1.3/include"
)

find_library (
    NFD_LIB
    NAMES nfd libnfd
    HINTS "/Users/samuliak/Documents/nativefiledialog/src"
    NO_DEFAULT_PATH
)

find_library(
    LVND_LIB
    NAMES lvnd liblvnd
    HINTS "/Users/samuliak/Documents/lvnd/lib/${BACKEND}"
    NO_DEFAULT_PATH
)

find_library(NS_FOUNDATION_LIB Foundation)
find_library(QUARTZCORE_LIB QuartzCore)
find_library(UNIFORMTYPEIDENTIFIERS_LIB UniformTypeIdentifiers)
find_library(APPKIT_LIB AppKit)

message(STATUS "LVND_LIB: [${LVND_LIB}]")
message(STATUS "NFD_LIB: [${NFD_LIB}]")

target_link_libraries(${PROJECT_NAME} PUBLIC
    "-framework GameController"
    ${NFD_LIB}
    ${NS_FOUNDATION_LIB}
    ${QUARTZCORE_LIB}
    ${UNIFORMTYPEIDENTIFIERS_LIB}
    ${APPKIT_LIB}
    ${LVND_LIB}
)

if (BACKEND_VULKAN)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mainVK")

    if (APPLE)
        find_library (
            Vulkan_LIBRARIES
            NAMES vulkan libvulkan # what to look for
            HINTS "/Users/samuliak/VulkanSDK/1.3.236.0/macOS/lib" # where to look
        )
    endif ()

    target_sources(${PROJECT_NAME} PRIVATE
        "/Users/samuliak/Documents/imgui/used_backends/imgui_impl_vulkan.cpp"
    )

    find_library (
        VULKAN_LIB
        NAMES vulkan libvulkan
        HINTS "/usr/local/Cellar/Vulkan/macOS/lib"
    )

    find_library (
        LAVA_CORE_LIB
        NAMES lavacore liblavacore
        HINTS "/Users/samuliak/Documents/lava_core/1.3/lib/vulkan"
        NO_DEFAULT_PATH
    )
    
    find_library (
        LAVA_UTILS_LIB
        NAMES lavautils liblavautils
        HINTS "/Users/samuliak/Documents/lava_utils/1.3/lib/vulkan"
        NO_DEFAULT_PATH
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${Vulkan_LIBRARIES}
        ${LAVA_CORE_LIB}
        ${LAVA_UTILS_LIB}
    )
elseif (BACKEND_METAL)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mainMTL")

    target_sources(${PROJECT_NAME} PRIVATE
        "/Users/samuliak/Documents/imgui/used_backends/imgui_impl_metal.mm"
    )

    find_library(METAL_LIB Metal)

    find_library (
        LAVA_CORE_LIB
        NAMES lavacore liblavacore
        HINTS "/Users/samuliak/Documents/lava_core/1.3/lib/metal"
        NO_DEFAULT_PATH
    )

    find_library (
        LAVA_UTILS_LIB
        NAMES lavautils liblavautils
        HINTS "/Users/samuliak/Documents/lava_utils/1.3/lib/metal"
        NO_DEFAULT_PATH
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${METAL_LIB}
        ${LAVA_CORE_LIB}
        ${LAVA_UTILS_LIB}
    )
endif ()

add_compile_options(
    -ldl -lpthread -lX11
)
