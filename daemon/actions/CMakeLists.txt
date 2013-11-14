install(FILES powerdevilaction.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})

add_subdirectory(bundled)
if(HAVE_DPMS)
    add_subdirectory(dpms)
endif()