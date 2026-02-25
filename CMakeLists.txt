cmake_minimum_required(VERSION 3.20)
project(DesktopPet)

set(CMAKE_CXX_STANDARD 17)

# Find SFML — looks for it on the system
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(DesktopPet src/main.cpp)

target_link_libraries(DesktopPet PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)

# macOS specific frameworks
if(APPLE)
    target_link_libraries(DesktopPet PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
endif()

# Copy pet_image.png next to the executable after build
add_custom_command(TARGET DesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/res/pet_image.png
        $<TARGET_FILE_DIR:DesktopPet>/res/pet_image.png
)