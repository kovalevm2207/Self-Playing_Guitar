add_library(guitar_core STATIC
    domain/music.cpp
    application/catalog_service.cpp
    application/transfer_session.cpp
    application/upload_arrangement.cpp
)
target_include_directories(guitar_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(guitar_core PUBLIC guitar_protocol)
guitar_configure_target(guitar_core)
