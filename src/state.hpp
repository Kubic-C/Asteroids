#pragma once
#include "base.hpp"

enum gameStateEnum_t : u8_t {
	initialGame = 0,
	connecting,
	connectionFailed,
	connected,
	start,
	play,
	gameOver,
	unknown,
};

constexpr u32_t stateCount = (u32_t)gameStateEnum_t::unknown + 1;

template<typename S>
void serialize(S& s, gameStateEnum_t& state) {
	s.value1b(state);
}

class gameState_t {
public:
	virtual void OnTransition() {}
	virtual void OnTick() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnDeTransition() {}

	virtual gameStateEnum_t Enum() = 0;

	flecs::entity moduleEntity;
};