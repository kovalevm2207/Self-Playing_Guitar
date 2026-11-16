qt_standard_project_setup(REQUIRES 6.8)

add_library(guitar_desktop STATIC
    src/adapters/json_catalog_repository.cpp
    src/adapters/file_favorite_store.cpp
    src/adapters/qt_serial_transport.cpp
    src/adapters/qt_serial_transport.h
    src/presentation/catalog_view_model.cpp
    src/presentation/catalog_view_model.h
    src/presentation/transfer_view_model.cpp
    src/presentation/transfer_view_model.h
)
target_include_directories(guitar_desktop PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(guitar_desktop PUBLIC guitar_core Qt6::Core Qt6::SerialPort)
guitar_configure_target(guitar_desktop)

qt_add_executable(SelfGuitar
    src/main.cpp
)

qt_add_qml_module(SelfGuitar
    URI Guitar
    VERSION 1.0
    QML_FILES qml/Main.qml
)

qt_add_resources(SelfGuitar catalog
    PREFIX "/"
    FILES data/songs.json
)

target_link_libraries(SelfGuitar PRIVATE
    guitar_desktop
    Qt6::Quick
    Qt6::QuickControls2
)
guitar_configure_target(SelfGuitar)

set_target_properties(SelfGuitar PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

install(TARGETS SelfGuitar
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
