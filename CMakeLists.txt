cmake_minimum_required (VERSION 3.22)
project(vulkan-hdr-demo)

# Use FindVulkan module added with CMAKE 3.7
IF (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
	message(STATUS "Looking for Vulkan on installed on device...")
	find_package(Vulkan)
ENDIF()

IF (UNIX AND NOT APPLE)
	set(LINUX TRUE)
ENDIF()

IF(WIN32)
    IF (NOT Vulkan_FOUND)
        MESSAGE("Looking for bundled version in ${CMAKE_SOURCE_DIR}/libs")
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
        IF (Vulkan_LIBRARY)
            set(Vulkan_FOUND ON)
            MESSAGE("Vulkan library not found on this device, using bundled Vulkan library version ${Vulkan_LIBRARIES}")
        ENDIF()
    ENDIF()
ELSEIF(LINUX)
    IF (NOT Vulkan_FOUND)
		find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR}/libs/vulkan" REQUIRED)
		IF (Vulkan_LIBRARY)
			set(Vulkan_FOUND ON)
			MESSAGE("Using bundled Vulkan library version")
		ENDIF()
	ENDIF()
ELSEIF(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
ENDIF(WIN32)

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
	message(STATUS ${Vulkan_LIBRARY})
ENDIF()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

set(EXNAME "vkdemo")

set(SRCDIR "${PROJECT_SOURCE_DIR}/src")
set(PCH_SRC "${SRCDIR}/stdafx.h")

# Add project source files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SRCDIR}/*.cpp" "${SRCDIR}/*.h")

# Add ImGUI source files
set(IMGUI_DIR  "${PROJECT_SOURCE_DIR}/external/imgui")
set(IMPLOT_DIR "${PROJECT_SOURCE_DIR}/external/imgui")

set(SRC_FILES
    ${SRC_FILES}

    ${IMGUI_DIR}/imgui_impl_glfw.cpp 
    ${IMGUI_DIR}/imgui_impl_vulkan.h
    ${IMGUI_DIR}/imgui_impl_vulkan.cpp 
    ${IMGUI_DIR}/imgui.cpp 
    ${IMGUI_DIR}/imgui_draw.cpp 
    ${IMGUI_DIR}/imgui_demo.cpp 
    ${IMGUI_DIR}/imgui_tables.cpp 
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot.h
    ${IMPLOT_DIR}/implot_demo.cpp
    ${IMPLOT_DIR}/implot_internal.h
    ${IMPLOT_DIR}/implot_items.cpp
)

# Make sure that the executable is in the root directory where "assets/" is located
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR})

add_executable (${EXNAME} ${SRC_FILES})

set_target_properties(${EXNAME} PROPERTIES 
    DEBUG_OUTPUT_NAME "${EXNAME}-debug"
)

target_precompile_headers(${EXNAME} PRIVATE ${PCH_SRC})

target_include_directories(${EXNAME} PUBLIC 
    ${SRCDIR} 
    ${PROJECT_SOURCE_DIR}/external
)

target_link_libraries(${EXNAME} 
    ${Vulkan_LIBRARY} 
    glfw
)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/assets/shaders/src")
set(SPV_DIR    "${PROJECT_SOURCE_DIR}/assets/shaders/bin")

set(SHADER_SRC ${SPV_DIR}/*)

# Compile shaders in assets/shaders folder
add_custom_target(glsl_shaders ALL
  DEPENDS ${SHADER_SRC}
  COMMENT "Shaders to compile"
)

add_dependencies(${EXNAME} glsl_shaders)

# Find Python
find_package(Python COMPONENTS Interpreter)

IF (Python_FOUND)
    message(STATUS "Python found: ${Python_EXECUTABLE}")

    # Compile shaders
    add_custom_command(
        TARGET glsl_shaders
        COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/compile_shaders.py ${PROJECT_SOURCE_DIR}
        COMMENT "Compiling shaders"
    )
ELSE()
    message(WARNING "Python not found, shaders will not be compiled automatically. Please compile shaders manually.")
ENDIF()
