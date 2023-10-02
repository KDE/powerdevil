include(ECMMarkAsTest)

add_subdirectory(migrateconfig)
add_subdirectory(profilegenerator)

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME MATCHES "Linux" AND DEFINED ENV{KDECI_BUILD})
    add_subdirectory(batterymonitor)
endif()
