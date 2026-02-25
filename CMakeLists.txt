cmake_minimum_required(VERSION 3.20)
project(DesktopPet)

set(CMAKE_CXX_STANDARD 17)

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

add_custom_command(TARGET DesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/res/pet_image.png
        $<TARGET_FILE_DIR:DesktopPet>/res/pet_image.png
)