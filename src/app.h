#pragma once
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <deque>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Sprite sheet  (res/pet_image.png)   256 × 384 px  —  64×64 per frame
//
//  Row 0  (y=  0)  STAND      4 frames
//  Row 1  (y= 64)  WALK RIGHT 4 frames
//  Row 2  (y=128)  WALK LEFT  4 frames
//  Row 3  (y=192)  SLEEP      4 frames
//  Row 4  (y=256)  FAINT/HIT  4 frames  (one-shot)
//  Row 5  (y=320)  FALLEN     4 frames  (looping)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int   FRAME_W       = 64;
static constexpr int   FRAME_H       = 64;
static constexpr int   STAND_FRAMES  = 4;
static constexpr int   WALK_FRAMES   = 4;
static constexpr int   SLEEP_FRAMES  = 2;
static constexpr int   FAINT_FRAMES  = 4;
static constexpr int   FLAT_FRAMES   = 4;

static constexpr float STAND_FPS     = 2.f;
static constexpr float WALK_FPS      = 8.f;
static constexpr float SLEEP_FPS     = 1.f;
static constexpr float FAINT_FPS     = 6.f;
static constexpr float FLAT_FPS      = 4.f;

static constexpr float WALK_SPEED    = 80.f;
static constexpr float GRAVITY       = 900.f;
static constexpr float FALLEN_DUR    = 2.8f;

static constexpr float W_STAND       = 0.60f;
static constexpr float W_WALK        = 0.25f;

static constexpr float STAND_DUR_LO  = 3.f;
static constexpr float STAND_DUR_HI  = 7.f;
static constexpr float WALK_DUR_LO   = 1.5f;
static constexpr float WALK_DUR_HI   = 4.f;
static constexpr float SLEEP_DUR_LO  = 4.f;
static constexpr float SLEEP_DUR_HI  = 10.f;

static constexpr float SPRITE_SCALE  = 2.f;
static constexpr float SPRITE_W      = FRAME_W * SPRITE_SCALE;
static constexpr float SPRITE_H      = FRAME_H * SPRITE_SCALE;

// ─────────────────────────────────────────────────────────────────────────────
enum class State { Stand, WalkRight, WalkLeft, Sleep, Fainting, Fallen };

inline float randRange(float lo, float hi) {
    return lo + (hi - lo) * (std::rand() / static_cast<float>(RAND_MAX));
}

inline State pickNextState(float posX, float minX, float maxX) {
    if (posX <= minX + 10.f) return State::WalkRight;
    if (posX >= maxX - 10.f) return State::WalkLeft;
    float r = randRange(0.f, 1.f);
    if (r < W_STAND)              return State::Stand;
    if (r < W_STAND + W_WALK/2.f) return State::WalkRight;
    if (r < W_STAND + W_WALK)     return State::WalkLeft;
    return State::Sleep;
}

