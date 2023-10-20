cmake_minimum_required(VERSION 3.16)

# KDE Gear version, managed by release service script
set(RELEASE_SERVICE_VERSION_MAJOR "23")
set(RELEASE_SERVICE_VERSION_MINOR "08")
set(RELEASE_SERVICE_VERSION_MICRO "2")

# generate patch level from release service version
set(_micro ${RELEASE_SERVICE_VERSION_MICRO})
if (_micro LESS "10")
    string(PREPEND _micro "0") # pad with 0
endif()
set(RELEASE_SERVICE_BASED_PATCHLEVEL "${RELEASE_SERVICE_VERSION_MAJOR}${RELEASE_SERVICE_VERSION_MINOR}${_micro}")

project(kdevzig VERSION "0.11.${RELEASE_SERVICE_BASED_PATCHLEVEL}")

# KDevplatform dependency version
set(KDEVPLATFORM_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(QT_MIN_VERSION "5.15.0")
set(KF5_DEP_VERSION "5.78.0")

find_package (ECM ${KF5_DEP_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)


include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

include(ECMAddTests)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdocumentation")
endif()

add_definitions( -DTRANSLATION_DOMAIN=\"kdevzig\" )


find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED Core Widgets Test)
find_package(KF5 ${KF5_DEP_VERSION} REQUIRED COMPONENTS
    I18n
    ThreadWeaver
    TextEditor
)

list(APPEND CMAKE_PREFIX_PATH "/home/jrm/projects/kdevelop/kdevplatform")
find_package(KDevPlatform ${KDEVPLATFORM_DEP_VERSION} CONFIG)
find_package(KDevelop ${KDEVPLATFORM_VERSION} REQUIRED)

set_package_properties(KDevPlatform PROPERTIES
    TYPE REQUIRED
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdocumentation")
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"kdevzig\")

add_definitions(
    -DQT_DEPRECATED_WARNINGS
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050500
    -DQT_NO_SIGNALS_SLOTS_KEYWORDS
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_STRICT_ITERATORS
    -DQT_USE_QSTRINGBUILDER
)

set(kdevziglanguagesupport_PART_SRCS
    ziglanguagesupport.cpp
    zigparsejob.cpp
    zighighlighting.cpp
)

ecm_qt_declare_logging_category(kdevziglanguagesupport_PART_SRCS
    HEADER zigdebug.h
    IDENTIFIER KDEV_ZIG
    CATEGORY_NAME "kdevelop.languages.zig"
)

add_subdirectory(app_templates)
add_subdirectory(duchain)
add_subdirectory(codecompletion)

kdevplatform_add_plugin(kdevziglanguagesupport JSON kdevzigsupport.json
    SOURCES ${kdevziglanguagesupport_PART_SRCS}
)

target_link_libraries(kdevziglanguagesupport
    KDev::Interfaces
    KDev::Language
    KF5::ThreadWeaver
    KF5::TextEditor
    kdevzigduchain
    kdevzigcompletion
)

install(FILES kdevzigsupport.categories DESTINATION ${KDE_INSTALL_CONFDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
