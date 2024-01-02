#pragma once
#include "game.hpp"

class connecting_t : public gameState_t {
public:
	connecting_t() 
		: text(game->GetFont()) {
		if (!game->IsHost()) {
			text.setString("Connecting...\n");
		} else {
			text.setString("Waiting for connection...\n");
		}

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

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::connecting;  }
private:
	sf::Text text;
};

class connectionFailed_t : public gameState_t {
public:
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
	virtual void OnUpdate() override {
		game->TransitionState<start_t>();
	}
	
	virtual void OnTick() override {}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::connected; }
private:
};

class start_t : public gameState_t {
public:
	start_t() 
		: text(game->GetFont(), "Both players must press R to start.") {
	}

	virtual void OnUpdate() override {
	}
	
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::start; }
private:
	bool changedToReady = false;
	float time = 1.0f;
	sf::Text text;
};

class play_t : public gameState_t {
public:
	virtual void OnUpdate() override {
	}
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::play; }
};

class gameOver_t : public gameState_t {
public:
	gameOver_t() 
		: text(game->GetFont(), "Game Over. Both players must\npress R to Reset") {
		sf::Vector2f center = ((sf::Vector2f)game->GetWindow().getSize() / 2.0f) - text.getLocalBounds().getCenter();
		text.setPosition(center);
	}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}
	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::gameOver; }

protected:
	sf::Text text;
};