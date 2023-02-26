cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME lava_engine)

project(${PROJECT_NAME})

#if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#    message("Using clang")
#elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#    message("Using gcc")
#endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

add_compile_options(
    -O3
    -Wno-deprecated-volatile
)

set(IMGUI_PLATFORM_FILE)
if (APPLE)
    set(IMGUI_PLATFORM_FILE external/imgui/used_backends/imgui_impl_osx.mm)
elseif (WIN32)
    set(IMGUI_PLATFORM_FILE external/imgui/used_backends/imgui_impl_win32.cpp)
endif ()

option(BACKEND_VULKAN "Build using vulkan backend" OFF)
option(BACKEND_METAL "Build using metal backend" OFF)
option(BACKEND_OPENGL "Build using opengl backend" OFF)

set(BACKEND)

if (BACKEND_VULKAN)
    set(BACKEND vulkan)

    add_compile_definitions(
        LV_BACKEND_VULKAN
    )

    include_directories(
        "external/vma"
    )
elseif (BACKEND_METAL)
    set(BACKEND metal)

    add_compile_definitions(
        LV_BACKEND_METAL
    )

    include_directories(
        "external/metal-cpp/include"
    )
elseif (BACKEND_OPENGL)
    set(BACKEND opengl)

    add_compile_definitions(
        LV_BACKEND_OPENGL
    )

    include_directories(
        "external/glad/include"
    )
endif ()

file(GLOB src
    "src/editor/*.cpp"
    "src/scene/*.cpp"
    "external/imgui/*.cpp"
    "external/imgui/misc/cpp/*.cpp"
    "external/imguizmo/include/ImGuizmo/*.cpp"
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    external/imgui/used_backends/imgui_impl_lvnd.cpp
    ${IMGUI_PLATFORM_FILE}
    ${src}
)

find_package(Bullet REQUIRED)

find_package(Assimp)

find_package(glm REQUIRED)

include_directories(
    "/Users/samuliak/Documents/lava_window/include"
    "/Users/samuliak/Documents/nativefiledialog/src/include"
    "/Users/samuliak/Documents/lava_core/include"
    "/Users/samuliak/Documents/lava_utils/include"
    "external/stb/include"
    "external/imgui"
    "external/imgui/used_backends"
    "external/imgui/misc/cpp"
    "external/imguizmo/include"
    "external/entt/src"
    "external/json/include"
    ${GLM_INCLUDE_DIRS}
    ${ASSIMP_INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIRS}
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
    HINTS "/Users/samuliak/Documents/lava_window/lib"
    NO_DEFAULT_PATH
)

message(STATUS "LVND_LIB: [${LVND_LIB}]")
message(STATUS "NFD_LIB: [${NFD_LIB}]")

find_library (
    LAVA_CORE_LIB
    NAMES lavacore liblavacore
    HINTS "/Users/samuliak/Documents/lava_core/lib"
    NO_DEFAULT_PATH
)
    
find_library (
    LAVA_UTILS_LIB
    NAMES lavautils liblavautils
    HINTS "/Users/samuliak/Documents/lava_utils/lib/${BACKEND}"
    NO_DEFAULT_PATH
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${NFD_LIB}
    ${LVND_LIB}
    ${LAVA_CORE_LIB}
    ${LAVA_UTILS_LIB}
    ${BULLET_LIBRARIES}
)

if (APPLE)
    find_library(NS_FOUNDATION_LIB Foundation)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(UNIFORMTYPEIDENTIFIERS_LIB UniformTypeIdentifiers)
    find_library(APPKIT_LIB AppKit)

    target_link_libraries(${PROJECT_NAME} PUBLIC
        "-framework GameController"
        ${NS_FOUNDATION_LIB}
        ${QUARTZCORE_LIB}
        ${UNIFORMTYPEIDENTIFIERS_LIB}
        ${APPKIT_LIB}
    )
endif ()

if (BACKEND_VULKAN)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mainVK")

    if (APPLE)
        include_directories(
            "/Users/samuliak/VulkanSDK/1.3.236.0/macOS/include"
        )
    else ()
        find_package(Vulkan REQUIRED)
    endif ()

    if (APPLE)
        find_library (
            Vulkan_LIBRARIES
            NAMES vulkan libvulkan # what to look for
            HINTS "/Users/samuliak/VulkanSDK/1.3.236.0/macOS/lib" # where to look
        )
    endif ()

    target_sources(${PROJECT_NAME} PRIVATE
        "external/imgui/used_backends/imgui_impl_vulkan.cpp"
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${Vulkan_LIBRARIES}
    )
elseif (BACKEND_METAL)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mainMTL")

    target_sources(${PROJECT_NAME} PRIVATE
        "external/imgui/used_backends/imgui_impl_metal.mm"
    )

    find_library(METAL_LIB Metal)

    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${METAL_LIB}
    )
elseif (BACKEND_OPENGL)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mainGL")

    target_sources(${PROJECT_NAME} PRIVATE
        "external/imgui/used_backends/imgui_impl_opengl3.cpp"
    )
endif ()

add_compile_options(
    -ldl -lpthread -lX11
)
