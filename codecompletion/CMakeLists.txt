set(kdevzigcompletion_SRC
    model.cpp
    worker.cpp
    context.cpp
    item.cpp
)

ecm_qt_declare_logging_category(kdevzigcompletion_SRC
    HEADER zigdebug.h
    IDENTIFIER KDEV_ZIG
    CATEGORY_NAME "kdevelop.languages.zig.completion"
)

add_library(kdevzigcompletion SHARED ${kdevzigcompletion_SRC})

generate_export_header(kdevzigcompletion BASE_NAME kdevzigcompletion EXPORT_MACRO_NAME KDEVZIGCOMPLETION_EXPORT)
target_link_libraries(kdevzigcompletion LINK_PRIVATE
    KDev::Interfaces
    KDev::Language
)

install(TARGETS kdevzigcompletion DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

#add_subdirectory(tests)
