cmake_minimum_required(VERSION 3.20)
project(DesktopPet)

set(CMAKE_CXX_STANDARD 17)

# Statically link MSVC runtime on Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Main)

# Sources
set(SOURCES src/main.cpp)
if(APPLE)
    list(APPEND SOURCES src/platform_mac.mm)
endif()

# No console window on Windows, proper .app bundle on macOS
if(WIN32)
    add_executable(DesktopPet WIN32 ${SOURCES})
elseif(APPLE)
    add_executable(DesktopPet MACOSX_BUNDLE ${SOURCES})
else()
    add_executable(DesktopPet ${SOURCES})
endif()

target_link_libraries(DesktopPet PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    $<$<BOOL:${WIN32}>:SFML::Main>
)

if(APPLE)
    target_link_libraries(DesktopPet PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
endif()

# ── Icons ──────────────────────────────────────────────────────────────────
if(WIN32)
    # Embed icon into the exe via a Windows resource file
    # CMake auto-compiles .rc files with MSVC
    set(ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    file(WRITE ${ICON_RC} "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/res/icon.ico\"\n")
    target_sources(DesktopPet PRIVATE ${ICON_RC})
endif()

if(APPLE)
    # .icns goes inside the .app bundle under Contents/Resources/
    set(APP_ICON "${CMAKE_SOURCE_DIR}/res/icon.icns")
    set_source_files_properties(${APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(DesktopPet PRIVATE ${APP_ICON})

    set_target_properties(DesktopPet PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME       "DesktopPet"
        MACOSX_BUNDLE_BUNDLE_VERSION    "1.0.0"
        MACOSX_BUNDLE_ICON_FILE         "icon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER    "com.yourname.desktoppet"
    )
endif()

# ── Install rules ──────────────────────────────────────────────────────────
install(TARGETS DesktopPet
    RUNTIME DESTINATION .
    BUNDLE  DESTINATION .
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res
    DESTINATION .
)

# Copy res/ next to exe for local development builds
add_custom_command(TARGET DesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:DesktopPet>/res
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/res/pet_image.png
        $<TARGET_FILE_DIR:DesktopPet>/res/pet_image.png
)

# ── CPack / installer ──────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME                "DesktopPet")
set(CPACK_PACKAGE_VENDOR              "Lagrange-040506")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A cute desktop pet that lives on your screen")
set(CPACK_PACKAGE_VERSION             "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "DesktopPet")

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "DesktopPet-1.0-win64")
    set(CPACK_GENERATOR         "NSIS")

    set(CPACK_NSIS_DISPLAY_NAME                 "Desktop Pet")
    set(CPACK_NSIS_PACKAGE_NAME                 "Desktop Pet")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON                     "${CMAKE_SOURCE_DIR}/res/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON                  "${CMAKE_SOURCE_DIR}/res/icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME          "DesktopPet.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT               "https://github.com/o0Sampson0o/DesktopPet")

    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\DesktopPet.lnk' '$INSTDIR\\\\DesktopPet.exe'"
    )
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "CreateShortCut '$DESKTOP\\\\DesktopPet.lnk' '$INSTDIR\\\\DesktopPet.exe'"
    )
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "Delete '$DESKTOP\\\\DesktopPet.lnk'"
    )

elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "DesktopPet-1.0-macos")
    set(CPACK_GENERATOR         "DragNDrop")
endif()

include(CPack)