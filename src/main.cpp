#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <deque>

#ifdef _WIN32
#include <Windows.h>
#endif

// ADD this instead at the top:
#ifdef __APPLE__
#include "platform_mac.h"
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Sprite sheet  (pet_image.png)   256 × 384 px  —  64×64 per frame
//
//  Row 0  (y=  0)  STAND      4 frames  (breathing + blink)   ← default
//  Row 1  (y= 64)  WALK RIGHT 4 frames
//  Row 2  (y=128)  WALK LEFT  4 frames
//  Row 3  (y=192)  SLEEP      4 frames  (2 unique, looping)
//  Row 4  (y=256)  FAINT/HIT  4 frames  (one-shot)
//  Row 5  (y=320)  FALLEN     4 frames  (looping spinning stars)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int   FRAME_W = 64;
static constexpr int   FRAME_H = 64;

// Frame counts per row
static constexpr int   STAND_FRAMES = 4;
static constexpr int   WALK_FRAMES = 4;
static constexpr int   SLEEP_FRAMES = 2;   // 2 unique, displayed in loop
static constexpr int   FAINT_FRAMES = 4;
static constexpr int   FLAT_FRAMES = 4;

// Animation speeds (fps)
static constexpr float STAND_FPS = 2.f;   // slow breathing / blink
static constexpr float WALK_FPS = 8.f;
static constexpr float SLEEP_FPS = 1.f;   // very slow breathing
static constexpr float FAINT_FPS = 6.f;
static constexpr float FLAT_FPS = 4.f;

// Movement
static constexpr float WALK_SPEED = 80.f;  // px / s
static constexpr float GRAVITY = 900.f;
static constexpr float FALLEN_DUR = 2.8f;  // seconds before recovery

// ─────────────────────────────────────────────────────────────────────────────
//  Behaviour weights  (must sum to 1.0)
//
//  STAND  60 %  — cat just stands and looks around
//  WALK   25 %  — walks a bit (split 12.5 % each direction)
//  SLEEP  15 %  — curls up and naps
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float W_STAND = 0.60f;
static constexpr float W_WALK = 0.25f;   // 12.5 % each direction
// static constexpr float W_SLEEP    = 0.15f;   // remainder

// State durations (seconds)
static constexpr float STAND_DUR_LO = 3.f;
static constexpr float STAND_DUR_HI = 7.f;
static constexpr float WALK_DUR_LO = 1.5f;
static constexpr float WALK_DUR_HI = 4.f;
static constexpr float SLEEP_DUR_LO = 4.f;
static constexpr float SLEEP_DUR_HI = 10.f;

// ─────────────────────────────────────────────────────────────────────────────
enum class State { Stand, WalkRight, WalkLeft, Sleep, Fainting, Fallen };

static float randRange(float lo, float hi) {
    return lo + (hi - lo) * (std::rand() / static_cast<float>(RAND_MAX));
}

// Pick next voluntary state according to weights, respecting screen edges
static State pickNextState(float posX, float minX, float maxX) {
    if (posX <= minX + 10.f) return State::WalkRight;
    if (posX >= maxX - 10.f) return State::WalkLeft;

    float r = randRange(0.f, 1.f);
    if (r < W_STAND)               return State::Stand;
    if (r < W_STAND + W_WALK / 2.f) return State::WalkRight;
    if (r < W_STAND + W_WALK)     return State::WalkLeft;
    return State::Sleep;
}

