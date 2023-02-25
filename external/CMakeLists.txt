add_subdirectory(glfw)

add_library(vma INTERFACE)
add_library(glm INTERFACE)

target_include_directories(glm INTERFACE glm)
target_include_directories(vma INTERFACE vma/include)
