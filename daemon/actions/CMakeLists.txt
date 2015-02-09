install(FILES powerdevilaction.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})

add_subdirectory(bundled)

if (X11_FOUND)
    add_subdirectory(dpms)
endif ()
