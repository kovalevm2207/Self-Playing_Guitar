# Portable firmware logic, exercised on the host without Arduino or Qt.
add_library(guitar_firmware STATIC
    src/config/machine_config.cpp
    src/hardware/actuator_bank.cpp
    src/hardware/solenoid_driver.cpp
    src/hardware/stepper_driver.cpp
    src/motion/motion_planner.cpp
    src/playback/playback_controller.cpp
    src/protocol/command_receiver.cpp
)
target_include_directories(guitar_firmware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(guitar_firmware PUBLIC guitar_protocol)
guitar_configure_target(guitar_firmware)
