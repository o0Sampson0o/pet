cmake_minimum_required(VERSION 3.20)
# To this:
if(APPLE)
    project(DesktopPet LANGUAGES CXX OBJCXX)
else()
    project(DesktopPet LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)

# ── Windows: MSVC static runtime, no SFML needed ─────────────────────────────
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── macOS: fetch SFML via FetchContent ────────────────────────────────────────
if(APPLE)
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        3.0.0
        GIT_SHALLOW    TRUE
    )
    set(SFML_BUILD_AUDIO   OFF)
    set(SFML_BUILD_NETWORK OFF)
    set(BUILD_SHARED_LIBS  OFF)
    FetchContent_MakeAvailable(SFML)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
if(WIN32)
    # Windows: Win32 + Direct2D + DirectComposition, no SFML
    add_executable(DesktopPet WIN32
        src/platform_win.cpp
    )
    target_link_libraries(DesktopPet PRIVATE
        d2d1
        d3d11
        dxgi
        dcomp
        windowscodecs
        dwmapi
    )
elseif(APPLE)
    # macOS: SFML-based, Objective-C++
    add_executable(DesktopPet MACOSX_BUNDLE
        src/platform_mac.mm
    )
    target_link_libraries(DesktopPet PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
        "-framework Cocoa"
        "-framework QuartzCore"
    )

    # macOS icon
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

# app.h is shared by both platforms
target_include_directories(DesktopPet PRIVATE src)

# ── Windows icon (embedded into exe via .rc file) ─────────────────────────────
if(WIN32)
    set(ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    file(WRITE ${ICON_RC} "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/res/icon.ico\"\n")
    target_sources(DesktopPet PRIVATE ${ICON_RC})
endif()

# ── Copy res/ next to exe for local dev builds ────────────────────────────────
add_custom_command(TARGET DesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:DesktopPet>/res
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/res/pet_image.png
        $<TARGET_FILE_DIR:DesktopPet>/res/pet_image.png
)

# ── Install rules ─────────────────────────────────────────────────────────────
install(TARGETS DesktopPet
    RUNTIME DESTINATION .
    BUNDLE  DESTINATION .
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res
    DESTINATION .
)

# ── CPack / installer ─────────────────────────────────────────────────────────
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
