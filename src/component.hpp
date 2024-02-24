#pragma once
#include "base.hpp"

struct HealthComponent : public ae::NetworkedComponent {
public:
    float getHealth() const { return health; }
    void setHealth(float health) { this->health = health;}

    bool isDestroyed() const { return destroyed; }
    void setDestroyed(bool destroyed) { this->destroyed = destroyed; }

    template<typename S>
    void serialize(S& s) {
        s.value4b(health);
        s.value1b(destroyed);
    }

private:
	float health = 1.0f;
    bool destroyed = false;
};

struct ColorComponent : public ae::NetworkedComponent {
public:
    ColorComponent() = default;
    explicit ColorComponent(const sf::Color& newColor)
        : color(newColor) {}

    void setColor(sf::Color color) { this->color = color; }
    sf::Color getColor() { return color; }

    template<typename S>
    void serialize(S& s) {
        s.object(color);
    }

private:
	sf::Color color;
};

struct PlayerColorComponent : public ae::NetworkedComponent {
public:
    void setColor(sf::Color color) { this->color = color; }
    sf::Color getColor() { return color; }

    template<typename S>
    void serialize(S& s) {
        s.object(color);
    }

private:
    sf::Color color;
};

struct HostPlayerComponent {};

struct PlayerComponent : public ae::NetworkedComponent {
public:
    bool isUpPressed() { return keys & InputFlagBits::UP; }
    bool isDownPressed() { return keys & InputFlagBits::DOWN; }
    bool isLeftPressed() { return keys & InputFlagBits::LEFT; }
    bool isRightPressed() { return keys & InputFlagBits::RIGHT; }
    bool isReadyPressed() { return keys & InputFlagBits::READY; }
    bool isFirePressed() { return keys & InputFlagBits::FIRE; }
    bool isTurretPlacePressed() { return keys & InputFlagBits::PLACE_TURRET; }
    void setKeys(u8 keys) { this->keys = keys; }
    sf::Vector2f getMouse() { return mouse; }
    void setMouse(sf::Vector2f mouse) { this->mouse = mouse; }

    float getLastFired() { return lastFired; }
    float getLastBlink() { return lastBlink; }
    float getTimeTillRevive() { return timeTillRevive; }
    float getTurretPlaceCooldown() { return turretCooldown; }

    void resetLastFired() { lastFired = 0.0f; }
    void resetLastBlink() { lastBlink = blinkResetTime; }
    void resetTimeTillRevive() { timeTillRevive = reviveImmunityTime; }
    void resetTurretPlaceCooldown() { turretCooldown = turretPlaceCooldown; }

    void addTimer(float deltaTime, bool isDead) {
        lastBlink -= deltaTime;
        lastFired += deltaTime;
        turretCooldown -= deltaTime;
        if(isDead)
            timeTillRevive -= deltaTime;
    }

    void setIsFiring(bool fire) { isFiring_ = fire; }
    bool isFiring() { return isFiring_; }

    void setIsReady(bool ready) { this->ready = ready;}
    bool isReady() { return ready; }

    template<typename S>
    void serialize(S& s) {
        s.value1b(keys);
        s.object(mouse);
        s.value1b(ready);
    }

private:
	u8 keys = 0;
	sf::Vector2f mouse;
	bool ready = false;
	bool isFiring_ = false;
	float timeTillRevive = 5.0f;
	float lastBlink = 0.0f;
	float lastFired = 0.0f;
    float turretCooldown = 0.0f;

};

struct AsteroidComponent : public ae::NetworkedComponent {
	u8 stage = initialAsteroidStage;

    template<typename S>
    void serialize(S& s) {
        s.value1b(stage);
    }
};

struct BulletComponent : public ae::NetworkedComponent {
    float damage = 10.0f;

    template<typename S>
    void serialize(S& s) {
        s.value4b(damage);
    }
};

// lives are shared between all players
struct SharedLivesComponent : public ae::NetworkedComponent {
    u32 lives = initialLives;

    template<typename S>
    void serialize(S& s) {
        s.value4b(lives);
    }
};

struct ScoreComponent : public ae::NetworkedComponent {
public:
    void removeScore(i32 amount) { score -= amount; }
    void addScore(i32 amount) { score += amount; }
    i32 getScore() const { return score; }
    void resetScore() { score = 0; }

    template<typename S>
    void serialize(S& s) {
        s.value4b(score);
    }

private:
    i32 score = 0;
};

struct MapSizeComponent : public ae::NetworkedComponent {
public:
    void setSize(float newWidth, float newHeight) { this->width = newWidth; this->height = newHeight; }
    void setSize(sf::Vector2u s) { this->width = (float)s.x; this->height = (float)s.y; }
    sf::Vector2f getSize() { return {width, height}; }
    float getWidth() { return width; }
    float getHeight() { return height; }

    template<typename S>
    void serialize(S& s) {
        s.value4b(width);
        s.value4b(height);
    }

private:
    float width = 0.0f;
    float height = 0.0f;

};
 
struct TurretComponent : public ae::NetworkedComponent {
public:
    void addTimer(float deltaTime) {
        lastFired -= deltaTime;
    }

    float getLastFired() { return lastFired; }
    void resetLastFired() { lastFired = playerFireRate; }


    template<typename S>
    void serialize(S& s) {}

private:
    float lastFired = 0.0f;
};

struct AsteroidTimerComponent {
    float resetTime = timePerAsteroidSpawn;
    float current = timePerAsteroidSpawn;
};

namespace prefabs {
    struct Player {};
    struct Asteroid {};
    struct Turret {};
    struct Bullet {};
}