set(kdevzigduchain_SRC
    helpers.cpp
    contextbuilder.cpp
    declarationbuilder.cpp
    expressionvisitor.cpp
    functionvisitor.cpp
    usebuilder.cpp
    zignode.cpp
    zigducontext.cpp
    parsesession.cpp
    kdevzigastparser.h
    nodetraits.h
    types/builtintype.cpp
    types/pointertype.cpp
    types/optionaltype.cpp
    types/errortype.cpp
    types/slicetype.cpp
    types/enumtype.cpp
    types/uniontype.cpp
    types/delayedtype.cpp
    types/vectortype.cpp
    navigation//navigationwidget.cpp
)

find_package(Zig MODULE REQUIRED)

ecm_qt_declare_logging_category(kdevzigduchain_SRC
    HEADER zigdebug.h
    IDENTIFIER KDEV_ZIG
    CATEGORY_NAME "kdevelop.languages.zig.duchain"
)

add_library(kdevzigduchain SHARED ${kdevzigduchain_SRC})

set(ZIG_OPTIMIZE "ReleaseSafe")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ZIG_OPTIMIZE "ReleaseFast")
endif()


# Zig parser
add_custom_target(
    kdevzigastparser
    COMMAND ${ZIG_EXECUTABLE} build-lib "${CMAKE_CURRENT_SOURCE_DIR}/kdevzigastparser.zig" -dynamic -DOptimize=${ZIG_OPTIMIZE} -fno-strip
    BYPRODUCTS libkdevzigastparser.so
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libkdevzigastparser.so DESTINATION ${KDE_INSTALL_LIBDIR})
add_dependencies(kdevzigduchain kdevzigastparser)

generate_export_header(kdevzigduchain BASE_NAME kdevzigduchain EXPORT_MACRO_NAME KDEVZIGDUCHAIN_EXPORT)
target_link_libraries(kdevzigduchain LINK_PRIVATE
    KDev::Interfaces
    KDev::Language
    LINK_PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/libkdevzigastparser.so
)

install(TARGETS kdevzigduchain DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