inline float durationFor(State s) {
    switch (s) {
        case State::Stand:    return randRange(STAND_DUR_LO, STAND_DUR_HI);
        case State::WalkRight:
        case State::WalkLeft: return randRange(WALK_DUR_LO,  WALK_DUR_HI);
        case State::Sleep:    return randRange(SLEEP_DUR_LO, SLEEP_DUR_HI);
        default:              return 2.f;
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

    void push(float y, float t) {
        hist.push_back({y, t});
        if ((int)hist.size() > HISTORY) hist.pop_front();
    }
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
//  PetApp — pure game logic, no platform code
//  Each platform feeds it dt + mouse position each frame and asks what to draw
// ─────────────────────────────────────────────────────────────────────────────
struct FrameInfo {
    int   row;      // sprite sheet row
    int   col;      // sprite sheet column
    float x;        // draw position X
    float y;        // draw position Y
};

class PetApp {
public:
    void init(float screenW, float screenH) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        minX    = 0.f;
        maxX    = screenW - SPRITE_W;
        posX    = screenW / 2.f;
        posY    = screenH * 0.80f;
        groundY = posY;
        velY    = 0.f;

        state         = State::Stand;
        stateTimer    = 0.f;
        stateDuration = durationFor(State::Stand);
        animFrame     = 0;
        animTimer     = 0.f;
        faintTimer    = 0.f;
        fallenTimer   = 0.f;
        totalTime     = 0.f;
    }

    // Call once per frame. mouseX/mouseY are in screen coordinates.
    // Returns what frame to draw and where.
    FrameInfo update(float dt, float mouseX, float mouseY) {
        totalTime += dt;
        stroke.push(mouseY, totalTime);

        // Hit detection — downstroke over pet triggers faint
        bool overPet = (mouseX >= posX && mouseX <= posX + SPRITE_W &&
                        mouseY >= posY && mouseY <= posY + SPRITE_H);

        if (overPet && stroke.downstroke() &&
            state != State::Fainting && state != State::Fallen)
        {
            state      = State::Fainting;
            faintTimer = 0.f;
            animFrame  = 0;
            animTimer  = 0.f;
            velY       = 0.f;
            stroke.clear();
        }

        int displayRow   = 0;
        int displayFrame = 0;

        switch (state)
        {
        case State::Stand:
        case State::WalkRight:
        case State::WalkLeft:
        case State::Sleep:
        {
            stateTimer += dt;
            if (stateTimer >= stateDuration) {
                state         = pickNextState(posX, minX, maxX);
                stateDuration = durationFor(state);
                stateTimer    = 0.f;
                animFrame     = 0;
                animTimer     = 0.f;
            }

            if (state == State::WalkRight) posX = std::min(posX + WALK_SPEED*dt, maxX);
            if (state == State::WalkLeft)  posX = std::max(posX - WALK_SPEED*dt, minX);

            switch (state) {
                case State::Stand:
                    displayRow = 0;
                    animTimer += dt;
                    if (animTimer >= 1.f/STAND_FPS) { animTimer=0.f; animFrame=(animFrame+1)%STAND_FRAMES; }
                    displayFrame = animFrame;
                    break;
                case State::WalkRight:
                    displayRow = 1;
                    animTimer += dt;
                    if (animTimer >= 1.f/WALK_FPS) { animTimer=0.f; animFrame=(animFrame+1)%WALK_FRAMES; }
                    displayFrame = animFrame;
                    break;
                case State::WalkLeft:
                    displayRow = 2;
                    animTimer += dt;
                    if (animTimer >= 1.f/WALK_FPS) { animTimer=0.f; animFrame=(animFrame+1)%WALK_FRAMES; }
                    displayFrame = animFrame;
                    break;
                case State::Sleep:
                    displayRow = 3;
                    animTimer += dt;
                    if (animTimer >= 1.f/SLEEP_FPS) { animTimer=0.f; animFrame=(animFrame+1)%SLEEP_FRAMES; }
                    displayFrame = animFrame;
                    break;
                default: break;
            }
            break;
        }

        case State::Fainting:
        {
            faintTimer += dt;
            animTimer  += dt;
            if (animTimer >= 1.f/FAINT_FPS) {
                animTimer = 0.f;
                if (animFrame < FAINT_FRAMES-1) ++animFrame;
            }
            displayRow   = 4;
            displayFrame = animFrame;

            if (faintTimer >= static_cast<float>(FAINT_FRAMES)/FAINT_FPS) {
                state       = State::Fallen;
                fallenTimer = 0.f;
                animFrame   = 0;
                animTimer   = 0.f;
                velY        = -90.f;
            }
            break;
        }

        case State::Fallen:
        {
            fallenTimer += dt;

            if (posY < groundY) {
                velY += GRAVITY * dt;
                posY += velY * dt;
                if (posY >= groundY) {
                    posY = groundY;
                    velY = -velY * 0.35f;
                    if (std::abs(velY) < 20.f) velY = 0.f;
                }
            } else { posY = groundY; velY = 0.f; }

            animTimer += dt;
            if (animTimer >= 1.f/FLAT_FPS) { animTimer=0.f; animFrame=(animFrame+1)%FLAT_FRAMES; }
            displayRow   = 5;
            displayFrame = animFrame;

            if (fallenTimer >= FALLEN_DUR) {
                posY          = groundY;
                state         = State::Stand;
                stateDuration = durationFor(State::Stand);
                stateTimer    = 0.f;
                animFrame     = 0;
                animTimer     = 0.f;
            }
            break;
        }
        }

        return { displayRow, displayFrame, posX, posY };
    }

private:
    float minX = 0, maxX = 0;
    float posX = 0, posY = 0, groundY = 0, velY = 0;
    float totalTime = 0;

    State state         = State::Stand;
    float stateTimer    = 0;
    float stateDuration = 0;
    int   animFrame     = 0;
    float animTimer     = 0;
    float faintTimer    = 0;
    float fallenTimer   = 0;

    StrokeDetector stroke;
};
