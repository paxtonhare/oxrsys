# SPDX-License-Identifier: MPL-2.0

set(OXRSYS_RUNTIME_SOURCES
    src/Config.cpp
    src/EntryPoint.cpp
    src/Runtime.cpp
    src/RuntimeStatus.cpp
    src/RuntimePlatform.cpp
    src/RuntimeSockets.cpp
    src/StreamingFrameQueue.cpp
    src/Instance.cpp
    src/Session.cpp
    src/Space.cpp
    src/ActionSet.cpp
    src/InputManager.cpp
    src/HandTracker.cpp
    src/StreamingServer.cpp
    src/TrackingReceiver.cpp
    src/VulkanDispatch.cpp
)

set(OXRSYS_VIDEO_ENCODER "AUTO" CACHE STRING "Runtime video encoder: AUTO, VIDEOTOOLBOX, or FFMPEG")
set_property(CACHE OXRSYS_VIDEO_ENCODER PROPERTY STRINGS AUTO VIDEOTOOLBOX FFMPEG)
string(TOUPPER "${OXRSYS_VIDEO_ENCODER}" OXRSYS_VIDEO_ENCODER_NORMALIZED)
if(NOT OXRSYS_VIDEO_ENCODER_NORMALIZED MATCHES "^(AUTO|VIDEOTOOLBOX|FFMPEG)$")
    message(FATAL_ERROR "OXRSYS_VIDEO_ENCODER must be AUTO, VIDEOTOOLBOX, or FFMPEG")
endif()

set(OXRSYS_RUNTIME_NEEDS_FFMPEG OFF)
set(OXRSYS_RUNTIME_ENABLE_OPENGL OFF)

if(APPLE)
    list(APPEND OXRSYS_RUNTIME_SOURCES
        src/SwapchainCommon.cpp
        src/SwapchainVulkan.cpp
        src/SwapchainMetal.mm
    )
    if(OXRSYS_VIDEO_ENCODER_NORMALIZED STREQUAL "FFMPEG")
        list(APPEND OXRSYS_RUNTIME_SOURCES
            src/FfmpegVideoEncoder.cpp
        )
        set(OXRSYS_RUNTIME_NEEDS_FFMPEG ON)
    else()
        list(APPEND OXRSYS_RUNTIME_SOURCES
            src/VideoEncoder.mm
        )
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(OXRSYS_VIDEO_ENCODER_NORMALIZED STREQUAL "VIDEOTOOLBOX")
        message(FATAL_ERROR "OXRSYS_VIDEO_ENCODER=VIDEOTOOLBOX is only supported on Apple platforms")
    endif()
    list(APPEND OXRSYS_RUNTIME_SOURCES
        src/SwapchainCommon.cpp
        src/SwapchainVulkan.cpp
        src/SwapchainOpenGL.cpp
        src/FfmpegVideoEncoder.cpp
    )
    set(OXRSYS_RUNTIME_NEEDS_FFMPEG ON)
    set(OXRSYS_RUNTIME_ENABLE_OPENGL ON)
elseif(WIN32)
    if(OXRSYS_VIDEO_ENCODER_NORMALIZED STREQUAL "VIDEOTOOLBOX")
        message(FATAL_ERROR "OXRSYS_VIDEO_ENCODER=VIDEOTOOLBOX is only supported on Apple platforms")
    endif()
    list(APPEND OXRSYS_RUNTIME_SOURCES
        src/SwapchainCommon.cpp
        src/SwapchainVulkan.cpp
        src/SwapchainD3D11.cpp
        src/SwapchainD3D12.cpp
        src/FfmpegVideoEncoder.cpp
    )
    set(OXRSYS_RUNTIME_NEEDS_FFMPEG ON)
else()
    message(FATAL_ERROR
        "The ${CMAKE_SYSTEM_NAME} runtime backend is not supported in this pass. "
        "Use macOS, Linux, or Windows for runtime builds."
    )
endif()

add_library(oxrsys_runtime SHARED
    ${OXRSYS_RUNTIME_SOURCES}
)

if(OXRSYS_RUNTIME_ENABLE_OPENGL)
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
endif()

set(OXRSYS_RUNTIME_LINK_LIBRARIES
    spdlog::spdlog
    glm::glm
    ${CMAKE_DL_LIBS}
)

