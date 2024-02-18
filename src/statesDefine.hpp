#pragma once
#include "game.hpp"
#include "component.hpp"

inline flecs::entity addPlayerComponents(flecs::entity e) {
	e.add<PlayerComponent>()
		.add<ae::TransformComponent>()
		.add<HealthComponent>()
		.add<ae::IntegratableComponent>()
		.add<ColorComponent>()
		.add<PlayerColorComponent>()
		.add<ae::ShapeComponent>()
		.set([&](ColorComponent& color) { color.setColor(sf::Color::Red); })
		.set([](ae::TransformComponent& transform) { transform.setPos({ 300, 200 }); })
		.set([&](ae::TransformComponent& transform, ae::ShapeComponent& shape) {
			shape.shape = 
				ae::getPhysicsWorld().createShape<ae::Polygon>(transform.getPos(), transform.getRot(), playerVertices);
		});

	return e;
}

inline void addSoundControlMenu(tgui::BackendGui& gui) {
	auto musicToggle = tgui::Button::create();
	musicToggle->setText("Toggle music");
	musicToggle->setPosition("50%", "10%");
	musicToggle->setSize("210", "60");
	musicToggle->setOrigin(0.5f, 0.5f);
	musicToggle->onClick([](){
		auto status = global->res.music.getStatus();
		if(status == sf::SoundSource::Paused) {
			global->res.music.play();
		} else {
			global->res.music.pause();
		}
	});
	gui.add(musicToggle);
}

class ClientInterface: public ae::ClientInterface {
public:
	ClientInterface() {
		inputUpdate.setRate(inputUPS);
		inputUpdate.setFunction([](float){
			ae::NetworkManager& networkManager = ae::getNetworkManager();
			ae::MessageBuffer buffer;
			MessageInput input(getInput());
			
			ae::Serializer ser = ae::startSerialize(buffer);
			ser.object(MESSAGE_HEADER_INPUT);
			ser.object(input);
			ae::endSerialize(ser, buffer);

			networkManager.sendMessage(0, std::move(buffer), true);
		});

		ae::getWindow().setTitle("ECS Asteroids Client");
	}

	void update() override {
		if(!global->player.is_alive()) {
			if(!playerRequestSent) {
				ae::MessageBuffer buffer;
				ae::Serializer ser = ae::startSerialize(buffer);
				ser.object(MESSAGE_HEADER_REQUEST_PLAYER_ID);
				ae::endSerialize(ser, buffer);

				ae::getNetworkManager().sendMessage(0, std::move(buffer), true, true);
				playerRequestSent = true;
			}
		} else {
			global->player.set([](PlayerComponent& player) {
				auto input = getInput();

				player.setKeys(input.first);
				player.setMouse(input.second);
			});
		}

		inputUpdate.update();
	}

	void onMessageRecieved(HSteamNetConnection conn, ae::MessageHeader header_, ae::Deserializer& des) override {
		MessageHeader header = (MessageHeader)header_;

		switch(header) {
		case MESSAGE_HEADER_REQUEST_PLAYER_ID: {
			u32 playerId = 0;
			des.object(playerId);

			global->player = ae::impl::af(playerId);
		} break;
		}
	}

	void onConnectionJoin(HSteamNetConnection connection) {
		playerRequestSent = false;
	}

private:
	bool playerRequestSent = false;
	ae::Ticker<void(float)> inputUpdate;
};

class ServerInterface: public ae::ServerInterface {
public:
	ServerInterface() {
		flecs::world& entityWorld = ae::getEntityWorld();
		entityWorld.add<AsteroidTimerComponent>();
		entityWorld.set([&](MapSizeComponent& size) { size.setSize(ae::getWindow().getSize()); });
		entityWorld.add<SharedLivesComponent>();
		entityWorld.add<ScoreComponent>();

		setNetworkUPS(stateUPS);

		ae::getWindow().setTitle("ECS Asteroids Server");
	}

