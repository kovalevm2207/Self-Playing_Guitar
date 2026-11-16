qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(SelfGuitar
    src/main.cpp
    src/backend.cpp
    src/backend.h
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
    guitar_protocol
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::SerialPort
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
