#pragma once
#include "game.hpp"

class connecting_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
		}
	};

	connecting_t()
		: text(game->GetFont()) {
		if (!game->IsHost()) {
			text.setString("Connecting...\n");
		}
		else {
			text.setString("Waiting for connection...\n");
		}
	}

	virtual void OnTransition() override {
		sf::Vector2f center = ((sf::Vector2f)game->GetWindow().getSize() / 2.0f) - text.getLocalBounds().getCenter();
		text.setPosition(center);

#ifdef NDEBUG
		game->PlayMainTrack();
#endif
	}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}

	virtual void OnTick() override {}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::connecting; }
private:
	sf::Text text;
};

class connectionFailed_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
		}
	};

	connectionFailed_t()
		: timeLeft(5.0f), text(game->GetFont(), "Connection Failed!") {}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}

	virtual void OnTick() override {
		timeLeft -= game->GetDeltaTime();
		if (timeLeft <= 0.0f) {
			game->MarkExit();
		}
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::connectionFailed; }
private:
	float timeLeft;
	sf::Text text;
};

class start_t;
class play_t;
class gameOver_t;

class connected_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
		}
	};

	connected_t() {}

	virtual void OnUpdate() override {
		game->TransitionState(gameStateEnum_t::start);
	}

	virtual void OnTick() override {}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::connected; }
private:
};

void UpdatePlayerReady(flecs::iter& iter, playerComp_t* players, color_t* colors);
void IsAllPlayersReady(flecs::iter& iter);
void OrientPlayers(flecs::iter& iter, transform_t* transforms);

class start_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			if(world.has<isHost_t>()) {
				world.system<playerComp_t, color_t>().kind(flecs::OnUpdate).iter(UpdatePlayerReady);
				world.system().kind(flecs::PostUpdate).iter(IsAllPlayersReady);
				world.system<transform_t>().with<playerComp_t>().iter(OrientPlayers);
			}
		}
	};

	start_t()
		: text(game->GetFont(), "Both players must press R to start.") {
	}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}

	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::start; }
private:
	sf::Text text;
};

void UpdatePlayerDead(flecs::iter& iter, health_t* health);
void IsAllPlayersDead(flecs::iter& iter);
void IsDead(flecs::iter& iter, health_t* healths);
void PlayerPlayInputUpdate(flecs::iter& iter, playerComp_t* players, integratable_t* integratables, transform_t* transforms);
void PlayerBlinkUpdate(flecs::iter& iter, playerComp_t* players, health_t* healths, color_t* colors);
void PlayerReviveUpdate(flecs::iter& iter, sharedLives_t* lives, playerComp_t* players, health_t* healths);
void AsteroidDestroyUpdate(flecs::iter& iter, asteroidComp_t*, health_t* healths);
void TransformWrap(flecs::iter& iter, mapSize_t* size, transform_t* transforms);
void AsteroidAddUpdate(flecs::iter& iter, physicsWorldComp_t* physicsWorld, mapSize_t* mapSize, asteroidTimer_t* timer);
void ObservePlayerCollision(flecs::iter& iter, size_t i, shapeComp_t&);
void ObserveBulletCollision(flecs::iter& iter, size_t i, shapeComp_t&);

class play_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			world.system<health_t>().kind(flecs::OnUpdate).iter(UpdatePlayerDead);
			world.system().kind(flecs::PostUpdate).iter(IsAllPlayersDead);
			world.system<health_t>().iter(IsDead);
			world.system<mapSize_t, transform_t>().term_at(1).singleton().iter(TransformWrap);
			world.system<playerComp_t, integratable_t, transform_t>().iter(PlayerPlayInputUpdate);
			world.system<playerComp_t, health_t, color_t>().iter(PlayerBlinkUpdate);
			world.system<sharedLives_t, playerComp_t, health_t>().term_at(1).singleton().iter(PlayerReviveUpdate);
			world.system<physicsWorldComp_t, mapSize_t, asteroidTimer_t>().term_at(1).singleton().term_at(2).singleton().term_at(3).singleton().iter(AsteroidAddUpdate);
			world.system<asteroidComp_t, health_t>().iter(AsteroidDestroyUpdate);
			world.observer<shapeComp_t>().event<collisionEvent_t>().with<playerComp_t>().each(ObservePlayerCollision);
			world.observer<shapeComp_t>().event<collisionEvent_t>().with<bulletComp_t>().each(ObserveBulletCollision);
		}
	};

	play_t() {}

	virtual void OnUpdate() override {
	}
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::play; }

public:
	void AddDeadCount() { deadCount++; }
	void ResetDeadCount() { deadCount = 0; }
	int GetDeadCount() { return deadCount; }

private:
	int deadCount = 0;
};

class gameOver_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			// re-using systems from the start module
			if (world.has<isHost_t>()) {
				world.system<playerComp_t, color_t>().kind(flecs::OnUpdate).iter(UpdatePlayerReady);
				world.system().kind(flecs::PostUpdate).iter(IsAllPlayersReady);
				world.system<transform_t>().with<playerComp_t>().iter(OrientPlayers);
			}
		}
	};

	gameOver_t()
		: text(game->GetFont(), "Game Over. Both players must\npress R to Reset") {

		destroyAsteroids = game->GetWorld().query<asteroidComp_t>();
		destroyBullets = game->GetWorld().query<bulletComp_t>();
		resetPlayers = game->GetWorld().query<playerComp_t, integratable_t, health_t>();
	}

	virtual void OnTransition() override {
		sf::Vector2f center = ((sf::Vector2f)game->GetWindow().getSize() / 2.0f) - text.getLocalBounds().getCenter();
		text.setPosition(center);

		destroyAsteroids.each([](flecs::entity e, asteroidComp_t& asteroid) {
			e.add<deferDelete_t>();
		});
		destroyBullets.each([](flecs::entity e, bulletComp_t& asteroid) {
			e.add<deferDelete_t>();
		});
		resetPlayers.each([](playerComp_t& player, integratable_t& integratable, health_t& health) {
			player.SetIsReady(false);
			integratable.AddLinearVelocity(-integratable.GetLinearVelocity());
			health.SetDestroyed(false);
			health.SetHealth(1.0f);
		});
	}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::gameOver; }

protected:
	flecs::query<asteroidComp_t> destroyAsteroids;
	flecs::query<bulletComp_t> destroyBullets;
	flecs::query<playerComp_t, integratable_t, health_t> resetPlayers;
	sf::Text text;
};

class unknown_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
		}
	};

	virtual void OnUpdate() override {
	}
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::unknown; }
};