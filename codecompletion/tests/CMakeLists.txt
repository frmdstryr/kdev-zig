set(completiontest_SRCS
    completiontest.cpp
    ../completiondebug.cpp)

ecm_add_test(${completiontest_SRCS}
    TEST_NAME completiontest
    LINK_LIBRARIES
        # must be run in build folder or it does not find kdevziglanguagesupport.so
        kdevziglanguagesupport
        kdevzigduchain
        kdevzigcompletion
        Qt5::Test
        KDev::Tests
)
