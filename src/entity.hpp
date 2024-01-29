#pragma once
#include "base.hpp"
#include "physics.hpp"

enum inputFlagBits_t : u8_t {
    UP = 1 << 0,
    RIGHT = 1 << 1,
    LEFT = 1 << 2,
    DOWN = 1 << 3,
    READY = 1 << 4,
    FIRE = 1 << 5
};

// is your component networked???
struct networked_t {
    bool dirty = true;
};

struct transform_t : public networked_t {
public:
    sf::Vector2f GetPos() { return pos; }
    void SetPos(sf::Vector2f pos) { this->pos = pos; dirty = true; }

    float GetRot() { return rot; }
    void SetRot(float rot) { this->rot = rot; dirty = true; }

    template<typename S>
    void serialize(S& s) {
        s.object(pos);
        s.value4b(rot);
    }

private:
	sf::Vector2f pos = {0.0f, 0.0f};
	float rot = 0.0f;
};

struct health_t : public networked_t {
public:
    float GetHealth() { return health; }
    void SetHealth(float health) { this->health = health; dirty = true;}

    bool IsDestroyed() { return destroyed; }
    void SetDestroyed(bool destroyed) { this->destroyed = destroyed; dirty = true; }

    template<typename S>
    void serialize(S& s) {
        s.value4b(health);
        s.value1b(destroyed);
    }

private:
	float health = 0.0f;
    bool destroyed = false;
};

struct integratable_t : public networked_t {
public:
    sf::Vector2f GetLinearVelocity() { return linearVelocity; }

    void AddLinearVelocity(sf::Vector2f vel) {
        SetLast();
        linearVelocity += vel;
        dirty = true;
    }

    float GetAngularVelocity() { return angularVelocity; }

    template<typename S>
    void serialize(S& s) {
        s.object(linearVelocity);
        s.value4b(angularVelocity);
    }

    bool IsSameAsLast() {
        return angularVelocity == lastAngularVelocity && lastLinearVelocity == linearVelocity;
    }

protected:
    void SetLast() {
        lastLinearVelocity = linearVelocity;
        lastAngularVelocity = angularVelocity;
    }

private:
    sf::Vector2f lastLinearVelocity = {0.0f, 0.0f};
    float lastAngularVelocity = 0.0f;
	sf::Vector2f linearVelocity = {0.0f, 0.0f};
	float angularVelocity = 0.0f;

};

struct color_t : public networked_t {
public:
    void SetColor(sf::Color color) { this->color = color; dirty = true; }
    sf::Color GetColor() { return color; }

    template<typename S>
    void serialize(S& s) {
        s.object(color);
    }

private:
	sf::Color color;

};

struct hostPlayer_t {};

struct playerComp_t : public networked_t {
public:
    bool IsUpPressed() { return keys & inputFlagBits_t::UP; }
    bool IsDownPressed() { return keys & inputFlagBits_t::DOWN; }
    bool IsLeftPressed() { return keys & inputFlagBits_t::LEFT; }
    bool IsRightPressed() { return keys & inputFlagBits_t::RIGHT; }
    bool IsReadyPressed() { return keys & inputFlagBits_t::READY; }
    bool IsFirePressed() { return keys & inputFlagBits_t::FIRE; }
    void SetKeys(u8_t keys) { this->keys = keys; dirty = true; }
    sf::Vector2f GetMouse() { return mouse; }
    void SetMouse(sf::Vector2f mouse) { this->mouse = mouse; dirty = true; }

    float GetLastFired() { return lastFired; }
    float GetLastBlink() { return lastBlink; }
    float GetTimeTillRevive() { return timeTillRevive; }

    void ResetLastFired() { lastFired = 0.0f; }
    void ResetLastBlink() { lastBlink = 1.0f; }
    void ResetTimeTillRevive() { timeTillRevive = 5.0f; }

    void AddTimer(float deltaTime) {
        timeTillRevive -= deltaTime;
        lastBlink -= deltaTime;
        lastFired += deltaTime;
    }

    void SetIsFiring(bool fire) { isFiring = fire; }
    bool IsFiring() { return isFiring; }

    template<typename S>
    void serialize(S& s) {
        s.value1b(keys);
        s.object(mouse);
        s.value1b(ready);
    }

private:
	u8_t keys = 0;
	sf::Vector2f mouse;
	bool ready = false;
	bool isFiring = false;
	float timeTillRevive = 5.0f;
	float lastBlink = 0.0f;
	float lastFired = 0.0f;

};

struct asteroidComp_t : public networked_t {
	u8_t stage = 1;

    template<typename S>
    void serialize(S& s) {
        s.value1b(stage);
    }
};

struct bulletComp_t : public networked_t {
    float damage = 10.0f;

    template<typename S>
    void serialize(S& s) {
        s.value4b(damage);
    }
};

// lives are shared between all players
struct sharedLives_t : public networked_t {
    u32_t lives = 0;

    template<typename S>
    void serialize(S& s) {
        s.value4b(lives);
    }
};

struct mapSize_t : public networked_t {
public:
    void SetSize(float width, float height) { this->width = width; this->height = height; dirty = true; }
    void SetSize(sf::Vector2u s) { this->width = (float)s.x; this->height = (float)s.y; dirty = true; }
    sf::Vector2f GetSize() { return {width, height}; }
    float GetWidth() { return width; }
    float GetHeight() { return height; }

    template<typename S>
    void serialize(S& s) {
        s.value4b(width);
        s.value4b(height);
    }

private:
    float width;
    float height;

};
 
struct shapeComp_t : public networked_t {
    u32_t shape = 0;

    template<typename S>
    void serialize(S& s) {
        s.value4b(shape);
    }
};

struct gameWindow_t {
    std::shared_ptr<sf::RenderWindow> window;
};

struct gameKeyboard_t {
    uint8_t keys;
    sf::Vector2f mouse;
};

struct asteroidTimer_t {
    float resetTime = timePerAsteroidSpawn;
    float current = timePerAsteroidSpawn;
};

struct isHost_t {};

struct deferDelete_t {};