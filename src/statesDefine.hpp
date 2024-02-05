#pragma once
#include "game.hpp"

/*
if (isUserHost) {
		SteamNetworkingIPAddr listenAddr;
		listenAddr.SetIPv4(0, defaultHostPort);
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ServerOnSteamNetConnectionStatusChanged);
		SetListen(sockets->CreateListenSocketIP(listenAddr, 1, &opt));
	} else {
		SteamNetworkingIPAddr connect_addr;
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ClientOnSteamNetConnectionStatusChanged);

		do {
			std::string ipv4;
			std::cout << "What IP Address would you like to connect to?: ";
			std::cin >> ipv4;

			connect_addr.ParseString(ipv4.c_str());
			connect_addr.m_port = defaultHostPort;

			std::cout << "Attempting to connect. This may take some time.\n";
			HSteamNetConnection connection = sockets->ConnectByIPAddress(connect_addr, 1, &opt);
			if (connection == k_HSteamNetConnection_Invalid) {
				continue;
			} else {
				AddNewClient(connection);
				break;
			}
		} while (true);
	}
*/

/*
	if(!isUserHost){
		world.set_entity_range(clientEntityStartRange, 0);
		world.enable_range_check(true);
	} else {
		// Initial game stuff
		AddPlayerComponents(world.entity()).add<hostPlayer_t>().set([](color_t& color){ color.SetColor(sf::Color::Red); });

		world.add<asteroidTimer_t>();
	}
*/

// Get all necessary information such as:
// - Are we hosting or playing solo
// - If not what IP are we connecting to
class initialGame_t : public gameState_t {
public:
	// Hosting:
	//	- Remember to add isHost_t to flecs world
	//  - Game needs to be marked as host as well
	// Quickplay:
	//  - Same as hosting but we transition straight to the play_t state
	// Client:
	//	- Connect to the given IP

	class module_t {
	public:
		module_t(flecs::world& world) {}
	};

	void OnTransition() override {
		tgui::Gui& gui = game->GetGUI();

		if(game->IsNetworkActive() && !game->IsHost()) {
			game->SetNetworkActive(false);
			CreateClientMenu(gui);
			return;
		}
		
		CreatePlayerInfoMenu(gui);

#ifdef NDEBUG
		game->PlayMainTrack();
#endif
	}

	void OnDeTransition() override {
		if(!game->IsHost()) {
			messageManager_t& messageManager = game->GetMessageManager();

			message_t message;
			message.SetConnection(game->GetHostConnection());

			auto ser = message.StartSerialize();
			message.Serialize(ser, messageHeader_t::playerInfo);
			message.Serialize(ser, playerInfo);
			message.EndSerialize(ser);

			messageManager.PushOutgoing(std::move(message));
		} else {
			game->GetClient(hostPlayerID).player.set([&](playerColor_t& color){
				color.SetColor(playerInfo.playerColor);
			});
		}
	}

	void OnUpdate() override {}
	void OnTick() override {}

	gameStateEnum_t Enum() { return initialGame; }
private:
	static void OnHostButtonClick() {
		game->StartHosting();
		game->LoadRestOfState();
		game->GetGUI().removeAllWidgets();
		game->TransitionState(connecting);
	}

	static void OnClientButtonClick() {
		game->LoadRestOfState();
		game->GetCastState<initialGame_t>().CreateClientMenu(game->GetGUI());
	}

	static void OnQuickplayClick() {
		game->StartHosting();
		game->LoadRestOfState();
		game->GetGUI().removeAllWidgets();
		game->TransitionState(start);
	}