if(OXRSYS_RUNTIME_NEEDS_FFMPEG)
    include(${CMAKE_SOURCE_DIR}/cmake/ResolveFFmpeg.cmake)

    list(APPEND OXRSYS_RUNTIME_LINK_LIBRARIES
        ${OXRSYS_FFMPEG_LIBRARIES}
    )
endif()

if(WIN32)
    list(APPEND OXRSYS_RUNTIME_LINK_LIBRARIES
        d3d11
        d3d12
        dxgi
        ws2_32
    )
endif()

target_include_directories(oxrsys_runtime PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/common/protocol/include
    ${openxr_SOURCE_DIR}/include
    ${openxr_SOURCE_DIR}/src/common
    ${OXRSYS_VULKAN_INCLUDE_DIRS}
    ${OXRSYS_FFMPEG_INCLUDE_DIRS}
)

if(OXRSYS_FFMPEG_LIBRARY_DIRS)
    target_link_directories(oxrsys_runtime PRIVATE
        ${OXRSYS_FFMPEG_LIBRARY_DIRS}
    )
endif()

target_compile_definitions(oxrsys_runtime PRIVATE
    XR_USE_GRAPHICS_API_VULKAN
    OXRSYS_USE_VULKAN
    OXRSYS_VERSION_MAJOR=${OXRSYS_VERSION_MAJOR}
    OXRSYS_VERSION_MINOR=${OXRSYS_VERSION_MINOR}
    OXRSYS_VERSION_PATCH=${OXRSYS_VERSION_PATCH}
)

if(APPLE)
    target_compile_definitions(oxrsys_runtime PRIVATE
        XR_USE_GRAPHICS_API_METAL
        OXRSYS_USE_METAL
    )
endif()

if(OXRSYS_RUNTIME_ENABLE_OPENGL)
    target_compile_definitions(oxrsys_runtime PRIVATE
        XR_USE_GRAPHICS_API_OPENGL
        XR_USE_PLATFORM_XLIB
        OXRSYS_USE_OPENGL_XLIB
        GL_GLEXT_PROTOTYPES
    )
endif()

if(WIN32)
    target_compile_definitions(oxrsys_runtime PRIVATE
        XR_USE_PLATFORM_WIN32
        XR_USE_GRAPHICS_API_D3D11
        XR_USE_GRAPHICS_API_D3D12
        OXRSYS_USE_D3D11
        OXRSYS_USE_D3D12
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# NOTE: We intentionally do NOT link against Vulkan::Vulkan.
# Linking the Vulkan loader would load the system MoltenVK ICD, which conflicts
# with app-embedded MoltenVK (e.g. Godot). Instead, all Vulkan functions are
# resolved dynamically through the app's pfnGetInstanceProcAddr.
target_link_libraries(oxrsys_runtime PRIVATE
    ${OXRSYS_RUNTIME_LINK_LIBRARIES}
)

if(OXRSYS_RUNTIME_ENABLE_OPENGL)
    target_link_libraries(oxrsys_runtime PRIVATE
        OpenGL::GL
        X11::X11
    )
endif()

if(APPLE)
    target_link_libraries(oxrsys_runtime PRIVATE
        "-framework Metal"
        "-framework MetalPerformanceShaders"
        "-framework Foundation"
        "-framework QuartzCore"
        "-framework AppKit"
        "-framework VideoToolbox"
        "-framework CoreMedia"
        "-framework CoreVideo"
    )
endif()

# Only export the negotiation entry point
set_target_properties(oxrsys_runtime PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    OUTPUT_NAME oxrsys-runtime
    PREFIX "lib"
)

if(APPLE)
    set_target_properties(oxrsys_runtime PROPERTIES
        OBJCXX_VISIBILITY_PRESET hidden
        SUFFIX ".dylib"
    )
endif()

if(WIN32)
    set_target_properties(oxrsys_runtime PROPERTIES
        PREFIX ""
    )
endif()

set(OXRSYS_RUNTIME_LIBRARY_FILENAME "$<TARGET_FILE_NAME:oxrsys_runtime>")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/oxrsys-runtime.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/oxrsys-runtime.json.in.configured
    @ONLY
)

# Copy manifest JSON next to the dylib
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/oxrsys-runtime.json
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/oxrsys-runtime.json.in.configured
)

# Copy default config file next to the dylib (if not already present)
add_custom_command(TARGET oxrsys_runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/oxrsys-runtime.toml
        ${CMAKE_CURRENT_BINARY_DIR}/oxrsys-runtime.toml
    COMMENT "Copying oxrsys-runtime.toml to build directory"
)