	void update() {
		global->player.set([](PlayerComponent& player){
			auto input = getInput();

			player.setKeys(input.first);
			player.setMouse(input.second);
		});

#ifndef NDEBUG
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F1)) {
			ae::getEntityWorld().set([](ScoreComponent& score){
				score.addScore(100);
			});
		}
#endif
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F3)) {
			ae::PhysicsWorld& physicsWorld = ae::getPhysicsWorld();
			float width = (float)ae::getWindow().getSize().x;
			float height = (float)ae::getWindow().getSize().y;

			float spawnX = (randomFloat() * width);
			float spawnY = (randomFloat() * height);

			float wallX = 0.0f;
			float distX = 0.0f;
			float distToLeft = spawnX;
			float distToRight = width - spawnX;
			if (distToLeft < distToRight) {
				wallX = width - 0.01f;
				distX = distToRight;
			}
			else {
				distX = distToLeft;
			}

			float wallY = 0.0f;
			float distY = 0.0f;
			float distToUp = spawnY;
			float distToDown = height - spawnY;
			if (distToUp < distToDown) {
				wallY = height - 0.1f;
				distY = distToDown;
			}
			else {
				distY = distToUp;
			}

			if (distX < distY) {
				spawnX = wallX;
			}
			else {
				spawnY = wallY;
			}

			ae::getEntityWorldNetworkManager().entity()
				.add<AsteroidComponent>()
				.add<ae::TransformComponent>()
				.add<HealthComponent>()
				.add<ae::IntegratableComponent>()
				.add<ColorComponent>()
				.add<AsteroidComponent>()
				.set([&](ae::ShapeComponent& shape, ae::TransformComponent& transform, ae::IntegratableComponent& integratable, ColorComponent& color) {
				transform.setPos({ spawnX, spawnY });

				sf::Vector2f center = sf::Vector2f(width, height) / 2.0f;
				sf::Vector2f velToCenter = (transform.getPos() - center).normalized();
				integratable.addLinearVelocity(velToCenter * 10.0f);

				color.setColor(sf::Color::Red);

				shape.shape = physicsWorld.createShape<ae::Polygon>();
				ae::Polygon& polygon = physicsWorld.getPolygon(shape.shape);

				std::vector<sf::Vector2f> vertices = generateRandomConvexShape(8, asteroidScalar);
				polygon.setVertices((u8)vertices.size(), vertices.data());
				polygon.setPos(transform.getPos());
					});
		}
	}

	void onConnectionJoin(HSteamNetConnection conn) {
		clients[conn] = addPlayerComponents(ae::getEntityWorldNetworkManager().entity());

		fullSyncUpdate(conn); // When a connection joins use this as an opportunity to sync all of them
	}

	void onConnectionLeave(HSteamNetConnection conn) {
		clients[conn].destruct();
		clients.erase(conn);
	}

	void onMessageRecieved(HSteamNetConnection conn, ae::MessageHeader header_, ae::Deserializer& des) override {
		MessageHeader header = (MessageHeader)header_;

		switch(header) {
		case MESSAGE_HEADER_INPUT: {
			MessageInput keys;
			des.object(keys);

			clients[conn].set([&](PlayerComponent& playerComponent){
				playerComponent.setKeys(keys.keys);
				playerComponent.setMouse(keys.mouse);
			});
		} break;

		case MESSAGE_HEADER_PLAYER_INFO: {
			MessagePlayerInfo playerInfo;
			des.object(playerInfo);

			clients[conn].set([&](PlayerColorComponent& playerColor){
				playerColor.setColor(playerInfo.playerColor);
			});
				
		} break;

		case MESSAGE_HEADER_REQUEST_PLAYER_ID: {
			u32 playerId = ae::impl::cf(clients[conn]);

			ae::MessageBuffer buffer;
			ae::Serializer ser = ae::startSerialize(buffer);
			ser.object(MESSAGE_HEADER_REQUEST_PLAYER_ID);
			ser.object(playerId);
			ae::endSerialize(ser, buffer);

			ae::getNetworkManager().sendMessage(conn, std::move(buffer), false, true);
		} break;

		default:
			ae::getNetworkManager().connectionAddWarning(conn);
			break;
		}
	}

	size_t getConnectionCount() const {
		return clients.size();
	}

