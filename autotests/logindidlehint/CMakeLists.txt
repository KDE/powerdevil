include(ECMAddTests)

ecm_add_test(logindidlehintclienttest.cpp
    ${CMAKE_SOURCE_DIR}/daemon/actions/bundled/logindidlehintclient.cpp
    TEST_NAME logindidlehintclienttest
    LINK_LIBRARIES Qt::Test Qt::DBus powerdevil_log_static
)
target_include_directories(logindidlehintclienttest PRIVATE ${CMAKE_SOURCE_DIR}/daemon/actions/bundled)
