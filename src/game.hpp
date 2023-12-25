#pragma once
#include "base.hpp"
#include "physics.hpp"

sf::Vector2f GetMouseWorldCoords();
std::vector<sf::Vector2f> GenerateRandomConvexShape(int size, float scale);
std::vector<sf::Vector2f> GetRandomPregeneratedConvexShape(float scale);

inline float RandomFloat() {
	int32_t num = rand();
	num &= ~- 0;

	// puts the rand float in between the range of 0.0f and 1.0f
	float randFloat = (float)num / (float)RAND_MAX;
	return randFloat;
}

enum class messageHeader_t: u8_t {
	input,
	state
};

template<typename S>
void serialize(S& s, messageHeader_t header) {
	s.value1b(header);
}

enum inputFlagBits_t: u8_t {
	UP = 1 << 0,
	RIGHT = 1 << 1,
	LEFT = 1 << 2,
	DOWN = 1 << 3,
	READY = 1 << 4,
	FIRE = 1 << 5
};

struct messageInput_t {
	messageInput_t() : input(0) {}
	messageInput_t(uint8_t i, sf::Vector2f mouse) : input(i), mouse(mouse) {}

	uint8_t input;
	sf::Vector2f mouse;

	template<typename S>
	void serialize(S& s) {
		s.value1b(input);
		s.object(mouse);
	}
};

struct message_t {
	message_t() {}

	message_t(message_t&& other) noexcept {
		bytes = std::move(other.bytes);
	}

	typedef bitsery::InputBufferAdapter<std::vector<u8_t>> inputAdapter_t;
	typedef bitsery::OutputBufferAdapter<std::vector<u8_t>> outputAdapter_t;
	typedef bitsery::Deserializer<inputAdapter_t> deserializer_t;
	typedef bitsery::Serializer<outputAdapter_t> serializer_t;

	u32_t Size() { return (u32_t)bytes.size();  }
	uint8_t* Data() { return bytes.data(); }

	void SetData(u32_t size, u8_t* data) {
		bytes.resize(size);
		memcpy(bytes.data(), data, size);
	}

	serializer_t StartSerialize() {
		serializer_t ser(bytes);
		return std::move(ser);
	}

	template<typename T>
	void Serialize(serializer_t& ser, const T& input) {
		ser.object(input);
	}

	void EndSerialize(serializer_t& ser) {
		ser.adapter().flush();
		bytes.resize(ser.adapter().writtenBytesCount());
	}

	deserializer_t StartDeserialize() {
		deserializer_t des(inputAdapter_t(bytes.begin(), bytes.size()));
		return std::move(des);
	}

	template<typename T>
	void Deserialize(deserializer_t& des, T& output) {
		des.object(output);
	}

	bool EndDeserialization(deserializer_t& des) {
		return des.adapter().isCompletedSuccessfully();
	}
private:
	std::vector<u8_t> bytes;
};

class messageManagerr_t {
public:
	void Update(HSteamNetConnection peer, ISteamNetworkingUtils* utils, ISteamNetworkingSockets* sockets) {
		// Send outgoing messages
		while (!outgoing.empty()) {
			message_t message = std::move(outgoing.front());

			sockets->SendMessageToConnection(peer, message.Data(), message.Size(), k_nSteamNetworkingSend_Unreliable, nullptr);
			outgoing.pop();
		}

		// Get incoming messages;
		do {
			SteamNetworkingMessage_t* message;
			int amount = sockets->ReceiveMessagesOnConnection(peer, &message, 1);
			if(amount < 1) {
				break;
			}

			message_t new_message;
			new_message.SetData(message->GetSize(), (u8_t*)message->GetData());
			incoming.push(std::move(new_message));

			message->Release();
		} while (true);
	}

	void PushOutgoing(message_t&& message) {
		outgoing.push(std::move(message));
	}

	bool IsIncomingEmpty() {
		return incoming.empty();
	}

	message_t PopIncoming() {
		message_t message = std::move(incoming.front());
		incoming.pop();
		return message;
	}
private:
	std::queue<message_t> outgoing;
	std::queue<message_t> incoming;
};

enum class gameStateEnum_t: u8_t {
	connecting = 0,
	connectionFailed,
	connected,
	start,
	play,
	gameOver
};

template<typename S>
void serialize(S& s, gameStateEnum_t state) {
	s.value1b(state);
}

class gameState_t {
public:
	virtual void OnTick() = 0;
	virtual void OnUpdate() = 0;
	virtual gameStateEnum_t Enum() = 0;
};

class gameWorld_t;

