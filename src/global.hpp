#pragma once
#include "base.hpp"

std::vector<sf::Vector2f> generateRandomConvexShape(int size, float scale);
std::vector<sf::Vector2f> getRandomPregeneratedConvexShape(float scale);

inline float randomFloat() {
	int32_t num = rand();
	num &= ~- 0;

	// puts the rand float in between the range of 0.0f and 1.0f
	float randFloat = (float)num / (float)RAND_MAX;
	return randFloat;
}

struct Global;
inline std::shared_ptr<Global> global = nullptr;

struct Global {
	Global()
		: destroyPlayer(res.destroySound), getNoobPlayer(res.getNoobSound) {}

	bool loadResources() {
		if (!res.music.openFromFile("./res/arcadeMusic.mp3")) {
			return false;
		}

		res.music.setLoop(true);
		ae::log("Loaded Music\n");

		if (!res.destroySound.loadFromFile("./res/destroySound.mp3")) {
			return false;
		}
		destroyPlayer.setBuffer(res.destroySound);

		if (!res.getNoobSound.loadFromFile("./res/getNoob.mp3")) {
			return false;
		}
		getNoobPlayer.setBuffer(res.getNoobSound);
		getNoobPlayer.setVolume(70);

		ae::log("Loaded Sounds\n");

		if (!res.font.loadFromFile("./res/times.ttf")) {
			return false;
		}

		ae::log("Loaded Font\n");

		return true;
	}

	struct {
		sf::Music music;
		sf::Font font;
		sf::SoundBuffer destroySound;
		sf::SoundBuffer getNoobSound;
	} res;

	sf::Sound destroyPlayer;
	sf::Sound getNoobPlayer;

	flecs::entity player;
};

enum MessageHeader: u8 {
	MESSAGE_HEADER_PLAYER_INFO = ae::MESSAGE_HEADER_CORE_LAST,
	MESSAGE_HEADER_INPUT,
	MESSAGE_HEADER_REQUEST_PLAYER_ID
};

template<typename S>
void serialize(S& s, MessageHeader& header) {
	s.value1b(header);
}

struct MessagePlayerInfo {
	sf::Color playerColor = sf::Color::Green;

	template<typename S>
	void serialize(S& s) {
		s.object(playerColor);
	}
};

inline std::pair<u8, sf::Vector2f> getInput() {
	std::pair<u8, sf::Vector2f> input;
	input.first = 0;
	
	if (!ae::getWindow().hasFocus()) {
		return input;
	}

	u8 newKeys = 0;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
		newKeys |= InputFlagBits::LEFT;
	}
	else {
		newKeys &= ~InputFlagBits::LEFT;
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
		newKeys |= InputFlagBits::RIGHT;
	}
	else {
		newKeys &= ~InputFlagBits::RIGHT;
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
		newKeys |= InputFlagBits::UP;
	}
	else {
		newKeys &= ~InputFlagBits::UP;
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
		newKeys |= InputFlagBits::DOWN;
	}
	else {
		newKeys &= ~InputFlagBits::DOWN;
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R)) {
		newKeys |= InputFlagBits::READY;
	}
	else {
		newKeys &= ~InputFlagBits::READY;
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H)) {
		newKeys |= InputFlagBits::PLACE_TURRET;
	}
	else {
		newKeys &= ~InputFlagBits::PLACE_TURRET;
	}

	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
		newKeys |= InputFlagBits::FIRE;
	}
	else {
		newKeys &= ~InputFlagBits::FIRE;
	}

	input.first = newKeys;
	input.second = (sf::Vector2f)sf::Mouse::getPosition(ae::getWindow());
	return input;
}

struct MessageInput {
	MessageInput() : keys(0) {}
	MessageInput(const std::pair<u8, sf::Vector2f>& input) : keys(input.first), mouse(input.second) {}
	MessageInput(u8 i, sf::Vector2f mouse) : keys(i), mouse(mouse) {}

	u8 keys;
	sf::Vector2f mouse;

	template<typename S>
	void serialize(S& s) {
		s.value1b(keys);
		s.object(mouse);
	}
};