	static void OnConnectClick(tgui::EditBox::Ptr editBox) {
		game->TryConnect((std::string)editBox->getText());
		game->GetGUI().removeAllWidgets();
		game->TransitionState(connecting);
	}
public:
	void CreatePlayerInfoMenu(tgui::BackendGui& gui) {
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
			CreateMainMenu(gui);
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

	void CreateMainMenu(tgui::BackendGui& gui) {
		gui.removeAllWidgets();
		gui.setTextSize(32);

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

	void CreateClientMenu(tgui::BackendGui& gui) {
		gui.removeAllWidgets();

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
	playerInfo_t playerInfo;
};

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
		: timeLeft(1.0f), text(game->GetFont(), "Connection Failed!") {}

	virtual void OnUpdate() override {
		game->GetWindow().draw(text);
	}

	virtual void OnTick() override {
		timeLeft -= game->GetDeltaTime();
		if (timeLeft <= 0.0f) {
			game->RemoveClient(game->GetHostConnection());
			timeLeft = 1.0f;
			game->TransitionState(initialGame);
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

void UpdatePlayerReady(flecs::iter& iter, playerComp_t* players, color_t* colors, playerColor_t* playerColors);
void IsAllPlayersReady(flecs::iter& iter);
void OrientPlayers(flecs::iter& iter, transform_t* transforms);

class start_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			if(world.has<isHost_t>()) {
				world.system<playerComp_t, color_t, playerColor_t>().kind(flecs::OnUpdate).iter(UpdatePlayerReady);
				world.system().kind(flecs::PostUpdate).iter(IsAllPlayersReady);
				world.system<transform_t>().with<playerComp_t>().iter(OrientPlayers);
			}
		}
	};

	start_t()
		: text(game->GetFont(), "Both players must press R to start.") {
	}

	virtual void OnTransition() override {
		sf::Vector2f center = ((sf::Vector2f)game->GetWindow().getSize() / 2.0f) - text.getLocalBounds().getCenter();
		text.setPosition(center);
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
void PlayerPlayInputUpdate(flecs::iter& iter, playerComp_t* players, integratable_t* integratables, transform_t* transforms, health_t* healths);
void PlayerBlinkUpdate(flecs::iter& iter, playerComp_t* players, health_t* healths, color_t* colors, playerColor_t* playerColors);
void PlayerReviveUpdate(flecs::iter& iter, sharedLives_t* lives, playerComp_t* players, health_t* healths);
void AsteroidDestroyUpdate(flecs::iter& iter, asteroidComp_t* asteroids, transform_t* transforms, integratable_t* integratables, health_t* healths);
void TransformWrap(flecs::iter& iter, mapSize_t* size, transform_t* transforms);
void AsteroidAddUpdate(flecs::iter& iter, physicsWorldComp_t* physicsWorld, mapSize_t* mapSize, asteroidTimer_t* timer);
void TurretPlayUpdate(flecs::iter& iter, physicsWorldComp_t* physicsWorld, transform_t* transforms, turretComp_t* turrets);
void ObservePlayerCollision(flecs::iter& iter, size_t i, shapeComp_t&);
void ObserveBulletCollision(flecs::iter& iter, size_t i, shapeComp_t&);

class play_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			if(world.has<isHost_t>()) { // Client should not be able to handle State changes...
				world.system<health_t>().with<playerComp_t>().kind(flecs::OnUpdate).with(flecs::Disabled).optional().iter(UpdatePlayerDead);
				world.system().kind(flecs::PostUpdate).iter(IsAllPlayersDead);
				world.system<health_t>().iter(IsDead);
				world.system<physicsWorldComp_t, mapSize_t, asteroidTimer_t>().term_at(1).singleton().term_at(2).singleton().term_at(3).singleton().iter(AsteroidAddUpdate);
				world.system<asteroidComp_t, transform_t, integratable_t, health_t>().iter(AsteroidDestroyUpdate);
				world.system<sharedLives_t, playerComp_t, health_t>().term_at(1).singleton().iter(PlayerReviveUpdate);
				world.system<physicsWorldComp_t, transform_t, turretComp_t>().term_at(1).singleton().iter(TurretPlayUpdate);
				world.system<mapSize_t, transform_t>().term_at(1).singleton().iter(TransformWrap);
			}

			world.system<playerComp_t, integratable_t, transform_t, health_t>().iter(PlayerPlayInputUpdate);
			world.system<playerComp_t, health_t, color_t, playerColor_t>().iter(PlayerBlinkUpdate);
			world.observer<shapeComp_t>().event<collisionEvent_t>().with<playerComp_t>().each(ObservePlayerCollision);
			world.observer<shapeComp_t>().event<collisionEvent_t>().with<bulletComp_t>().each(ObserveBulletCollision);
		}
	};

	play_t() {}

	void OnTransition() override {
		CreateStats(game->GetGUI());
	}

	void OnDeTransition() override {
		game->GetGUI().removeAllWidgets();
	}

	virtual void OnUpdate() override {
		i32_t score = game->GetWorld().get<score_t>()->GetScore();
		u32_t lives = game->GetWorld().get<sharedLives_t>()->lives;
		text->setText(FormatString("Lives: %li\nScore:%u", lives, score));
	}

	virtual void OnTick() override {
	}

	virtual gameStateEnum_t Enum() override { return gameStateEnum_t::play; }

public:
	void AddDeadCount() { deadCount++; }
	void ResetDeadCount() { deadCount = 0; }
	int GetDeadCount() { return deadCount; }

private:
	void CreateStats(tgui::BackendGui& gui) {
		game->GetGUI().removeAllWidgets();
		text = tgui::Label::create();
		text->getRenderer()->setTextColor(sf::Color::White);
		gui.add(text);
	}

private:
	tgui::Label::Ptr text;
	int deadCount = 0;
};

class gameOver_t : public gameState_t {
public:
	class module_t {
	public:
		module_t(flecs::world& world) {
			// re-using systems from the start module
			if (world.has<isHost_t>()) {
				world.system<playerComp_t, color_t, playerColor_t>().kind(flecs::OnUpdate).iter(UpdatePlayerReady);
				world.system().kind(flecs::PostUpdate).iter(IsAllPlayersReady);
				world.system<transform_t>().with<playerComp_t>().iter(OrientPlayers);
			}
		}
	};

