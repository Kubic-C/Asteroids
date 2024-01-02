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
    bool dirty = false;
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
    void ResetLastBlink() { lastBlink = 0.5f; }
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
    u32_t shape;

    template<typename S>
    void serialize(S& s) {
        s.value4b(shape);
    }
};

struct gameWindow_t {
    std::shared_ptr<sf::RenderWindow> window;
};

struct isHost_t {};

inline void Integrate(flecs::iter& iter, transform_t* transforms, integratable_t* integratables) {
	for(auto i : iter) {
		transform_t& transform = transforms[i];
		integratable_t& integratable = integratables[i];
		
        transform.SetPos(transform.GetPos() + integratable.GetLinearVelocity() * iter.delta_time());
		transform.SetRot(transform.GetRot() + integratable.GetAngularVelocity() * iter.delta_time());
		
        if(integratable.IsSameAsLast()) {
            transform.dirty = false;
            integratable.dirty = false;
        }
        
	}
}

inline void IsDead(flecs::iter& iter, health_t* healths) {
	for(auto i : iter) {
		if(healths[i].GetHealth() < 0.0f) {
			healths[i].SetDestroyed(true);
		}
	}
}

inline void PlayerPlayInputUpdate(flecs::iter& iter, playerComp_t* players, integratable_t* integratables, transform_t* transforms) {
	float deltaTime = iter.delta_time();
    
    for(auto i : iter) {
        playerComp_t& player = players[i];
        integratable_t& integratable = integratables[i];
        transform_t& transform = transforms[i];

        player.AddTimer(deltaTime);

        if(player.GetMouse() != sf::Vector2f())
            transform.SetRot((transform.GetPos() - player.GetMouse()).angle().asRadians());

        if (player.IsUpPressed()) {
            integratable.AddLinearVelocity({0.0f, -playerSpeed});
        }
        if (player.IsDownPressed()) {
            integratable.AddLinearVelocity({ 0.0f, playerSpeed });
        }
        if (player.IsLeftPressed()) {
            integratable.AddLinearVelocity({ -playerSpeed, 0.0f });
        }
        if (player.IsRightPressed()) {
            integratable.AddLinearVelocity({ playerSpeed, 0.0f });
        }

        if (player.IsFirePressed() && player.GetLastFired() > playerFireRate) {
            player.ResetLastFired();
            player.SetIsFiring(true);
        }
        else {
            player.SetIsFiring(false);
        }
	}
}

inline void PlayerBlinkUpdate(flecs::iter& iter, playerComp_t* players, health_t* healths, color_t* colors) {
    for(auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];
        color_t& color = colors[i];

        std::cout << "HAS HEALTH UPDATE\n";

        if (player.GetLastBlink() <= 0.0f && healths->IsDestroyed()) {
            player.ResetLastBlink();
            color.SetColor(sf::Color::Cyan);
        }
        else {
            //color.SetColor(sf::Color::Green);
        }
    }
}

inline void PlayerReviveUpdate(flecs::iter& iter, sharedLives_t* lives, playerComp_t* players, health_t* healths) {
    for(auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];

        if (health.IsDestroyed() && lives->lives != 0) {
            if (player.GetTimeTillRevive() <= 0.0f) {
                player.ResetTimeTillRevive();
                health.SetDestroyed(false);
            }
        }
    }
}

inline void TransformWrap(flecs::iter& iter, mapSize_t* size, transform_t* transforms) {
    for(auto i : iter) {
        transform_t& transform = transforms[i];
        sf::Vector2f pos = transform.GetPos();

        if (pos.x >= size->GetWidth()) {
            pos.x = 0.0f;
        } else if (pos.x <= 0.0f) {
            pos.x = size->GetWidth();
        }

        if (pos.y >= size->GetHeight()) {
            pos.y = 0.0f;
        } else if (pos.y <= 0.0f) {
            pos.y = size->GetHeight();
        }

        if(pos != transform.GetPos())
            transform.SetPos(pos);
    }
}

inline void ShapeUpdate(flecs::iter& iter, transform_t* transforms, shapeComp_t* shapes) {
    for(auto i : iter) {
        transform_t& transform = transforms[i];
        shapeComp_t& shape = shapes[i];


        // PHYSISICSICSICISI
        //shape.shape->SetPos(transform.pos);
        //shape.shape->SetRot(transform.rot);
    }
}

inline void HostPlayerInputUpdate(flecs::iter& iter, gameWindow_t* window, playerComp_t* players) {
    for(auto i : iter) {
        playerComp_t& player = players[i];

        player.SetMouse((sf::Vector2f)sf::Mouse::getPosition(*window->window));
        
        if(!window->window->hasFocus())
            continue;

        uint8_t keys = 0;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
            keys |= inputFlagBits_t::LEFT;
        } else {
            keys &= ~inputFlagBits_t::LEFT;
        }
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
            keys |= inputFlagBits_t::RIGHT;
        } else {
            keys &= ~inputFlagBits_t::RIGHT;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
            keys |= inputFlagBits_t::UP;
        } else {
            keys &= ~inputFlagBits_t::UP;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
            keys |= inputFlagBits_t::DOWN;
        } else {
            keys &= ~inputFlagBits_t::DOWN;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::R)) {
            keys |= inputFlagBits_t::READY;
        } else {
            keys &= ~inputFlagBits_t::READY;
        }
        if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
           keys |= inputFlagBits_t::FIRE;
        } else {
            keys &= ~inputFlagBits_t::FIRE;
        }

        player.SetKeys(keys);
    }
}

inline void ImportComponents(flecs::world& world) {
    world.component<transform_t>().is_a<networked_t>();
    world.component<health_t>().is_a<networked_t>();
    world.component<integratable_t>().is_a<networked_t>();
    world.component<color_t>().is_a<networked_t>();
    world.component<playerComp_t>().is_a<networked_t>();
    world.component<asteroidComp_t>().is_a<networked_t>();
    world.component<sharedLives_t>().is_a<networked_t>();
    world.component<bulletComp_t>().is_a<networked_t>();
    world.component<mapSize_t>().is_a<networked_t>();
    world.component<shapeComp_t>().is_a<networked_t>();
}

inline void ImportSystems(flecs::world& world) {
   world.system<transform_t, integratable_t>().iter(Integrate);
   world.system<health_t>().iter(IsDead);
   world.system<playerComp_t, integratable_t, transform_t>().iter(PlayerPlayInputUpdate);
   world.system<playerComp_t, health_t, color_t>().iter(PlayerBlinkUpdate);
   world.system<sharedLives_t, playerComp_t, health_t>().term_at(1).singleton().iter(PlayerReviveUpdate);
   world.system<mapSize_t, transform_t>().term_at(1).singleton().iter(TransformWrap);
   world.system<transform_t, shapeComp_t>().iter(ShapeUpdate);
   world.system<gameWindow_t, playerComp_t>().term_at(1).singleton().term<hostPlayer_t>().iter(HostPlayerInputUpdate);
}
