#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "VulkanMemoryAllocator::VulkanMemoryAllocator" for configuration "MinSizeRel"
set_property(TARGET VulkanMemoryAllocator::VulkanMemoryAllocator APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(VulkanMemoryAllocator::VulkanMemoryAllocator PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "CXX"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocators.lib"
  )

list(APPEND _cmake_import_check_targets VulkanMemoryAllocator::VulkanMemoryAllocator )
list(APPEND _cmake_import_check_files_for_VulkanMemoryAllocator::VulkanMemoryAllocator "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocators.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