static float durationFor(State s) {
    switch (s) {
    case State::Stand:     return randRange(STAND_DUR_LO, STAND_DUR_HI);
    case State::WalkRight:
    case State::WalkLeft:  return randRange(WALK_DUR_LO, WALK_DUR_HI);
    case State::Sleep:     return randRange(SLEEP_DUR_LO, SLEEP_DUR_HI);
    default:               return 2.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse downstroke detector
// ─────────────────────────────────────────────────────────────────────────────
struct StrokeDetector {
    static constexpr int   HISTORY = 8;
    static constexpr float MIN_VEL = 350.f;

    struct Sample { float y, t; };
    std::deque<Sample> hist;

    void push(float y, float t) { hist.push_back({ y,t }); if ((int)hist.size() > HISTORY) hist.pop_front(); }
    void clear() { hist.clear(); }

    bool downstroke() const {
        if ((int)hist.size() < 3) return false;
        float dy = hist.back().y - hist.front().y;
        float dt = hist.back().t - hist.front().t;
        if (dt <= 0.f) return false;
        return (dy / dt) > MIN_VEL && dy > 20.f;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // ── Window ────────────────────────────────────────────────────────────────
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(sf::VideoMode(desktop), "Desktop Pet", sf::Style::None);
    window.setFramerateLimit(60);

#ifdef _WIN32
    sf::WindowHandle hwnd = window.getNativeHandle();
    SetWindowLong(hwnd, GWL_EXSTYLE,
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif

#ifdef __APPLE__
    setupMacWindow(window.getNativeHandle());
#endif

    // ── Texture & Sprite ──────────────────────────────────────────────────────
    sf::Texture petTexture;
    if (!petTexture.loadFromFile("res/pet_image.png"))
        return -1;

    sf::Sprite petSprite(petTexture);
    petSprite.setScale({ 2.f, 2.f });

    const float SPRITE_W = FRAME_W * 2.f;
    const float SPRITE_H = FRAME_H * 2.f;

    float posX = static_cast<float>(desktop.size.x) / 2.f;
    float posY = static_cast<float>(desktop.size.y) * 0.80f;
    float velY = 0.f;
    const float groundY = posY;
    const float minX = 0.f;
    const float maxX = static_cast<float>(desktop.size.x) - SPRITE_W;

    // ── Behaviour ─────────────────────────────────────────────────────────────
    State state = State::Stand;
    float stateTimer = 0.f;
    float stateDuration = durationFor(State::Stand);

    // Universal animation counters (driven per-state below)
    int   animFrame = 0;
    float animTimer = 0.f;

    // Faint-specific
    float faintTimer = 0.f;
    float fallenTimer = 0.f;

    StrokeDetector stroke;
    float totalTime = 0.f;
    sf::Clock clock;

    // ── Game loop ─────────────────────────────────────────────────────────────
    while (window.isOpen())
    {
        const float dt = clock.restart().asSeconds();
        totalTime += dt;

        // Events
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (const auto* k = event->getIf<sf::Event::KeyPressed>())
                if (k->code == sf::Keyboard::Key::Escape) window.close();
        }

        // Mouse stroke detection
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        float mx = static_cast<float>(mp.x), my = static_cast<float>(mp.y);
        stroke.push(my, totalTime);

        sf::FloatRect hitbox({ posX, posY }, { SPRITE_W, SPRITE_H });
        bool overPet = hitbox.contains({ mx, my });

        if (overPet && stroke.downstroke() &&
            state != State::Fainting && state != State::Fallen)
        {
            state = State::Fainting;
            faintTimer = 0.f;
            animFrame = 0;
            animTimer = 0.f;
            velY = 0.f;
            stroke.clear();
        }

        // ── State machine ─────────────────────────────────────────────────────
        int displayRow = 0;
        int displayFrame = 0;

        switch (state)
        {
            // ── Voluntary states ─────────────────────────────────────────────────
        case State::Stand:
        case State::WalkRight:
        case State::WalkLeft:
        case State::Sleep:
        {
            stateTimer += dt;

            // Transition when duration expires
            if (stateTimer >= stateDuration) {
                state = pickNextState(posX, minX, maxX);
                stateDuration = durationFor(state);
                stateTimer = 0.f;
                animFrame = 0;
                animTimer = 0.f;
            }

            // Movement
            if (state == State::WalkRight) posX = std::min(posX + WALK_SPEED * dt, maxX);
            if (state == State::WalkLeft)  posX = std::max(posX - WALK_SPEED * dt, minX);

            // Animation
            switch (state) {
            case State::Stand: {
                displayRow = 0;
                animTimer += dt;
                if (animTimer >= 1.f / STAND_FPS) { animTimer = 0.f; animFrame = (animFrame + 1) % STAND_FRAMES; }
                displayFrame = animFrame;
                break;
            }
            case State::WalkRight: {
                displayRow = 1;
                animTimer += dt;
                if (animTimer >= 1.f / WALK_FPS) { animTimer = 0.f; animFrame = (animFrame + 1) % WALK_FRAMES; }
                displayFrame = animFrame;
                break;
            }
            case State::WalkLeft: {
                displayRow = 2;
                animTimer += dt;
                if (animTimer >= 1.f / WALK_FPS) { animTimer = 0.f; animFrame = (animFrame + 1) % WALK_FRAMES; }
                displayFrame = animFrame;
                break;
            }
            case State::Sleep: {
                displayRow = 3;
                animTimer += dt;
                if (animTimer >= 1.f / SLEEP_FPS) { animTimer = 0.f; animFrame = (animFrame + 1) % SLEEP_FRAMES; }
                displayFrame = animFrame;
                break;
            }
            default: break;
            }
            break;
        }

        // ── Fainting (one-shot 4 frames) ──────────────────────────────────────
        case State::Fainting:
        {
            faintTimer += dt;
            animTimer += dt;
            if (animTimer >= 1.f / FAINT_FPS) {
                animTimer = 0.f;
                if (animFrame < FAINT_FRAMES - 1) ++animFrame;
            }
            displayRow = 4;
            displayFrame = animFrame;

            if (faintTimer >= static_cast<float>(FAINT_FRAMES) / FAINT_FPS) {
                state = State::Fallen;
                fallenTimer = 0.f;
                animFrame = 0;
                animTimer = 0.f;
                velY = -90.f;
            }
            break;
        }

        // ── Fallen flat ────────────────────────────────────────────────────────
        case State::Fallen:
        {
            fallenTimer += dt;

            // Bounce physics
            if (posY < groundY) {
                velY += GRAVITY * dt;
                posY += velY * dt;
                if (posY >= groundY) {
                    posY = groundY;
                    velY = -velY * 0.35f;
                    if (std::abs(velY) < 20.f) velY = 0.f;
                }
            }
            else { posY = groundY; velY = 0.f; }

            // Spinning star animation
            animTimer += dt;
            if (animTimer >= 1.f / FLAT_FPS) { animTimer = 0.f; animFrame = (animFrame + 1) % FLAT_FRAMES; }
            displayRow = 5;
            displayFrame = animFrame;

            // Recovery → always go back to Stand first
            if (fallenTimer >= FALLEN_DUR) {
                posY = groundY;
                state = State::Stand;
                stateDuration = durationFor(State::Stand);
                stateTimer = 0.f;
                animFrame = 0;
                animTimer = 0.f;
            }
            break;
        }
        } // switch

        // ── Render ────────────────────────────────────────────────────────────
        petSprite.setTextureRect(sf::IntRect(
            { displayFrame * FRAME_W, displayRow * FRAME_H },
            { FRAME_W, FRAME_H }
        ));
        petSprite.setPosition({ posX, posY });

        window.clear(sf::Color(0, 0, 0, 0));
        window.draw(petSprite);
        window.display();
    }

    return 0;
}