private:
	std::unordered_map<HSteamNetConnection, flecs::entity> clients;
};

class ConnectingState;
class StartState;
class ConnectedState;

// Get all necessary information such as:
// - Are we hosting or playing solo
// - If not what IP are we connecting to
class MainMenuState : public ae::State {
public:
	// Hosting:
	//	- Remember to add isHost to flecs world
	//  - Game needs to be marked as host as well
	// Quickplay:
	//  - Same as hosting but we transition straight to the play state
	// Client:
	//	- Connect to the given IP

	void onEntry() override {
		ae::NetworkManager& networkManager = ae::getNetworkManager();
		tgui::Gui& gui = ae::getGui();

		if(networkManager.hasNetworkInterface<ClientInterface>()) {
			createClientMenu(gui);
			return;
		}
		
		createPlayerInfoMenu(gui);

#ifdef NDEBUG
		global->res.music.play();
#endif
	}

	void onLeave() override {
		ae::NetworkManager& networkManager = ae::getNetworkManager();

		if(networkManager.hasNetworkInterface<ClientInterface>()) {
			ae::MessageBuffer message;
			
			auto ser = ae::startSerialize(message);
			ser.object(MESSAGE_HEADER_PLAYER_INFO);
			ser.object(playerInfo);
			ae::endSerialize(ser, message);

			networkManager.sendMessage(0, std::move(message), true, true);
		} else {
			global->player.set([&](PlayerColorComponent& color){
				color.setColor(playerInfo.playerColor);
			});
		}
	}

private:
	static void openServer() {
		ae::NetworkManager& networkManager = ae::getNetworkManager();
		std::shared_ptr<ServerInterface> server = std::make_shared<ServerInterface>();
		networkManager.setNetworkInterface(server);
		SteamNetworkingIPAddr addr;
		addr.Clear();
		addr.SetIPv4(0, defaultHostPort);
		if (!networkManager.open(addr)) {
			ae::log(ae::ERROR_SEVERITY_WARNING, "Failed to open server.\n");
			networkManager.setNetworkInterface(nullptr);
			return;
		}

		global->player = 
			addPlayerComponents(ae::getEntityWorldNetworkManager().entity());
	}

	static void createClient() {
		ae::NetworkManager& networkManager = ae::getNetworkManager();
		std::shared_ptr<ClientInterface> client = std::make_shared<ClientInterface>();
		networkManager.setNetworkInterface(client);
	}

	static void openClient(const char* ip) {
		ae::NetworkManager& networkManager = ae::getNetworkManager();

		SteamNetworkingIPAddr addr;
		addr.Clear();
		addr.ParseString(ip);
		addr.m_port = (u16)defaultHostPort;

		if(!networkManager.open(addr)) {
			ae::log(ae::ERROR_SEVERITY_WARNING, "Failed to open client. %s:%i\n", ip, defaultHostPort);
			return;
		}
	}
	
	static void OnHostButtonClick() {
		openServer();

		ae::transitionState<ConnectingState>();
	}

	static void OnClientButtonClick() {
		createClient();

		ae::getCurrentState<MainMenuState>().createClientMenu(ae::getGui());
	}

	static void OnQuickplayClick() {
		openServer();

		ae::transitionState<StartState>();
	}

