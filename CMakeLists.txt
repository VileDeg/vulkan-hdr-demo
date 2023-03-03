cmake_minimum_required (VERSION 3.24)
project(vulkan-hdr-demo)

set(CMAKE_CXX_STANDARD 20)

set(SRCDIR "${PROJECT_SOURCE_DIR}/src")
set(PCH_SRC "${SRCDIR}/stdafx.h")

find_package(Vulkan REQUIRED)
add_subdirectory(external)

set(EXNAME "vkdemo")


file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SRCDIR}/*.cpp" "${SRCDIR}/*.h")
add_executable (${EXNAME} ${SRC_FILES})

set_target_properties(
    ${EXNAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_precompile_headers(${EXNAME} PRIVATE ${PCH_SRC})

target_include_directories(${EXNAME} PUBLIC ${SRCDIR} ${Vulkan_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/external)

target_link_libraries(${EXNAME} 
    ${Vulkan_LIBRARIES}
    glfw
)
