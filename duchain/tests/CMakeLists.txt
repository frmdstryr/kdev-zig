
add_definitions(-DDUCHAIN_ZIG_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

ecm_add_test(duchaintest.cpp
    TEST_NAME duchain
    LINK_LIBRARIES
        # kdevziglanguagesupport
        kdevzigduchain
        Qt${QT_MAJOR_VERSION}::Test
        KDev::Language
        KDev::Tests
)
