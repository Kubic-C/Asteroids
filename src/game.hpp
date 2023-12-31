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

struct client_t {
	u8_t keys;
	sf::Vector2f mouse;
	u32_t playerID;
};

enum messageFlagBits_t {
	SEND_ALL = 1 << 0
};

struct message_t {
	message_t() {}

	message_t(message_t&& other) noexcept {
		bytes = std::move(other.bytes);
		connection = other.connection;
		flags = other.flags;
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

	void SetConnection(HSteamNetConnection connection) {
		this->connection = connection;
	}

	HSteamNetConnection GetConnection() const {
		return connection;
	}

	void SetFlags(uint8_t flags) {
		this->flags = flags;
	}

	bool IsSendAll() {
		return flags & SEND_ALL;
	}

private:
	uint8_t flags = 0;
	HSteamNetConnection connection = 0;
	std::vector<u8_t> bytes;
};

class messageManagerr_t {
protected:

	void Send(ISteamNetworkingSockets* sockets, HSteamNetConnection connection, void* payload, u32_t size) {
		EResult result = sockets->SendMessageToConnection(connection, payload, size, k_nSteamNetworkingSend_Unreliable, nullptr);
		if(result != k_EResultOK) {
			std::cout << "Failed to send:" << result << '\n';
			exit(EXIT_FAILURE);
		}
	}

public:
	void Init() {
		pollGroup = SteamNetworkingSockets()->CreatePollGroup();
		if(pollGroup == k_HSteamNetPollGroup_Invalid) {
			std::cout << "Failed to create poll group\n";
			exit(EXIT_FAILURE);
		}
	}
	

