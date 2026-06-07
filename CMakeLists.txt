cmake_minimum_required(VERSION 3.24)

project(SoundSwitcher
    VERSION 0.1.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus Svg)
find_package(KF6GlobalAccel REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)

add_executable(sound-switcher
    src/app_icon.cpp
    src/app_icon.h
    src/autostart_manager.cpp
    src/autostart_manager.h
    src/audio_manager.cpp
    src/audio_manager.h
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/settings_store.cpp
    src/settings_store.h
    src/shortcut_manager.cpp
    src/shortcut_manager.h
    src/tray_controller.cpp
    src/tray_controller.h
    data/icons.qrc
)

target_link_libraries(sound-switcher
    PRIVATE
        Qt6::Widgets
        Qt6::DBus
        Qt6::Svg
        KF6::GlobalAccel
        PkgConfig::PULSE
)

target_compile_definitions(sound-switcher
    PRIVATE
        SOUNDSWITCHER_DESKTOP_ID="io.github.fabian.SoundSwitcher"
        SOUNDSWITCHER_VERSION="${PROJECT_VERSION}"
)

set(SOUNDSWITCHER_DESKTOP_ID "io.github.fabian.SoundSwitcher")

configure_file(
    data/${SOUNDSWITCHER_DESKTOP_ID}.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/${SOUNDSWITCHER_DESKTOP_ID}.desktop
    @ONLY
)

configure_file(
    data/${SOUNDSWITCHER_DESKTOP_ID}.autostart.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/${SOUNDSWITCHER_DESKTOP_ID}.autostart.desktop
    @ONLY
)

if(DEFINED ENV{XDG_CONFIG_HOME} AND NOT "$ENV{XDG_CONFIG_HOME}" STREQUAL "")
    set(_soundswitcher_default_autostart_dir "$ENV{XDG_CONFIG_HOME}/autostart")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(_soundswitcher_default_autostart_dir "$ENV{HOME}/.config/autostart")
else()
    set(_soundswitcher_default_autostart_dir "${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart")
endif()

option(SOUNDSWITCHER_INSTALL_AUTOSTART "Install a desktop autostart entry" ON)
set(SOUNDSWITCHER_AUTOSTART_DIR "${_soundswitcher_default_autostart_dir}"
    CACHE PATH "Directory for the SoundSwitcher autostart desktop entry")

install(TARGETS sound-switcher
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SOUNDSWITCHER_DESKTOP_ID}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES data/${SOUNDSWITCHER_DESKTOP_ID}.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

if(SOUNDSWITCHER_INSTALL_AUTOSTART)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SOUNDSWITCHER_DESKTOP_ID}.autostart.desktop
        DESTINATION ${SOUNDSWITCHER_AUTOSTART_DIR}
        RENAME ${SOUNDSWITCHER_DESKTOP_ID}.desktop
    )
endif()
