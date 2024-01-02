#pragma once
#include "base.hpp"
#include "entity.hpp"
#include "physics.hpp"
#include "network.hpp"

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

enum class gameStateEnum_t: u8_t {
	connecting = 0,
	connectionFailed,
	connected,
	start,
	play,
	gameOver,
	unknown
};

template<typename S>
void serialize(S& s, gameStateEnum_t& state) {
	s.value1b(state);
}

class gameState_t {
public:
	virtual void OnTick() = 0;
	virtual void OnUpdate() = 0;
	virtual gameStateEnum_t Enum() = 0;
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

		sockets = nullptr;
		utils = nullptr;
	}

	~global_t() {
		StopMainTrack();
	}

	int Init();

	void Update();

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

public: /* host side */
	void HostNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty() && false) {
			message_t message = messageManager.PopIncoming();

			auto des = message.StartDeserialize();

			if (!message.EndDeserialization(des)) {
				std::cout << "Error occurred when deserializing\n";
				continue;
			}
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerStateUpdate) {
			network.lastUpdateSent = 0.0f;

			message_t message;
			message.SetFlags(SEND_ALL);

			auto ser = message.StartSerialize();
			message.Serialize(ser, state->Enum());
			worldSnapshotBuilder->BuildMessage(world, message, ser, context);
 			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
	}

	void AddNewClient(HSteamNetConnection connection) {
		clients[connection] = {};
		messageManager.AddConnection(connection);

		if(IsHost())
			SyncAllClients();
	}

	void RemoveClient(HSteamNetConnection connection) {
		clients.erase(connection);
	}
	
	// Updates all clients with the current state
	// Note: is slow, call sparingly
	void SyncAllClients() {
		flecs::entity normalPipeline = world.get_pipeline();
	
		// Must insure the worldSnapshotBuilder is clear (aside from deleted entities)
		// so we don't have entity-components left over from a previous snapshot
		worldSnapshotBuilder->ResetComponentUpdateQueue();

		// The network pipeline contains the systems
		// that fetch all networked components
		// and put them into the worldSnapshotBuilder
		world.set_pipeline(networkPipeline);
		world.progress(0.0f);

		message_t message;
		message.SetFlags(SEND_ALL);

		auto ser = message.StartSerialize();
		message.Serialize(ser, state->Enum());
		worldSnapshotBuilder->BuildMessage(world, message, ser, context);
		message.EndSerialize(ser);

		messageManager.PushOutgoing(std::move(message));

		// Restoring previous pipeline
		world.set_pipeline(normalPipeline);
	}

public: /* client side */
	void ClientNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty()) {
			message_t message = messageManager.PopIncoming();
			gameStateEnum_t stateEnum = gameStateEnum_t::unknown;

			auto des = message.StartDeserialize();

			message.Deserialize(des, stateEnum);
			DeserializeWorldSnapshot(world, context, message, des);

			if(!message.EndDeserialization(des)) {
				std::cout << "failed to end deserialization\n";
			}

			if (stateEnum != state->Enum()) {
				TransitionState(stateEnum);
			}
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerInputUpdate && false) {
			network.lastUpdateSent = 0.0f;

			message_t message;
			message.SetConnection(GetHostConnection());
			auto ser = message.StartSerialize();
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		}
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

public: /* mmm OOP boiler plate */
	sf::RenderWindow& GetWindow() {
		return *window;
	}

	void MarkExit() {
		exit = true;
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

	float GetDeltaTime() {
		return time.deltaTime;
	}

	float GetFrameTime() {
		return time.frameTime;
	}

public:
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* utils;

protected: /* HELPER FUNCTIONS FOR INIT */
	bool LoadResources() {
		if (!res.music.openFromFile("./arcadeMusic.mp3")) {
			return false;
		}

		res.music.setLoop(true);
		std::cout << "Loaded Music\n";

		if (!res.destroySound.loadFromFile("./destroySound.mp3")) {
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

	void NetworkInit() {
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
		window = std::make_shared<sf::RenderWindow>();
		window->create(sf::VideoMode({600, 400}), title);
	}

protected:
	void DeserializeWorldSnapshot(flecs::world& world, networkEcsContext_t& context, message_t& message, deserializer_t& des);

private:
	flecs::world world;
	networkEcsContext_t context;
	std::shared_ptr<worldSnapshotBuilder_t> worldSnapshotBuilder;
	flecs::entity networkPipeline;

	std::unordered_map<HSteamNetConnection, client_t> clients;

	flecs::entity player;

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

	std::shared_ptr<sf::RenderWindow> window;
	sf::Sound destroyPlayer;
	sf::Sound getNoobPlayer;

	bool exit;
	gameState_t* state;
	messageManager_t messageManager;

	HSteamListenSocket listen;
	HSteamNetConnection connection;
	uint8_t input;
};