class entity_t {
public:
	entity_t()
		: world(nullptr) {}
	entity_t(gameWorld_t* world)
		: world(world), pos(sf::Vector2f()), rot(0.0f), health(10.0f) {}
	entity_t(gameWorld_t* world, sf::Vector2f pos, float rot)
		: world(world), pos(pos), rot(rot), health(10.0f) {}

	template<typename S>
	void serialize(S& s) {
		s.value4b(rot);
		s.value4b(health);
		s.object(pos);
		s.object(vel);
		s.value1b(destroyed);
		s.object(color);
	}

	void AddVelocity(sf::Vector2f vec) {
		vel += vec;
	}

	void Move(sf::Vector2f vec) {
		pos += vec;
	}

	virtual sf::Vector2f GetPos() {
		return pos;
	}

	void SetPos(sf::Vector2f vec) {
		pos = vec;
	}
	
	sf::Color GetColor() const {
		return color;
	}

	void SetColor(sf::Color color) {
		this->color = color;
	}

	float GetRot() {
		return rot;
	}

	void Intergrate(float deltaTime) {
		pos = pos + vel * deltaTime;
	}

	void Wrap(float width, float height) {
		if (pos.x >= width) {
			pos.x = 0.0f;
		}
		else if (pos.x <= 0.0f) {
			pos.x = width;
		}

		if (pos.y >= height) {
			pos.y = 0.0f;
		}
		else if (pos.y <= 0.0f) {
			pos.y = height;
		}
	}

	virtual void Tick(gameStateEnum_t state, float deltaTime) {}
	virtual void Update(gameStateEnum_t state, float frameTime) {}

	bool IsDestroyed() {
		return destroyed;
	}

	void MarkDestroyed() {
		destroyed = true;
	}

protected:
	gameWorld_t* world;
	sf::Color color;
	bool destroyed = false;
	sf::Vector2f pos;
	sf::Vector2f vel;
	float rot;
	float health;
};

class gameWorld_t;

class player_t : public entity_t {
	friend class gameWorld_t;
public:
	player_t() {}
	player_t(gameWorld_t* world);

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<entity_t>{});
		s.value1b(input);
		s.object(mouse);
		s.value1b(ready);
		s.object(polygon);
	}

	void SetLocalControlled(bool localControlled) {
		isLocalControlled = localControlled;
	}

	void SetInput(u8_t input, sf::Vector2f mouse) {
		this->input = input;
		this->mouse = mouse;
	}

	virtual void Tick(gameStateEnum_t state, float deltaTime);
	
	virtual void Update(gameStateEnum_t state, float frameTime) {
	}

	bool IsReady() {
		return ready;
	}

	bool IsFiring() {
		return isFiring;
	}

	void ResetReady() {
		ready = false;
	}
protected:
	float timeTillRevive = 5.0f;
	float lastBlink = 0.0f;
	float lastFired = 0.0f;
	bool isLocalControlled = false; // LOCAL FLAG. DO NOT SERIALIZE
	polygon_t polygon;
	u8_t input = 0;
	bool ready = false;
	bool isFiring = false;
	sf::Vector2f mouse;
};

class asteroids_t : public entity_t {
	friend class gameWorld_t;
public:
	asteroids_t() {}
	asteroids_t(gameWorld_t* world)
		: entity_t(world) {
		SetColor(sf::Color::Red);
	}

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<entity_t>{});
		s.object(polygon);
		s.value4b(stage);
	}

	virtual void Tick(gameStateEnum_t state, float deltaTime) override {
		polygon.SetPos(pos);
		polygon.SetRot(rot);
	}

	void SetStage(int stage) {
		this->stage = stage;
	}

	int GetStage() {
		return stage;
	}

	void GenerateShape() {
		std::vector<sf::Vector2f> vertices = GetRandomPregeneratedConvexShape(8.0f * (1.0f / (float)stage));
		//std::vector<sf::Vector2f> vertices = {
		//	{-10.0f, -10.0f},
		//	{ 10.0f, -10.0f},
		//	{ 10.0f, 10.0f},
		//	{-10.0f, 10.0f}
		//};

		polygon.SetVertices(vertices.size(), vertices.data());
	}

protected:
	int stage = 1;
	polygon_t polygon;
};

class bullet_t : public entity_t {
	friend class gameWorld_t;
public:
	bullet_t() {}
	bullet_t(gameWorld_t* world) 
		: entity_t(world), circle(2.5f) {
		SetColor(sf::Color::Yellow);
	}

	virtual void Tick(gameStateEnum_t state, float deltaTime) override {
		circle.SetPos(pos);
	}

