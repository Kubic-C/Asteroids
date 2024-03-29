#pragma once

#include <asteroids/asteroids.hpp>

inline struct GameConfig {
    float playerSpeed;
    float playerFireRate;
    float playerBulletRecoilMultiplier;
    float playerBulletSpeed;
    float playerBaseHealth;
    float blinkResetTime;
    float reviveImmunityTime;
    int initialLives;
    i32 turretPrice;
    u32 maxTurrets;
    float turretPlaceCooldown;
    float turretRange;
    float timePerAsteroidSpawn;
    float timeToRemovePerAsteroidSpawn;
    u32 scorePerAsteroid;
    u32 initialAsteroidStage;
    float asteroidScalar;
    float asteroidDestroySpeedMultiplier;
    int defaultHostPort;
    float inputUPS;
    float stateUPS;
    u32 maxAsteroids;
} config;

constexpr u16 AsteroidCollisionMask = 1 << 0;
constexpr u16 PlayerCollisionMask = 1 << 1;

constexpr std::initializer_list<sf::Vector2f> playerVertices = {
    {10.0f, -10.0f},
    {-10.0f, 0.0f},
    {10.0f, 10.0f},

};

const std::initializer_list<std::vector<sf::Vector2f>> asteroidHulls = {
    { std::vector<sf::Vector2f>{sf::Vector2f(3.44514f, 8.1677f),sf::Vector2f(1.90872f, 7.87155f),sf::Vector2f(1.01001f, 7.20185f),sf::Vector2f(2.91266f, 3.48274f),sf::Vector2f(4.29551f, 2.01859f),sf::Vector2f(6.54363f, 1.11988f),sf::Vector2f(7.58272f, 1.81765f),sf::Vector2f(6.32682f, 4.1793f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(1.45631f, 8.40574f),sf::Vector2f(2.11136f, 5.96402f),sf::Vector2f(3.5638f, 3.41975f),sf::Vector2f(6.39299f, 3.4156f),sf::Vector2f(7.77364f, 4.40049f),sf::Vector2f(8.64721f, 6.88787f),sf::Vector2f(8.00778f, 7.39692f),sf::Vector2f(4.60631f, 8.00436f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(4.00693f, 8.35032f),sf::Vector2f(1.94754f, 8.06784f),sf::Vector2f(1.07031f, 3.27937f),sf::Vector2f(1.09058f, 1.74099f),sf::Vector2f(5.58437f, 1.18995f),sf::Vector2f(7.72115f, 6.16202f),sf::Vector2f(7.00677f, 7.89547f),sf::Vector2f(5.31043f, 8.15525f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(5.99356f, 8.91992f),sf::Vector2f(3.06915f, 6.66375f),sf::Vector2f(2.65484f, 5.38978f),sf::Vector2f(2.21244f, 1.50832f),sf::Vector2f(4.28965f, 2.59819f),sf::Vector2f(6.33903f, 4.09507f),sf::Vector2f(6.87298f, 5.25965f),sf::Vector2f(7.01239f, 7.6274f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(6.22794f, 7.04682f),sf::Vector2f(3.00665f, 5.22523f),sf::Vector2f(2.24442f, 4.66784f),sf::Vector2f(2.53081f, 2.42314f),sf::Vector2f(3.24812f, 2.65215f),sf::Vector2f(7.57344f, 5.03137f),sf::Vector2f(7.53755f, 6.4218f),sf::Vector2f(6.54534f, 6.94037f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(5.63466f, 8.12131f),sf::Vector2f(2.92633f, 7.91403f),sf::Vector2f(2.56792f, 7.29658f),sf::Vector2f(4.7738f, 3.17512f),sf::Vector2f(5.86172f, 3.53499f),sf::Vector2f(8.00143f, 4.89196f),sf::Vector2f(8.86377f, 5.92276f),sf::Vector2f(6.78289f, 7.47505f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(5.40126f, 8.52855f),sf::Vector2f(1.01294f, 6.66399f),sf::Vector2f(3.30232f, 2.1558f),sf::Vector2f(5.8019f, 1.48805f),sf::Vector2f(7.67696f, 4.66222f),sf::Vector2f(6.77728f, 6.86468f),sf::Vector2f(5.91079f, 8.41551f),sf::Vector2f(5.66811f, 8.49583f)} },
    { std::vector<sf::Vector2f>{sf::Vector2f(3.46638f, 8.70434f),sf::Vector2f(2.51958f, 5.72524f),sf::Vector2f(3.0589f, 2.28153f),sf::Vector2f(5.03308f, 2.58745f),sf::Vector2f(5.56972f, 2.95221f),sf::Vector2f(6.56609f, 7.66304f),sf::Vector2f(6.56975f, 8.12644f),sf::Vector2f(5.44032f, 8.37034f)} },
};

enum InputFlagBits : u8 {
    UP = 1 << 0,
    RIGHT = 1 << 1,
    LEFT = 1 << 2,
    DOWN = 1 << 3,
    READY = 1 << 4,
    FIRE = 1 << 5,
    PLACE_TURRET = 1 << 6
};