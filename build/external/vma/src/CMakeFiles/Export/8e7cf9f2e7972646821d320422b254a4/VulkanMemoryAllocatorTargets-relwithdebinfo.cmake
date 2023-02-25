#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "VulkanMemoryAllocator::VulkanMemoryAllocator" for configuration "RelWithDebInfo"
set_property(TARGET VulkanMemoryAllocator::VulkanMemoryAllocator APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(VulkanMemoryAllocator::VulkanMemoryAllocator PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "CXX"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocatorrd.lib"
  )

list(APPEND _cmake_import_check_targets VulkanMemoryAllocator::VulkanMemoryAllocator )
list(APPEND _cmake_import_check_files_for_VulkanMemoryAllocator::VulkanMemoryAllocator "${_IMPORT_PREFIX}/lib/VulkanMemoryAllocatorrd.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