	static void OnConnectClick(tgui::EditBox::Ptr editBox) {
		openClient(((std::string)editBox->getText()).c_str());

		ae::transitionState<ConnectingState>();
	}
public:
	void createPlayerInfoMenu(tgui::BackendGui& gui) {
		auto horiBarBottom = tgui::HorizontalLayout::create();
		horiBarBottom->setHeight("25%");
		horiBarBottom->setPosition("0%", "70%");
		horiBarBottom->setWidth("100%");
		gui.add(horiBarBottom);

		auto panel = tgui::Panel::create();
		panel->getRenderer()->setBackgroundColor(sf::Color::Green);
		horiBarBottom->add(panel);

		auto continueButton = tgui::Button::create();
		continueButton->setText("Continue");
		continueButton->setHeight("20%");
		continueButton->onPress([&]() {
			gui.removeAllWidgets();
			createMainMenu(gui);
			});
		horiBarBottom->add(continueButton);

		auto horiBarTop = tgui::HorizontalLayout::create();
		horiBarTop->setHeight("25%");
		horiBarTop->setPosition("0%", "50%");
		horiBarTop->setWidth("100%");
		gui.add(horiBarTop);

		horiBarTop->addSpace(0.05f);
		auto rSlider = tgui::Slider::create();
		rSlider->getRenderer()->setTrackColor(sf::Color::Red);
		rSlider->getRenderer()->setTrackColorHover(sf::Color::Red);
		rSlider->setMinimum(0);
		rSlider->setMaximum(255);
		rSlider->setStep(1);
		rSlider->setValue(0);
		rSlider->onValueChange([&](tgui::Panel::Ptr panel, float value) {
			playerInfo.playerColor.r = value;	

			sf::Color color = panel->getRenderer()->getBackgroundColor();
			color.r = value;
			panel->getRenderer()->setBackgroundColor(color);
		}, panel);
		horiBarTop->add(rSlider, 0.2f);

		auto gSlider = tgui::Slider::create();
		gSlider->getRenderer()->setTrackColor(sf::Color::Green);
		gSlider->getRenderer()->setTrackColorHover(sf::Color::Green);
		gSlider->setMinimum(0);
		gSlider->setMaximum(255);
		gSlider->setStep(1);
		gSlider->setValue(255);
		gSlider->onValueChange([&](tgui::Panel::Ptr panel, float value) {
			playerInfo.playerColor.g = value;

			sf::Color color = panel->getRenderer()->getBackgroundColor();
			color.g = value;
			panel->getRenderer()->setBackgroundColor(color);
		}, panel);
		horiBarTop->add(gSlider, 0.2f);

		auto bSlider = tgui::Slider::create();
		bSlider->getRenderer()->setTrackColor(sf::Color::Blue);
		bSlider->getRenderer()->setTrackColorHover(sf::Color::Blue);
		bSlider->setMinimum(0);
		bSlider->setMaximum(255);
		bSlider->setStep(1);
		bSlider->setValue(0);
		bSlider->onValueChange([&](tgui::Panel::Ptr panel, float value) {
			playerInfo.playerColor.b = value;
			
			sf::Color color = panel->getRenderer()->getBackgroundColor();
			color.b = value;
			panel->getRenderer()->setBackgroundColor(color);
		}, panel);
		horiBarTop->add(bSlider, 0.2f);
		horiBarTop->addSpace(0.05f);

	}

	void createMainMenu(tgui::BackendGui& gui) {
		gui.removeAllWidgets();
		gui.setTextSize(32);

		addSoundControlMenu(gui);

		auto vertBar = tgui::VerticalLayout::create();
		vertBar->setHeight("50%");
		vertBar->setPosition("50%", "50%");
		vertBar->setOrigin(0.5f, 0.5f);
		gui.add(vertBar);

		auto label = tgui::Label::create("ECS Asteroids by Kubic0x43");
		label->getRenderer()->setTextColor(sf::Color::White);
		label->setPosition("50%f", 0.0f);
		label->setWidth("100%");
		label->setOrigin(-0.1f, 0.0f);
		vertBar->add(label, 0.2);

		auto horiBar = tgui::HorizontalLayout::create();
		horiBar->setHeight("50%");
		vertBar->add(horiBar, 0.1);

		float spacing = 0.05f;
		horiBar->addSpace(spacing);
		auto hostButton = tgui::Button::create("Host");
		horiBar->setTextSize(24);
		hostButton->onPress(&OnHostButtonClick);
		horiBar->add(hostButton, 0.2);
		horiBar->addSpace(spacing);

		auto clientButton = tgui::Button::copy(hostButton);
		clientButton->setText("Client");
		clientButton->onPress(&OnClientButtonClick);
		horiBar->add(clientButton, 0.2);
		horiBar->addSpace(spacing);

		auto quickplayButton = tgui::Button::copy(hostButton);
		quickplayButton->setText("Quickplay");
		quickplayButton->onPress(&OnQuickplayClick);
		horiBar->add(quickplayButton, 0.2);
		horiBar->addSpace(spacing);
	}

