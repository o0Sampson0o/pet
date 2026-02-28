cmake_minimum_required(VERSION 3.20)
project(DesktopPet)

set(CMAKE_CXX_STANDARD 17)

# Statically link MSVC runtime on Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

# Sources — add platform_mac.mm only on Apple
set(SOURCES src/main.cpp)
if(APPLE)
    list(APPEND SOURCES src/platform_mac.mm)
endif()

add_executable(DesktopPet ${SOURCES})

target_link_libraries(DesktopPet PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)

if(APPLE)
    target_link_libraries(DesktopPet PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
endif()

# Install rules — defines what goes where when CPack packages everything
install(TARGETS DesktopPet
    RUNTIME DESTINATION .
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res
    DESTINATION .
)

# ── CPack configuration ────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME                "DesktopPet")
set(CPACK_PACKAGE_VENDOR              "Lagrange-040506")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A cute desktop pet that lives on your screen")
set(CPACK_PACKAGE_VERSION             "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "DesktopPet")

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "DesktopPet-1.0-win64")
    set(CPACK_GENERATOR         "NSIS")

    # Installer appearance
    set(CPACK_NSIS_DISPLAY_NAME             "Desktop Pet")
    set(CPACK_NSIS_PACKAGE_NAME             "Desktop Pet")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # Start Menu shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\DesktopPet.lnk' '$INSTDIR\\\\DesktopPet.exe'"
    )

    # Desktop shortcut
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "CreateShortCut '$DESKTOP\\\\DesktopPet.lnk' '$INSTDIR\\\\DesktopPet.exe'"
    )

    # Remove desktop shortcut on uninstall
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "Delete '$DESKTOP\\\\DesktopPet.lnk'"
    )

    # Register in Add/Remove Programs
    set(CPACK_NSIS_INSTALLED_ICON_NAME  "DesktopPet.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT       "https://github.com/YOURNAME/DesktopPet")

elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "DesktopPet-1.0-macos")
    set(CPACK_GENERATOR         "DragNDrop")
endif()

include(CPack)