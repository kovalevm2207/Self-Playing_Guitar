add_executable(score_tests score_tests.cpp)
target_link_libraries(score_tests PRIVATE guitar_protocol)
guitar_configure_target(score_tests)
add_test(NAME score_protocol COMMAND score_tests)

add_executable(receiver_tests receiver_tests.cpp)
target_link_libraries(receiver_tests PRIVATE guitar_firmware)
target_include_directories(receiver_tests PRIVATE stubs)
guitar_configure_target(receiver_tests)
add_test(NAME firmware_parser COMMAND receiver_tests)

add_executable(playback_tests playback_tests.cpp)
target_link_libraries(playback_tests PRIVATE guitar_firmware)
guitar_configure_target(playback_tests)
add_test(NAME firmware_playback COMMAND playback_tests)

add_executable(playback_integration_tests playback_integration_tests.cpp)
target_link_libraries(playback_integration_tests PRIVATE guitar_firmware guitar_core)
guitar_configure_target(playback_integration_tests)
add_test(NAME playback_integration COMMAND playback_integration_tests)

add_executable(catalog_tests catalog_tests.cpp)
target_link_libraries(catalog_tests PRIVATE guitar_core)
guitar_configure_target(catalog_tests)
add_test(NAME catalog_domain COMMAND catalog_tests)

add_executable(transfer_tests transfer_tests.cpp)
target_link_libraries(transfer_tests PRIVATE guitar_core)
guitar_configure_target(transfer_tests)
add_test(NAME transfer_session COMMAND transfer_tests)

if(GUITAR_BUILD_APP)
    add_executable(presentation_tests presentation_tests.cpp)
    target_link_libraries(presentation_tests PRIVATE
        guitar_desktop Qt6::Quick Qt6::QuickControls2)
    target_compile_definitions(presentation_tests PRIVATE
        GUITAR_QML_FILE="${PROJECT_SOURCE_DIR}/desktop/qml/Main.qml"
        GUITAR_CATALOG_FILE="${PROJECT_SOURCE_DIR}/desktop/data/songs.json")
    guitar_configure_target(presentation_tests)
    add_test(NAME qt_presentation COMMAND presentation_tests)
    set_tests_properties(qt_presentation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