	float GetBulletDamage() {
		return bulletDamage;
	}

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<entity_t>{});
		s.object(circle);
		s.value4b(bulletDamage);
	}

protected:
	circle_t circle;
	float bulletDamage = 10.0f;
};

struct aabb_t {
	aabb_t() {
		min[0] = 0.0f;
		min[1] = 0.0f;
		max[0] = 0.0f;
		max[1] = 0.0f;
	}

	aabb_t(float width, float height) {
		min[0] = 0.0f;
		min[1] = 0.0f;
		max[0] = width;
		max[1] = height;
	}

	float min[2];
	float max[2];

	bool IsPointInside(sf::Vector2f v) {
		return min[0] <= v.x && v.x <= max[0] && min[1] <= v.y && v.y <= max[1];
	}
};

class gameWorld_t {
public:
	gameWorld_t()
		: players({player_t(this), player_t(this)}) {
		Reset();
	}

	void SetMapBoundries(float width, float height) {
		mapWidth = width;
		mapHeight = height;
	}

	void SetIsHost(bool isHost) {
		this->isHost = isHost;
	}

	template<typename S>
	void serialize(S& s) {
		s.value4b(score);
		s.container(players);
		s.container(asteroids, UINT32_MAX);
		s.container(bullets, UINT32_MAX);
	}

	enum playerIndex_t {
		HOST = 0,
		CLIENT = 1,
		INVALID = 32
	};

	void SetInput(u8_t input, sf::Vector2f mouse) {
		if (isHost) {
			players[HOST].SetLocalControlled(true);
			players[HOST].SetInput(input, mouse);
		} else {
			players[CLIENT].SetLocalControlled(true);
			players[CLIENT].SetInput(input, mouse);
		}
	}

	void CheckCollision();

	u8_t GetTotalLives() {
		return lives;
	}

	// Used by host only vvv 
	void SetClientInput(messageInput_t input) {
		assert(isHost);
		players[CLIENT].SetInput(input.input, input.mouse);
	}

	void Tick(gameState_t* state, float deltaTime) {
		SpawnAsteroidWhenReady(state->Enum(), deltaTime);
		CheckCollision();
		for(u32_t i = 0; i < bullets.size(); i++) {
			bullets[i].Intergrate(deltaTime);
			bullets[i].Tick(state->Enum(), deltaTime);
			bullets[i].Wrap(mapWidth, mapHeight);
		}

		for (u8_t i = 0; i < 2; i++) {
			players[i].Intergrate(deltaTime);
			players[i].Tick(state->Enum(), deltaTime);
			players[i].Wrap(mapWidth, mapHeight);

			if(players[i].IsFiring()) {
				sf::Vector2f dir = sf::Vector2f(-1.0f, 0.0f); 
				dir = dir.rotatedBy(sf::radians(players[i].GetRot()));
				AddBullet(players[i].GetPos(), dir);

			}
		}

		for (asteroids_t& asteroid : asteroids) {
			asteroid.Intergrate(deltaTime);
			asteroid.Tick(state->Enum(), deltaTime);
			asteroid.Wrap(mapWidth, mapHeight);
		}
	}

	void RenderPolygon(sf::RenderWindow& window, sf::Color color, polygon_t& polygon) {
		sf::ConvexShape shape(polygon.GetVerticeCount());
		shape.setFillColor(color);
		for(u8_t i = 0; i < polygon.GetVerticeCount(); i++) {
			shape.setPoint(i, polygon.GetWorldVertices()[i]);
		}

		window.draw(shape);
	}

	void Render(sf::RenderWindow& window);

	bool IsBothReady() {
		return players[HOST].IsReady() && players[CLIENT].IsReady();
	}

	bool IsBothDestroyed() {
		return players[HOST].IsDestroyed() && players[CLIENT].IsDestroyed();
	}

	void SpawnAsteroidWhenReady(gameStateEnum_t state, float deltaTime) {
		if (!isHost ||state != gameStateEnum_t::play)
			return;

		timeToSpawnAsteroid -= deltaTime;
		if (timeToSpawnAsteroid <= 0.0f) {
			// although we are subtracting, we are making it faster
			asteroidSpawnRate -= timeToRemovePerAsteroidSpawn;
			timeToSpawnAsteroid = asteroidSpawnRate;

			float spawnX = (RandomFloat() * mapWidth);
			float spawnY = (RandomFloat() * mapHeight);
			
			float wallX = 0.0f;
			float distX = 0.0f;
			float distToLeft = spawnX;
			float distToRight = mapWidth - spawnX;
			if (distToLeft < distToRight) {
				wallX = mapWidth - 0.01f;
				distX = distToRight;
			} else {
				distX = distToLeft;
			}

			float wallY = 0.0f;
			float distY = 0.0f;
			float distToUp = spawnY;
			float distToDown = mapHeight - spawnY;
			if (distToUp < distToDown) {
				wallY = mapHeight - 0.1f;
				distY = distToDown;
			} else {
				distY = distToUp;
			}

			if (distX < distY) {
				spawnX = wallX;
			} else {
				spawnY = wallY;
			}

			auto& asteroid = asteroids.emplace_back(this);
			asteroid.GenerateShape();
			asteroid.SetPos({ spawnX, spawnY });

			sf::Vector2f center = sf::Vector2f(mapWidth, mapHeight) / 2.0f;
			sf::Vector2f velToCenter = (asteroid.GetPos() - center).normalized();
			asteroid.AddVelocity(velToCenter * 10.0f);
		}
	}