	void createClientMenu(tgui::BackendGui& gui) {
		gui.removeAllWidgets();

		if(ae::getNetworkManager().getNetworkInterface().hasFailed()) {
			auto hasFailedText = tgui::Label::create();
			hasFailedText->setPosition("50%", "40%");
			hasFailedText->setOrigin(0.5f, 0.5f);
			hasFailedText->setText("Connection failed!");
			hasFailedText->getRenderer()->setBackgroundColor(sf::Color::Red);
			gui.add(hasFailedText);
		}

		auto ipAddress = tgui::EditBox::create();
		ipAddress->setSize("50%", "10%");
		ipAddress->setPosition("50%", "50%");
		ipAddress->setOrigin(0.5f, 0.5f);
		ipAddress->setDefaultText("Enter IP Adress");
		gui.add(ipAddress);

		auto connectButton = tgui::Button::create();
		connectButton->setSize("50%", "10%");
		connectButton->setPosition("50%", "60%");
		connectButton->setOrigin(0.5f, 0.5f);
		connectButton->setText("Connect");
		connectButton->onPress(OnConnectClick, ipAddress);
		gui.add(connectButton);
	}

private:
	MessagePlayerInfo playerInfo;
};

class ConnectingState : public ae::State {
public:
	void onEntry() override {
		createConnectingMenu(ae::getGui());
	}

	void onUpdate() override {
		if(ae::getNetworkManager().getNetworkInterface().hasFailed()) 
			ae::transitionState<MainMenuState>();

		if(ae::getNetworkManager().hasNetworkInterface<ServerInterface>())
			if(ae::getNetworkManager().getNetworkInterface<ServerInterface>().getConnectionCount() > 0)
				ae::transitionState<StartState>();
	}

private:
	void createConnectingMenu(ae::Gui& gui) {
		gui.removeAllWidgets();

		tgui::Label::Ptr connectingText = tgui::Label::create();
		connectingText->setText("Attempting to connect!");
		connectingText->setPosition("50%", "50%");
		connectingText->setOrigin(0.5f, 0.5f);
		connectingText->getRenderer()->setTextColor(sf::Color::White);
		gui.add(connectingText);
	}
};

class ConnectionFailedState : public ae::State {
public:
	void onTick(float deltaTime) override {
		timeLeft -= deltaTime;
		if (timeLeft <= 0.0f) {
			ae::getNetworkManager().close();
			timeLeft = 1.0f;
			ae::transitionState<MainMenuState>();
		}
	}

private:
	float timeLeft = 1.0f;
};

class ConnectedState : public ae::State {
public:
	void onUpdate() override {
		ae::transitionState<StartState>();
	}

private:
};

void updatePlayerReady(flecs::iter& iter, PlayerComponent* players, ColorComponent* colors, PlayerColorComponent* playerColors);
void isAllPlayersReady(flecs::iter& iter);
void orientPlayers(flecs::iter& iter, ae::TransformComponent* transforms);

struct HostStartStateModule {
	HostStartStateModule(flecs::world& world) {
		world.system<PlayerComponent, ColorComponent, PlayerColorComponent>().kind(flecs::OnUpdate).iter(updatePlayerReady);
		world.system().kind(flecs::PostUpdate).iter(isAllPlayersReady);
		world.system<ae::TransformComponent>().with<PlayerComponent>().iter(orientPlayers);
	}
};

