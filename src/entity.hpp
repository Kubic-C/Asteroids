#pragma once
#include "base.hpp"
#include "physics.hpp"

enum inputFlagBits_t : u8_t {
    UP = 1 << 0,
    RIGHT = 1 << 1,
    LEFT = 1 << 2,
    DOWN = 1 << 3,
    READY = 1 << 4,
    FIRE = 1 << 5,
    PLACE_TURRET = 1 << 6
};

// is your component networked???
struct networked_t {
    bool dirty = true;
};

struct transform_t : public networked_t {
public:
    sf::Vector2f GetUnweightedPos() { return pos; }
    sf::Vector2f GetPos() { return pos + origin; }
    void SetPos(sf::Vector2f pos) { this->pos = pos - origin; dirty = true; }

    float GetRot() { return rot; }
    void SetRot(float rot) { this->rot = rot; dirty = true; }

    sf::Vector2f GetOrigin() { return origin; } 
    void SetOrigin(sf::Vector2f origin) { this->origin = origin; dirty = true; }

    template<typename S>
    void serialize(S& s) {
        s.object(pos);
        s.value4b(rot);
        s.object(origin);
    }

private:
	sf::Vector2f pos = {0.0f, 0.0f};
	float rot = 0.0f;
    sf::Vector2f origin = {0.0f, 0.0f};
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
	float health = 1.0f;
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
    bool IsTurretPlacePressed() { return keys & inputFlagBits_t::PLACE_TURRET; }
    void SetKeys(u8_t keys) { this->keys = keys; dirty = true; }
    sf::Vector2f GetMouse() { return mouse; }
    void SetMouse(sf::Vector2f mouse) { this->mouse = mouse; dirty = true; }

    float GetLastFired() { return lastFired; }
    float GetLastBlink() { return lastBlink; }
    float GetTimeTillRevive() { return timeTillRevive; }
    float GetTurretPlaceCooldown() { return turretCooldown; }

    void ResetLastFired() { lastFired = 0.0f; }
    void ResetLastBlink() { lastBlink = blinkResetTime; }
    void ResetTimeTillRevive() { timeTillRevive = reviveImmunityTime; }
    void ResetTurretPlaceCooldown() { turretCooldown = turretPlaceCooldown; }

    void AddTimer(float deltaTime, bool isDead) {
        lastBlink -= deltaTime;
        lastFired += deltaTime;
        turretCooldown -= deltaTime;
        if(isDead)
            timeTillRevive -= deltaTime;
    }

    void SetIsFiring(bool fire) { isFiring = fire; }
    bool IsFiring() { return isFiring; }

    void SetIsReady(bool ready) { this->ready = ready; dirty = true;}
    bool IsReady() { return ready; }

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
    float turretCooldown = 0.0f;

};

struct asteroidComp_t : public networked_t {
	u8_t stage = initialAsteroidStage;

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
    u32_t lives = initialLives;

    template<typename S>
    void serialize(S& s) {
        s.value4b(lives);
    }
};

struct score_t : public networked_t {
public:
    void RemoveScore(i32_t amount) { score -= amount; dirty = true; }
    void AddScore(i32_t amount) { score += amount; dirty = true; }
    i32_t GetScore() const { return score; }
    void ResetScore() { score = 0; dirty = true;}

    template<typename S>
    void serialize(S& s) {
        s.value4b(score);
    }

private:
    i32_t score = 0;
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
 
struct turretComp_t : public networked_t {
public:
    void AddTimer(float deltaTime) {
        lastFired -= deltaTime;
    }

    float GetLastFired() { return lastFired; }
    void ResetLastFired() { lastFired = playerFireRate; }


    template<typename S>
    void serialize(S& s) {
        s.value4b(lastFired);
    }

private:
    float lastFired = 0.0f;
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

struct timedDelete_t {
    float timer = 0.0f;
};

struct deferDelete_t {};