	void AddBullet(sf::Vector2f start, sf::Vector2f dir) {
		bullet_t& bullet = bullets.emplace_back(bullet_t(this));
		bullet.SetPos(start);
		bullet.AddVelocity(dir * 100.0f);
	}

	void GameEnd() {
		asteroidSpawnRate = 10000.0f;
		players[HOST].ResetReady();
		players[CLIENT].ResetReady();
	}

	void Reset() {
		mapWidth = windowWidth;
		mapHeight = windowHeight;
		
		sf::Vector2f center = sf::Vector2f(mapWidth, mapHeight) / 2.0f;

		center.x -= 50;
		center.y += 50;
		players[HOST].SetPos(center);
		players[HOST].rot = 3.14159f;
		center.x += 100;
		players[CLIENT].SetPos(center);

		players[HOST].vel = { 0.0f, 0.0f };
		players[CLIENT].vel = {0.0f, 0.0f};

		players[HOST].ResetReady();
		players[CLIENT].ResetReady();
		players[HOST].destroyed = false;
		players[CLIENT].destroyed = false;

		asteroidSpawnRate = timePerAsteroidSpawn;
		timeToSpawnAsteroid = asteroidSpawnRate;
		lives = 3;
		mapWidth = 0.0f;
		mapHeight = 0.0f;

		asteroids.clear();
		bullets.clear();
	}

private:
	float asteroidSpawnRate = timePerAsteroidSpawn;
	float timeToSpawnAsteroid = asteroidSpawnRate;

	u8_t lives = 3;
	float mapWidth = 0.0f;
	float mapHeight = 0.0f;
	bool isHost = false;
	u32_t score;
	std::array<player_t, 2> players;
	std::vector<asteroids_t> asteroids;
	std::vector<bullet_t> bullets;
};