	void Update(std::unordered_map<HSteamNetConnection, client_t>& clients, ISteamNetworkingUtils* utils, ISteamNetworkingSockets* sockets) {
		// Send outgoing messages
		while (!outgoing.empty()) {
			message_t message = std::move(outgoing.front());

			if(message.IsSendAll()) {
				for(const std::pair<HSteamNetConnection, client_t>& pair : clients) {
					if(pair.first == message.GetConnection() || pair.first == hostPlayerID) 
						continue;

					Send(sockets, pair.first, message.Data(), message.Size());
				}
			} else {
				Send(sockets, message.GetConnection(), message.Data(), message.Size());
			}

			outgoing.pop();
		}

		// Get incoming messages;
		do {
			SteamNetworkingMessage_t* message;
			int amount = sockets->ReceiveMessagesOnPollGroup(pollGroup, &message, 1);
			if(amount < 1) {
				break;
			}

			message_t new_message;
			new_message.SetData(message->GetSize(), (u8_t*)message->GetData());
			new_message.SetConnection(message->GetConnection());
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

	void AddConnection(HSteamNetConnection connection) {
		if(!SteamNetworkingSockets()->SetConnectionPollGroup(connection, pollGroup)) {
			std::cout << "Failed to add connection to poll group: " << connection << '\n'; 
		}
	}

private:
	HSteamNetPollGroup pollGroup;
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

	void ResetDestroyed() {
		destroyed = false;
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
	gameWorld_t() {
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
		s.value1b(lives);
		s.container(players, UINT32_MAX);
		s.container(asteroids, UINT32_MAX);
		s.container(bullets, UINT32_MAX);
	}

	u32_t AddNewPlayer() {
		u32_t playerID = nextPlayerID;
		nextPlayerID++;
		players.emplace_back(this);

		return playerID;
	}

	void HostSetInput(std::unordered_map<HSteamNetConnection, client_t>& clients) {
		for(auto& pair : clients) {
			players[pair.second.playerID].SetInput(pair.second.keys, pair.second.mouse);
		}
	}

	void CheckCollision();

	u8_t GetTotalLives() {
		return lives;
	}

	void Tick(gameState_t* state, float deltaTime) {
		SpawnAsteroidWhenReady(state->Enum(), deltaTime);
		CheckCollision();
		for(u32_t i = 0; i < bullets.size(); i++) {
			bullets[i].Intergrate(deltaTime);
			bullets[i].Tick(state->Enum(), deltaTime);
			bullets[i].Wrap(mapWidth, mapHeight);
		}

		for (u8_t i = 0; i < players.size(); i++) {
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

	bool IsAllReady() {
		for(u32_t i = 0; i < players.size(); i++) {
			if(!players[i].IsReady())
				return false;
		}

		return true;
	}

	bool IsAllDestroyed() {
		for (u32_t i = 0; i < players.size(); i++) {
			if (!players[i].IsDestroyed())
				return false;
		}

		return true;
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

		for(u32_t i = 0; i < players.size(); i++) {
			players[i].ResetReady();
		}
	}

	void Reset() {
		mapWidth = windowWidth;
		mapHeight = windowHeight;
		
		for (u32_t i = 0; i < players.size(); i++) {
			players[i].vel = { 0.0f, 0.0f };
			players[i].ResetReady();
			players[i].ResetDestroyed();
		}

		asteroidSpawnRate = timePerAsteroidSpawn;
		timeToSpawnAsteroid = asteroidSpawnRate;
		lives = 3;
		mapWidth = 0.0f;
		mapHeight = 0.0f;

		asteroids.clear();
		bullets.clear();
	}

	void MarkHost() {
		isHost = true;
	}

private:
	float asteroidSpawnRate = timePerAsteroidSpawn;
	float timeToSpawnAsteroid = asteroidSpawnRate;

	u8_t lives = 3;
	float mapWidth = 0.0f;
	float mapHeight = 0.0f;
	bool isHost = false;
	u32_t score = 0;
	u8_t nextPlayerID = 0;
	std::vector<player_t> players;
	std::vector<asteroids_t> asteroids;
	std::vector<bullet_t> bullets;
};

class global_t;

inline global_t* game = nullptr;

class global_t {

public:
	global_t() 
		: destroyPlayer(res.destroySound), getNoobPlayer(res.getNoobSound) {
		assert(game == nullptr);

		window = nullptr;

		exit = false;
		state = nullptr;

		listen = k_HSteamListenSocket_Invalid;
		connection = k_HSteamNetConnection_Invalid;
		input = 0;
		
		clients[hostPlayerID].playerID = world.AddNewPlayer();
	}

	~global_t() {
		StopMainTrack();
		delete window;
	}
	
	bool LoadResources() {
		if(!res.music.openFromFile("./arcadeMusic.mp3")) {
			return false;
		}

		res.music.setLoop(true);
		std::cout << "Loaded Music\n";

		if(!res.destroySound.loadFromFile("./destroySound.mp3")) {
			return false;
		}
		destroyPlayer.setBuffer(res.destroySound);

		if (!res.getNoobSound.loadFromFile("./getNoob.mp3")) {
			return false;
		}
		getNoobPlayer.setBuffer(res.getNoobSound);
		getNoobPlayer.setVolume(70);

		std::cout << "Loaded Sounds\n";

		if (!res.font.loadFromFile("./times.ttf")) {
			return false;
		}

		std::cout << "Loaded Font\n";
		
		return true;
	}

	void FinishNetworkInit() {
		SteamNetworkingErrMsg msg;
		if (!GameNetworkingSockets_Init(nullptr, msg)) {
			std::cout << msg << std::endl;
			::exit(EXIT_FAILURE);
		}
		game->sockets = SteamNetworkingSockets();
		game->utils = SteamNetworkingUtils();

		messageManager.Init();
	}

	void CreateWindow(u32_t w, u32_t h, std::string title) {
		window = new sf::RenderWindow(sf::VideoMode({ w, h }), title);
	}

	sf::RenderWindow& GetWindow() {
		return *window;
	}

	void MarkExit() {
		exit = true;
	}

	void MarkHost() {
		world.MarkHost();
	}

	void Update();

	void HandleInput() {
		if(IsHost()) {
			clients[hostPlayerID].keys = input;
			clients[hostPlayerID].mouse = GetMouseWorldCoords();
			world.HostSetInput(clients);
		} 
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

	void TransitionState(gameStateEnum_t state);

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

			clients[message.GetConnection()].keys = input.input;
			clients[message.GetConnection()].mouse = input.mouse;
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerStateUpdate) {
			network.lastUpdateSent = 0.0f;

			message_t message;
			message.SetFlags(SEND_ALL);
			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::state);
			message.Serialize(ser, state->Enum());
			message.Serialize(ser, world);
 			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
	}

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
			message.SetConnection(GetHostConnection());
			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::input);
			message.Serialize(ser, messageInput_t(input, GetMouseWorldCoords()));
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
	}

	void PlayMainTrack() {
		res.music.play();
	}

	void StopMainTrack() {
		res.music.stop();
	}

	void PlayDestroySound() {
		destroyPlayer.play();
	}

	void PlayNoobSound() {
		getNoobPlayer.play();
	}

	sf::Font& GetFont() {
		return res.font;
	}

	gameState_t& GetState() {
		return *state;
	}

	bool IsHost() {
		return IsListenSocketValid();
	}

	void SetListen(HSteamListenSocket socket) {
		listen = socket;
	}

	bool IsListenSocketValid() {
		return listen != k_HSteamListenSocket_Invalid;
	}

	bool ShouldAppClose() {
		return !window->isOpen() || exit; 
	}

	gameWorld_t& GetWorld() {
		return world;
	}

	float GetDeltaTime() {
		return time.deltaTime;
	}

	float GetFrameTime() {
		return time.frameTime;
	}

	void AddNewClient(HSteamNetConnection connection) {
		messageManager.AddConnection(connection);
		clients[connection].playerID = world.AddNewPlayer();
	}

	HSteamNetConnection GetHostConnection() {
		// the ID of the host connection will be the first non-host-id
		for(const std::pair<HSteamNetConnection, client_t>& pair : clients) {
			if(pair.first != hostPlayerID) {
				return pair.first;
			}
		}
	
		return -1;
	}

public:
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* utils;

private:
	std::unordered_map<HSteamNetConnection, client_t> clients;

	struct {
		sf::Music music;
		sf::Font font;
		sf::SoundBuffer destroySound;
		sf::SoundBuffer getNoobSound;
	} res;

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

	sf::RenderWindow* window;
	sf::Sound destroyPlayer;
	sf::Sound getNoobPlayer;

	bool exit;
	gameState_t* state;
	gameWorld_t world;
	messageManagerr_t messageManager;

	HSteamListenSocket listen;
	HSteamNetConnection connection;
	uint8_t input;
};