	gameOver_t()
		: text(game->GetFont(), "Game Over. Both players must\npress R to Reset") {

		destroyAsteroids = game->GetWorld().query_builder<asteroidComp_t>().build();
		destroyBullets = game->GetWorld().query_builder<bulletComp_t>().build();
		resetPlayers = game->GetWorld().query_builder<playerComp_t, integratable_t, health_t, color_t>()
			.with(flecs::Disabled).optional().build();
		destroyTurrets = game->GetWorld().query_builder<turretComp_t>().build();
	}

	virtual void OnTransition() override {
		sf::Vector2f center = ((sf::Vector2f)game->GetWindow().getSize() / 2.0f) - text.getLocalBounds().getCenter();
		text.setPosition(center);

		game->GetWorld().defer_begin();
		destroyAsteroids.each([](flecs::entity e, asteroidComp_t& asteroid) {
			e.add<deferDelete_t>();
		});
		destroyBullets.each([](flecs::entity e, bulletComp_t& asteroid) {
			e.add<deferDelete_t>();
		});
		destroyTurrets.each([](flecs::entity e, turretComp_t& turret) {
			e.add<deferDelete_t>();
		});

		std::vector<flecs::entity> entitiesToEnable;
		resetPlayers.each([&](flecs::entity e, playerComp_t& player, integratable_t& integratable, health_t& health, color_t& color) {
			player.SetIsReady(false);
			integratable.AddLinearVelocity(-integratable.GetLinearVelocity());
			health.SetDestroyed(false);
			health.SetHealth(1.0f);
			color.SetColor(sf::Color::Red);

			if(!e.enabled())
				entitiesToEnable.push_back(e);
		});
		game->GetWorld().defer_end();

		for(auto e : entitiesToEnable)
			game->EnableEntity(e);

		game->GetWorld().get_mut<sharedLives_t>()->lives = initialLives;
		game->GetWorld().get_mut<score_t>()->ResetScore();

		if(game->GetWorld().has<isHost_t>())
			game->GetWorld().get_mut<asteroidTimer_t>()->resetTime = timePerAsteroidSpawn;
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
	flecs::query<playerComp_t, integratable_t, health_t, color_t> resetPlayers;
	flecs::query<turretComp_t> destroyTurrets;
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