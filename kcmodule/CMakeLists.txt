include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_BINARY_DIR}/common
)

add_subdirectory(common)
add_subdirectory(global)
add_subdirectory(profiles)
