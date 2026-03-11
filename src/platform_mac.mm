#ifdef __APPLE__

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#import  <Cocoa/Cocoa.h>
#import  <QuartzCore/QuartzCore.h>

#include "app.h"

int main() {
    // ── Window ────────────────────────────────────────────────────────────────
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(sf::VideoMode(desktop), "DesktopPet", sf::Style::None);
    window.setFramerateLimit(60);

    // macOS transparency setup
    NSWindow* nsWindow = (NSWindow*)window.getNativeHandle();
    [nsWindow setTitleVisibility:NSWindowTitleHidden];
    [nsWindow setTitlebarAppearsTransparent:YES];
    [nsWindow setOpaque:NO];
    [nsWindow setBackgroundColor:[NSColor clearColor]];
    [nsWindow setIgnoresMouseEvents:YES];
    [nsWindow setLevel:NSFloatingWindowLevel];

    // ── Sprite sheet ──────────────────────────────────────────────────────────
    sf::Texture petTexture;
    if (!petTexture.loadFromFile("res/pet_image.png"))
        return -1;
    petTexture.setSmooth(false);

    sf::Sprite petSprite(petTexture);
    petSprite.setScale({SPRITE_SCALE, SPRITE_SCALE});

    // ── Game logic ────────────────────────────────────────────────────────────
    PetApp pet;
    pet.init(static_cast<float>(desktop.size.x),
             static_cast<float>(desktop.size.y));

    sf::Clock clock;

    // ── Game loop ─────────────────────────────────────────────────────────────
    while (window.isOpen()) {
        // Events
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (const auto* k = event->getIf<sf::Event::KeyPressed>())
                if (k->code == sf::Keyboard::Key::Escape) window.close();
        }

        float dt = clock.restart().asSeconds();

        // Mouse position in screen coordinates
        sf::Vector2i mp = sf::Mouse::getPosition();
        FrameInfo fi = pet.update(dt,
            static_cast<float>(mp.x),
            static_cast<float>(mp.y)
        );

        // Render
        petSprite.setTextureRect(sf::IntRect(
            { fi.col * FRAME_W, fi.row * FRAME_H },
            { FRAME_W, FRAME_H }
        ));
        petSprite.setPosition({fi.x, fi.y});

        window.clear(sf::Color(0, 0, 0, 0));
        window.draw(petSprite);
        window.display();
    }

    return 0;
}

#endif // __APPLE__
