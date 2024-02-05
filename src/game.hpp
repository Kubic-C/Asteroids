#pragma once
#include "physicsEcs.hpp"
#include "state.hpp"

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
		input = 0;

		sockets = nullptr;
		utils = nullptr;
	}

	~global_t() {
		StopMainTrack();
	}

	int Init();

	void Update();

	void TransitionState(gameStateEnum_t state);

	void EnableEntity(flecs::entity e);
	void DisableEntity(flecs::entity e);

public: /* host side */
	void StartHosting();

	void HostNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty()) {
			message_t message = messageManager.PopIncoming();
			messageHeader_t header;

			auto des = message.StartDeserialize();
			message.Deserialize(des, header);
			switch(header) {
			case messageHeader_t::input: {
				messageInput_t input;
				message.Deserialize(des, input);
				clients[message.GetConnection()].player.set([&](playerComp_t& comp){
					comp.SetMouse(input.mouse);
					comp.SetKeys(input.input);
				});
			} break;
			case messageHeader_t::playerInfo: {
				playerInfo_t playerInfo;
				message.Deserialize(des, playerInfo);
				clients[message.GetConnection()].player.set([&](playerColor_t& color) {
					color.SetColor(playerInfo.playerColor);
					});
			} break;
			default:
				break;
			}

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
			message.Serialize(ser, messageHeader_t::state);
			message.Serialize(ser, state->Enum());
			worldSnapshotBuilder->BuildMessage(world, message, ser, context);
			physicsWorldSnapshotBuilder->BuildMessage(*physicsWorld, message, ser);
 			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));

			time.nups++;
		}
	}

	flecs::entity AddPlayerComponents(flecs::entity e) {
		e.add<isNetworked_t>()
			.add<playerComp_t>()
			.add<transform_t>()
			.add<health_t>()
			.add<integratable_t>()
			.add<color_t>()
			.add<playerColor_t>()
			.add<shapeComp_t>()
			.set([&](color_t& color) { color.SetColor(sf::Color::Red); })
			.set([](transform_t& transform) { transform.SetPos({ 300, 200 }); })
			.set([&](transform_t& transform, shapeComp_t& shape) {
				shape.shape = physicsWorld->CreateShape<polygon_t>(transform.GetPos(), transform.GetRot(), playerVertices); 
			});
	
		return e;
	}

	void AddNewClient(HSteamNetConnection connection) {
		clients[connection] = {};

		if(connection != hostPlayerID)
			messageManager.AddConnection(connection);

		if(IsHost()) {
			clients[connection].player = AddPlayerComponents(world.entity());

			SyncAllClients();
		}
	}

	void RemoveClient(HSteamNetConnection connection) {
		if(clients[connection].player.is_alive())
			clients[connection].player.destruct();
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
		message.Serialize(ser, messageHeader_t::state);
		message.Serialize(ser, state->Enum());
		worldSnapshotBuilder->BuildMessage(world, message, ser, context);
		physicsWorldSnapshotBuilder->BuildMessage(*physicsWorld, message, ser, true);
		message.EndSerialize(ser);

		messageManager.PushOutgoing(std::move(message));

		// Restoring previous pipeline
		world.set_pipeline(normalPipeline);
	}

