#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "VulkanMemoryAllocator::VulkanMemoryAllocator" for configuration "Release"
set_property(TARGET VulkanMemoryAllocator::VulkanMemoryAllocator APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(VulkanMemoryAllocator::VulkanMemoryAllocator PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocator.lib"
  )

list(APPEND _cmake_import_check_targets VulkanMemoryAllocator::VulkanMemoryAllocator )
list(APPEND _cmake_import_check_files_for_VulkanMemoryAllocator::VulkanMemoryAllocator "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocator.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
