cmake_minimum_required (VERSION 3.24)
project(vulkan-hdr-demo)

set(CMAKE_CXX_STANDARD 20)

set(SRCDIR "${PROJECT_SOURCE_DIR}/src")
set(PCH_NAME "stdafx")
set(PCH_SRC "${SRCDIR}/stdafx.h")




find_package(Vulkan REQUIRED)
add_subdirectory(external)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

#find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

#file(GLOB_RECURSE GLSL_SOURCE_FILES
#    "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
#    "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
#    "${PROJECT_SOURCE_DIR}/assets/shaders/*.comp"
#    )

set(EXNAME "vkdemo")


file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SRCDIR}/*.cpp" "${SRCDIR}/*.h")
add_executable (${EXNAME} ${SRC_FILES})

set_target_properties(
    ${EXNAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_precompile_headers(${EXNAME} PRIVATE ${PCH_SRC})
#set_property(TARGET chapter_3 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:chapter_3>)

target_include_directories(${EXNAME} PUBLIC ${SRCDIR} ${Vulkan_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/external)

target_link_libraries(${EXNAME} 
    #${PCH_NAME}
    ${Vulkan_LIBRARIES}
    glfw
    #glm
    #vma
)

#add_library(${PCH_NAME} STATIC ${PCH_SRC})
#set_target_properties(${PCH_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#target_include_directories(${PCH_NAME} PUBLIC ${SRCDIR} ${Vulkan_INCLUDE_DIRS})

#foreach(GLSL ${GLSL_SOURCE_FILES})
#  message(STATUS "BUILDING SHADER")
#  get_filename_component(FILE_NAME ${GLSL} NAME)
#  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
#  message(STATUS ${GLSL})
#  add_custom_command(
#    OUTPUT ${SPIRV}
#    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
#    DEPENDS ${GLSL})
#  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
#endforeach(GLSL)

#add_custom_target(
#    Shaders 
#    DEPENDS ${SPIRV_BINARY_FILES}
#    )