class StartState : public ae::State {
public:
	void onEntry() override {
		createStartMenu(ae::getGui());
	}

private:
	void createStartMenu(ae::Gui& gui) {
		gui.removeAllWidgets();

		tgui::Label::Ptr connectingText = tgui::Label::create();
		connectingText->setText("All players must press 'R' to start!");
		connectingText->setPosition("50%", "20%");
		connectingText->setOrigin(0.5f, 0.5f);
		connectingText->getRenderer()->setTextColor(sf::Color::White);
		gui.add(connectingText);
	}
};

void updatePlayerDead(flecs::iter& iter, HealthComponent* health);
void isAllPlayersDead(flecs::iter& iter);
void isDead(flecs::iter& iter, HealthComponent* healths);
void playerPlayInputUpdate(flecs::iter& iter, PlayerComponent* players, ae::IntegratableComponent* integratables, ae::TransformComponent* transforms, HealthComponent* healths);
void playerBlinkUpdate(flecs::iter& iter, PlayerComponent* players, HealthComponent* healths, ColorComponent* colors, PlayerColorComponent* playerColors);
void playerReviveUpdate(flecs::iter& iter, SharedLivesComponent* lives, PlayerComponent* players, HealthComponent* healths);
void asteroidDestroyUpdate(flecs::iter& iter, AsteroidComponent* asteroids, ae::TransformComponent* transforms, ae::IntegratableComponent* integratables, HealthComponent* healths);
void transformWrap(flecs::iter& iter, MapSizeComponent* size, ae::TransformComponent* transforms);
void asteroidAddUpdate(flecs::iter& iter, MapSizeComponent* mapSize, AsteroidTimerComponent* timer);
void turretPlayUpdate(flecs::iter& iter, ae::TransformComponent* transforms, TurretComponent* turrets);
void observePlayerCollision(flecs::iter& iter, size_t i, ae::ShapeComponent&);
void observeBulletCollision(flecs::iter& iter, size_t i, ae::ShapeComponent&);

struct HostPlayStateModule {
	HostPlayStateModule(flecs::world& world) {
		world.system<HealthComponent>().with<PlayerComponent>().kind(flecs::OnUpdate).with(flecs::Disabled).optional().iter(updatePlayerDead);
		world.system().kind(flecs::PostUpdate).iter(isAllPlayersDead);
		world.system<HealthComponent>().iter(isDead);
		world.system<MapSizeComponent, AsteroidTimerComponent>().term_at(1).singleton().term_at(2).singleton().iter(asteroidAddUpdate);
		world.system<AsteroidComponent, ae::TransformComponent, ae::IntegratableComponent, HealthComponent>().iter(asteroidDestroyUpdate);
		world.system<SharedLivesComponent, PlayerComponent, HealthComponent>().term_at(1).singleton().iter(playerReviveUpdate);
		world.system<ae::TransformComponent, TurretComponent>().iter(turretPlayUpdate);
		world.system<MapSizeComponent, ae::TransformComponent>().term_at(1).singleton().iter(transformWrap);
		world.system<PlayerComponent, ae::IntegratableComponent, ae::TransformComponent, HealthComponent>().iter(playerPlayInputUpdate);
	}
};

struct PlayStateModule {
	PlayStateModule(flecs::world& world) {
		world.system<PlayerComponent, HealthComponent, ColorComponent, PlayerColorComponent>().iter(playerBlinkUpdate);
		world.observer<ae::ShapeComponent>().event<ae::CollisionEvent>().with<PlayerComponent>().each(observePlayerCollision);
		world.observer<ae::ShapeComponent>().event<ae::CollisionEvent>().with<BulletComponent>().each(observeBulletCollision);
	}
};

class PlayState : public ae::State {
public:
	void onEntry() override {
		createStats(ae::getGui()); 
	}

	void onLeave() override {
		ae::getGui().removeAllWidgets();
	}