inline struct game_t {
	game_t() 
		: destroyPlayer(destroySound), getNoobPlayer(getNoobSound) {
		
	}

	bool LoadResources() {
		if(!music.openFromFile("./arcadeMusic.mp3")) {
			return false;
		}

		music.setLoop(true);
		std::cout << "Loaded Music\n";

		if(!destroySound.loadFromFile("./destroySound.mp3")) {
			return false;
		}
		destroyPlayer.setBuffer(destroySound);

		if (!getNoobSound.loadFromFile("./getNoob.mp3")) {
			return false;
		}
		getNoobPlayer.setBuffer(getNoobSound);
		game.getNoobPlayer.setVolume(70);

		std::cout << "Loaded Sounds\n";

		if (!SetFont("./times.ttf")) {
			return false;
		}

		std::cout << "Loaded Font\n";
		
		return true;
	}

	bool SetFont(std::string path) {
		return font.loadFromFile(path);
	}

	void CreateWindow(u32_t w, u32_t h, std::string title) {
		window = new sf::RenderWindow(sf::VideoMode({ w, h }), title);
	}

	void Update() {
		sockets->RunCallbacks();

		sf::Event event;
		if (window->pollEvent(event)) {
			// handle window events ...
			switch (event.type) {
			case event.Closed:
				exit = true;
				break;
			case event.MouseButtonPressed:
				if(event.mouseButton.button == sf::Mouse::Button::Left) {
					input |= inputFlagBits_t::FIRE;
				}
				break;
			case event.MouseButtonReleased:
				if (event.mouseButton.button == sf::Mouse::Button::Left) {
					input &= ~inputFlagBits_t::FIRE;
				}
				break;

			case event.KeyPressed:
				if (event.key.code == sf::Keyboard::A) {
					input |= inputFlagBits_t::LEFT;
				}
				if (event.key.code == sf::Keyboard::D) {
					input |= inputFlagBits_t::RIGHT;
				}
				if (event.key.code == sf::Keyboard::W) {
					input |= inputFlagBits_t::UP;
				}
				if (event.key.code == sf::Keyboard::S) {
					input |= inputFlagBits_t::DOWN;
				}
				if (event.key.code == sf::Keyboard::R) {
					input |= inputFlagBits_t::READY;
				}
				break;
			case event.KeyReleased:
				if (event.key.code == sf::Keyboard::A) {
					input &= ~inputFlagBits_t::LEFT;
				}
				if (event.key.code == sf::Keyboard::D) {
					input &= ~inputFlagBits_t::RIGHT;
				}
				if (event.key.code == sf::Keyboard::W) {
					input &= ~inputFlagBits_t::UP;
				}
				if (event.key.code == sf::Keyboard::S) {
					input &= ~inputFlagBits_t::DOWN;
				}
				if (event.key.code == sf::Keyboard::R) {
					input &= ~inputFlagBits_t::READY;
				}
				break;
			default:
				break;
			}
		}

		world.SetInput(input, GetMouseWorldCoords());
		world.SetMapBoundries((float)window->getSize().x, (float)window->getSize().y);

		assert(state);
		float now = NowSeconds();
		time.frameTime = now - time.lastFrame;
		time.ticksToDo += time.frameTime * ticksPerSecond;
		time.lastFrame = now;

		while (time.ticksToDo >= 1.0f) {
			now = NowSeconds();
			time.deltaTime = now - time.lastTick;
			time.lastTick = now;

			state->OnTick();
			world.Tick(state, time.deltaTime);
			time.ticksToDo--;
		}

		window->clear();
		state->OnUpdate();
		world.Render(*window);

		sf::CircleShape shape(5.0f);
		shape.setPosition(debug.mouse);
		window->draw(shape);
		window->display();

		if (connection != k_HSteamNetConnection_Invalid) {
			if (listen == k_HSteamListenSocket_Invalid) {
				ClientNetworkUpdate(time.frameTime);
			} else {
				HostNetworkUpdate(time.frameTime);
			}
		
			messageManager.Update(connection, utils, sockets);
		}
	}

	~game_t() {
		music.stop();
		delete window;
	}

	template<typename T>
	void TransitionState() {
		if (dynamic_cast<T*>(state) != nullptr) {
			// Don't transition to the new state if we are already in that state
			return;
		}

		if (state) {
			delete state;
		}

		state = new T;
	}

	void HostNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty()) {
			message_t message = messageManager.PopIncoming();
			messageHeader_t header;
			messageInput_t input;

			auto des = message.StartDeserialize();
			message.Deserialize(des, header);
			message.Deserialize(des, input);
			if (!message.EndDeserialization(des)) {
				std::cout << "Error occurred when deserializing\n";
				continue;
			}

			world.SetClientInput(input);
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerStateUpdate) {
			network.lastUpdateSent = 0.0f;

			message_t message;
			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::state);
			message.Serialize(ser, state->Enum());
			message.Serialize(ser, world);
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
	}

	void TransitionState(gameStateEnum_t state);

	void ClientNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty()) {
			message_t message = messageManager.PopIncoming();
			messageHeader_t header;
			gameStateEnum_t stateEnum;

			auto des = message.StartDeserialize();
			message.Deserialize(des, header);
			message.Deserialize(des, stateEnum);
			message.Deserialize(des, world);
			if (!message.EndDeserialization(des)) {
				std::cout << "Error occurred when deserializing\n";
				continue;
			}

			if (stateEnum != state->Enum()) {
				TransitionState(stateEnum);
			}
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerInputUpdate) {
			network.lastUpdateSent = 0.0f;

			message_t message;
			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::input);
			message.Serialize(ser, messageInput_t(input, GetMouseWorldCoords()));
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
	}

	sf::RenderWindow* window;
	sf::Music music;
	sf::Font font;
	sf::SoundBuffer destroySound;
	sf::SoundBuffer getNoobSound;
	sf::Sound destroyPlayer;
	sf::Sound getNoobPlayer;

	struct {
		sf::Vector2f mouse;
	} debug;

	bool exit;
	gameState_t* state;
	gameWorld_t world;
	float timeRecieved;
	messageManagerr_t messageManager;
	
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* utils;
	HSteamListenSocket listen;
	HSteamNetConnection connection;
	uint8_t input;

	struct {
		float lastUpdateSent = 0.0f;
	} network;

	struct {
		float frameTime = 0.0f;
		float lastFrame = 0.0f;
		float deltaTime = 0.0f;
		float lastTick  = 0.0f;
		float ticksToDo = 0.0f;
	} time;
} game;