public: /* client side */
	bool TryConnect(std::string ipAddress);

	void ClientNetworkUpdate(float frameTime) {
		while (!messageManager.IsIncomingEmpty()) {
			message_t message = messageManager.PopIncoming();
			messageHeader_t header;

			auto des = message.StartDeserialize();
			message.Deserialize(des, header);
			switch(header) {
			case messageHeader_t::state: {
				gameStateEnum_t stateEnum = gameStateEnum_t::unknown;
				message.Deserialize(des, stateEnum);
				DeserializeWorldSnapshot(world, context, message, des);
				DeserializePhysicsSnapshot(*physicsWorld, message, des);

				if (stateEnum != state->Enum()) {
					TransitionState(stateEnum);
				}
			} break;
			default:
				::exit(EXIT_FAILURE);
				break;
			}

			if(!message.EndDeserialization(des)) {
				std::cout << "failed to end deserialization\n";
			}
		}

		network.lastUpdateSent += frameTime;
		if (network.lastUpdateSent >= timePerInputUpdate) {
			network.lastUpdateSent = 0.0f;

			const gameKeyboard_t* keyboard = game->world.get<gameKeyboard_t>();

			message_t message;
			message.SetConnection(GetHostConnection());

			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::input);
			message.Serialize(ser, messageInput_t(keyboard->keys, keyboard->mouse));
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));

			time.nups++;
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
	tgui::Gui& GetGUI() {
		return *gui;
	}

	flecs::world& GetWorld() {
		return world;
	}

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

	template<typename T>
	T& GetCastState() {
		return dynamic_cast<T&>(*state);
	}

	bool IsHost() {
		return IsListenSocketValid();
	}

	void SetListen(HSteamListenSocket socket) {
		listen = socket;
	}

	void SetNetworkActive(bool active) {
		networkActive = active;
	}

	bool IsNetworkActive() {
		return networkActive;
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

	messageManager_t& GetMessageManager() {
		return messageManager;
	}

	client_t& GetClient(HSteamNetConnection connection) {
		return clients[connection];
	}

public:
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* utils;

	// When whether we are a client or host is known, call this function
	void LoadRestOfState();

protected: /* HELPER FUNCTIONS FOR INIT */
	bool LoadResources() {
		if (!res.music.openFromFile("./res/arcadeMusic.mp3")) {
			return false;
		}

		res.music.setLoop(true);
		std::cout << "Loaded Music\n";

		if (!res.destroySound.loadFromFile("./res/destroySound.mp3")) {
			return false;
		}
		destroyPlayer.setBuffer(res.destroySound);

		if (!res.getNoobSound.loadFromFile("./res/getNoob.mp3")) {
			return false;
		}
		getNoobPlayer.setBuffer(res.getNoobSound);
		getNoobPlayer.setVolume(70);

		std::cout << "Loaded Sounds\n";

		if (!res.font.loadFromFile("./res/times.ttf")) {
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
		window->create(sf::VideoMode({w, h}), title, sf::Style::Titlebar | sf::Style::Close);
		window->setFramerateLimit(0); // When in host mode, frameratelimit can actually slow down the program thus affecting all clients
	
		gui = std::make_shared<tgui::Gui>(*window);
	}

protected:
	void DeserializeWorldSnapshot(flecs::world& world, networkEcsContext_t& context, message_t& message, deserializer_t& des);
	void DeserializePhysicsSnapshot(physicsWorld_t& physicsWorld, message_t& message, deserializer_t& des);

private:
	flecs::world world;
	networkEcsContext_t context;
	std::shared_ptr<worldSnapshotBuilder_t> worldSnapshotBuilder;
	std::shared_ptr<physicsWorld_t> physicsWorld;
	std::shared_ptr<physicsWorldSnapshotBuilder_t> physicsWorldSnapshotBuilder;
	flecs::entity networkPipeline;

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

		float nextFrameCount = 0.0f;
		float fps = 0.0f;
		float tps = 0.0f;
		float nups = 0.0f;
	} time;

	struct {
		float framesCounted = 0.0f;
		float fps = 0.0f;
		float tps = 0.0f;
		float nups = 0.0f;
	} stats;

	std::shared_ptr<tgui::Gui> gui;
	std::shared_ptr<sf::RenderWindow> window;
	sf::Sound destroyPlayer;
	sf::Sound getNoobPlayer;

	bool exit;
	std::array<std::shared_ptr<gameState_t>, stateCount> states;
	std::shared_ptr<gameState_t> state;
	messageManager_t messageManager;

	bool networkActive = false;
	HSteamListenSocket listen;
	uint8_t input;
};