	virtual void onUpdate() override {
		i32 score = ae::getEntityWorld().get<ScoreComponent>()->getScore();
		u32 lives = ae::getEntityWorld().get<SharedLivesComponent>()->lives;
		text->setText(ae::formatString("Lives: %li\nScore:%u", lives, score));
	}

private:
	void createStats(tgui::BackendGui& gui) {
		gui.removeAllWidgets();

		text = tgui::Label::create();
		text->getRenderer()->setTextColor(sf::Color::White);
		gui.add(text);
	}

private:
	tgui::Label::Ptr text;
};

struct HostGameOverStateModule {
public:
	HostGameOverStateModule(flecs::world& world) {
		world.system<PlayerComponent, ColorComponent, PlayerColorComponent>().kind(flecs::OnUpdate).iter(updatePlayerReady);
		world.system().kind(flecs::PostUpdate).iter(isAllPlayersReady);
		world.system<ae::TransformComponent>().with<PlayerComponent>().iter(orientPlayers);
	}
};

class GameOverState : public ae::State {
public:
	GameOverState() {
		destroyAsteroids = ae::getEntityWorld().query_builder<AsteroidComponent>().build();
		destroyBullets = ae::getEntityWorld().query_builder<BulletComponent>().build();
		resetPlayers = ae::getEntityWorld().query_builder<PlayerComponent, ae::IntegratableComponent, HealthComponent, ColorComponent>()
			.with(flecs::Disabled).optional().build();
		destroyTurrets = ae::getEntityWorld().query_builder<TurretComponent>().build();
	}

	void onEntry() override {
		createGameOverMenu(ae::getGui());

		ae::getEntityWorld().defer_begin();
		destroyAsteroids.each([](flecs::entity e, AsteroidComponent& asteroid) {
			e.destruct();
		});
		destroyBullets.each([](flecs::entity e, BulletComponent& asteroid) {
			e.destruct();
		});
		destroyTurrets.each([](flecs::entity e, TurretComponent& turret) {
			e.destruct();
		});

		std::vector<flecs::entity> entitiesToEnable;
		resetPlayers.each([&](
				flecs::entity e, 
				PlayerComponent& player, 
				ae::IntegratableComponent& integratable, 
				HealthComponent& health, 
				ColorComponent& color) {
			player.setIsReady(false);
			integratable.addLinearVelocity(-integratable.getLinearVelocity());
			health.setDestroyed(false);
			health.setHealth(1.0f);
			color.setColor(sf::Color::Red);

			if(!e.enabled())
				entitiesToEnable.push_back(e);

			e.modified<PlayerComponent>();
			e.modified<ae::IntegratableComponent>();
			e.modified<HealthComponent>();
			e.modified<ColorComponent>();
		});
		ae::getEntityWorld().defer_end();

		for(const auto& e : entitiesToEnable)
			ae::getEntityWorldNetworkManager().enable(e);

		if(ae::getNetworkManager().hasNetworkInterface<ServerInterface>()) {
			ae::getEntityWorld().get_mut<SharedLivesComponent>()->lives = initialLives;
			ae::getEntityWorld().get_mut<ScoreComponent>()->resetScore();
			ae::getEntityWorld().get_mut<AsteroidTimerComponent>()->resetTime = timePerAsteroidSpawn;
		}
	}

protected:
	void createGameOverMenu(ae::Gui& gui) {
		gui.removeAllWidgets();

		tgui::Label::Ptr toStartText = tgui::Label::create();
		toStartText->setText("All players must press 'R' to start again!");
		toStartText->setPosition("50%", "20%");
		toStartText->setOrigin(0.5f, 0.5f);
		toStartText->getRenderer()->setTextColor(sf::Color::White);
		gui.add(toStartText);
	}

	flecs::query<AsteroidComponent> destroyAsteroids;
	flecs::query<BulletComponent> destroyBullets;
	flecs::query<PlayerComponent, ae::IntegratableComponent, HealthComponent, ColorComponent> resetPlayers;
	flecs::query<TurretComponent> destroyTurrets;
};