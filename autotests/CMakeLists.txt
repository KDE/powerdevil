include(ECMMarkAsTest)

add_executable(batteryitemtest batteryitemtest.cpp)
target_link_libraries(batteryitemtest PRIVATE Qt6::QuickTest)
ecm_mark_as_test(batteryitemtest)

add_test(
    NAME batteryitemtest
    COMMAND batteryitemtest -input ${CMAKE_CURRENT_SOURCE_DIR}/tst_batteryitem.qml
)
set_tests_properties(batteryitemtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_subdirectory(migrateconfig)
