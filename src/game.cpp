#include "game.hpp"
#include "states.hpp"

sf::Vector2f GetMouseWorldCoords() {
	sf::Vector2i mouseScreenCoords = sf::Mouse::getPosition(game->GetWindow());

	// Since we have not set the view, there is no transformation to be done.
	return (sf::Vector2f)mouseScreenCoords;
}

bool RandomBool() {
    return (bool)(rand() % 2);
}

std::vector<sf::Vector2f> GenerateRandomConvexShape(int size, float scale) {
    // Generate two lists of random X and Y coordinates
    std::vector<float> xPool;
    std::vector<float> yPool;

    for (int i = 0; i < size; i++) {
        xPool.push_back(RandomFloat() * 8.0f + 1.0f);
        yPool.push_back(RandomFloat() * 8.0f + 1.0f);
    }

    // Sort them
    std::sort(xPool.begin(), xPool.end());
    std::sort(yPool.begin(), yPool.end());

    // Isolate the extreme points
    float minX = xPool[0];
    float maxX = xPool[size - 1];
    float minY = yPool[0];
    float maxY = yPool[size - 1];

    // Divide the interior points into two chains & Extract the vector components
    std::vector<float> xVec;
    std::vector<float> yVec;

    float lastTop = minX, lastBot = minX;
    for (int i = 1; i < size - 1; i++) {
        float x = xPool[i];

        if (RandomBool()) {
            xVec.push_back(x - lastTop);
            lastTop = x;
        }
        else {
            xVec.push_back(lastBot - x);
            lastBot = x;
        }
    }

    xVec.push_back(maxX - lastTop);
    xVec.push_back(lastBot - maxX);

    float lastLeft = minY, lastRight = minY;
    for (int i = 1; i < size - 1; i++) {
        float y = yPool[i];

        if (RandomBool()) {
            yVec.push_back(y - lastLeft);
            lastLeft = y;
        }
        else {
            yVec.push_back(lastRight - y);
            lastRight = y;
        }
    }

    yVec.push_back(maxY - lastLeft);
    yVec.push_back(lastRight - maxY);

    // Randomly pair up the X- and Y-components
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(yVec.begin(), yVec.end(), g);

    // Combine the paired up components into vectors
    std::vector<sf::Vector2f> vec;

    for (int i = 0; i < size; i++) {
        vec.push_back({xVec[i], yVec[i]});
    }

    // Sort the vectors by angle
    std::sort(vec.begin(), vec.end(),
        [&](sf::Vector2f v1, sf::Vector2f v2) {
            return v1.angle() < v2.angle();
        });

    // Lay them end-to-end
    float x = 0, y = 0;
    float minPolygonX = 0;
    float minPolygonY = 0;
    std::vector<sf::Vector2f> points;

    for (int i = 0; i < size; i++) {
        points.push_back({x, y});

        x += vec[i].x;
        y += vec[i].y;

        minPolygonX = std::min(minPolygonX, x);
        minPolygonY = std::min(minPolygonY, y);
    }

    // Move the polygon to the original min and max coordinates
    float xShift = minX - minPolygonX;
    float yShift = minY - minPolygonY;

    for (int i = 0; i < size; i++) {
        sf::Vector2f p = points[i];
        points[i] = sf::Vector2f(p.x + xShift, p.y + yShift) * scale;
    }

    return points;
}

std::vector<sf::Vector2f> GetRandomPregeneratedConvexShape(float scale) {
    auto shape = *(asteroidHulls.begin() + (rand() % asteroidHulls.size()));

    for(int i = 0; i < shape.size(); i++) {
        shape[i].x *= scale;
        shape[i].y *= scale;
    }

    return shape;
}

void global_t::TransitionState(gameStateEnum_t state) {
	switch (state) {
	case gameStateEnum_t::connecting:
		TransitionState<connecting_t>();
		break;
	case gameStateEnum_t::connectionFailed:
		TransitionState<connectionFailed_t>();
		break;
	case gameStateEnum_t::connected:
		TransitionState<connected_t>();
		break;
	case gameStateEnum_t::start:
		TransitionState<start_t>();
		break;
	case gameStateEnum_t::play:
		TransitionState<play_t>();
		break;
	case gameStateEnum_t::gameOver:
		TransitionState<gameOver_t>();
		break;
	default:
		break;
	}
}

void ClientOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
    switch (pInfo->m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_None:
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
        game->TransitionState<connectionFailed_t>();
        break;

    case k_ESteamNetworkingConnectionState_Connecting:
        game->TransitionState<connecting_t>();
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        game->TransitionState<connected_t>();
        break;

    default:
        break;
    }
}

void ServerOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
    switch (pInfo->m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_None:
        break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        game->RemoveClient(pInfo->m_hConn);

        game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
        break;
    case k_ESteamNetworkingConnectionState_Connecting:
        if (game->sockets->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
            game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
            game->TransitionState<connectionFailed_t>();
            break;
        }

        game->TransitionState<connecting_t>();
        break;
    case k_ESteamNetworkingConnectionState_Connected:
        game->AddNewClient(pInfo->m_hConn);
        game->TransitionState<connected_t>();
        break;
    default:
        break;
    }
}

int global_t::Init() {
    if (!LoadResources()) {
        std::cout << "Failed to load resources.\n";
        return false;
    }

    bool isUserHost = false;
    do {
        std::string in;
        std::cout << "Are you hosting(y/n): ";
        std::cin >> in;
        if (in.empty() || in.size() > 1) {
            std::cout << "Enter a valid response.\n";
            continue;
        }
        if (in[0] == 'y') {
            isUserHost = true;
            break;
        }
        else if (in[0] == 'n') {
            isUserHost = false;
            break;
        }
        else {
            std::cout << "Enter a valid response.\n";
            continue;
        }

    } while (true);

    // to prevent freezing, we initialize all data after the neccessary terminal input
    // is entered

    NetworkInit();

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

    std::string additionalInfo = "";
    if (isUserHost) {
        additionalInfo += "Host";
    }
    else {
        additionalInfo += "Client";
    }

    CreateWindow(600, 400, "Asteroids " + additionalInfo);
    TransitionState<connecting_t>();

    if(isUserHost)
        world.add<isHost_t>();
    else 
        world.component<isHost_t>();

    ImportNetwork(world, context, worldSnapshotBuilder, networkPipeline);
    ImportComponents(world);
    ImportSystems(world);

    world.set([&](mapSize_t& size){ size.SetSize(window->getSize()); });
    world.add<sharedLives_t>();
    world.set([&](gameWindow_t& w){ w.window = window; });

    if(isUserHost) {
        player = world.entity()
            .add<isNetworked_t>()
            .add<playerComp_t>()
            .add<transform_t>()
            .add<health_t>()
            .add<integratable_t>()
            .add<color_t>()
            .add<hostPlayer_t>()
            .set([&](color_t& color){ if(isUserHost) { color.SetColor(sf::Color::Red); } else { color.SetColor(sf::Color::Green); } })
            .set([](transform_t& transform){ transform.SetPos({300, 200}); });
    } else {
        world.set_entity_range(clientEntityStartRange, 0);
        world.enable_range_check(true);
    }

    // for debugging purposes
    std::string json = world.to_json();

    std::ofstream jsonFile("worldEcs.json");
    if(!jsonFile.is_open()) {
        // it's non-fatal, carry on with program execution
        std::cout << "Json file could not be written to\n";
    } else {
        jsonFile << json;
        jsonFile.close();
    }

    return true;
}

void global_t::Update() {
    {
        sockets->RunCallbacks();

        sf::Event event;
        if (window->pollEvent(event)) {
            // handle window events ...
            switch (event.type) {
            case event.Closed:
                exit = true;
                break;
            case event.KeyPressed:
                if(event.key.code == sf::Keyboard::Key::R) {
                    player.remove<health_t>();
                } else if(event.key.code == sf::Keyboard::Key::Q) {
                    player = world.entity()
                        .add<isNetworked_t>()
                        .add<playerComp_t>()
                        .add<transform_t>()
                        .add<health_t>()
                        .add<integratable_t>()
                        .add<color_t>()
                        .add<hostPlayer_t>()
                        .set([&](color_t& color) { color.SetColor(sf::Color::Red); })
                        .set([](transform_t& transform) { transform.SetPos({ 300, 200 }); });
                }
                break;
            default:
                break;
            }
        }

        assert(state);
        float now = NowSeconds();
        time.frameTime = now - time.lastFrame;
        time.ticksToDo += time.frameTime * ticksPerSecond;
        time.lastFrame = now;

        if (state)
            std::cout << "State: " << (int)state->Enum() << std::endl;

        while (time.ticksToDo >= 1.0f) {
            now = NowSeconds();
            time.deltaTime = now - time.lastTick;
            time.lastTick = now;

            state->OnTick();
            world.progress(time.deltaTime);
            time.ticksToDo--;
        }

        window->clear();
        state->OnUpdate();
        sf::Text eid(res.font);
        world.each([&](flecs::entity e, transform_t& transform, color_t& color){
            sf::CircleShape shape(5.0f);
            shape.setFillColor(color.GetColor());
            shape.setPosition(transform.GetPos());
            window->draw(shape);

            eid.setPosition(transform.GetPos());
            eid.setString(std::to_string(e.id()));
            window->draw(eid);
        });

        sf::Text deltaTimeText(res.font, std::to_string(time.deltaTime) + " | NE: " + std::to_string(world.count<isNetworked_t>()));
        window->draw(deltaTimeText);

        window->display();

        if (state->Enum() != gameStateEnum_t::connecting) {
            if (IsHost()) {
                HostNetworkUpdate(time.frameTime);
            } else {
                ClientNetworkUpdate(time.frameTime);
            }

            messageManager.Update(clients, utils, sockets);
        }
    }
}

void global_t::DeserializeWorldSnapshot(flecs::world& world, networkEcsContext_t& context, message_t& message, deserializer_t& des) {
    /**
        * What is known:
        * - Size of serialized components is known on both sides of the connection; their >>>SERIALIZED<<< sizes are the same.
        *  
        * Below is how entity and their components are serialized in messages.
        * 
        * b - bytes
        * ukn - unknown byte usage
        * ... - repeat above pattern
        * 
        * Byte size | Name
        * 4b | Entity archetype count
        *  vvv Archetype table 1 vvv 
        * 4b | Entity component type count
        * 4b | Component type 1
        * 4b | Component type 2
        * 4b | ... May continue ...
        * 4b | Entiy count
        * 8b | Entity id
        * ukn | Component data 1
        * ukn | Component data 2
        * ukn | Component data 3
        * vvv Archetype table 2 vvv 
        * 4b | Entity component type count
        * 4b | Component type 1
        * 4b | ... May continue ...
        * 4b | Entiy count
        * 8b | Entity id
        * ukn | Component data 1
        * 8b | Entity id
        * ukn | Component data 1
        */

    world.enable_range_check(false);

    u32_t entityArchetypeCount = 0;
    message.Deserialize(des, entityArchetypeCount);

    std::vector<u64_t> entityComponentTypes; // placing outside of the for loop saves calls to malloc and free
    for(u32_t i = 0; i < entityArchetypeCount; i++) {
        entityComponentTypes.clear();

        u32_t entityComponentTypeCount = 0;
        message.Deserialize(des, entityComponentTypeCount);
        entityComponentTypes.resize(entityComponentTypeCount);
        for(u32_t j = 0; j < entityComponentTypeCount; j++) {
            message.Deserialize(des, entityComponentTypes[j]);
            if(!world.exists(entityComponentTypes[j])) {
                // Invalid component type...
                message.EndDeserialization(des);
                assert(!("Invalid component type was recieved: " + std::to_string(entityComponentTypes[j])).c_str());
                return;
            }
        }

        u32_t entityCount = 0;
        message.Deserialize(des, entityCount);
        for(u32_t j = 0; j < entityCount; j++) {
            u64_t entityId = 0;
            message.Deserialize(des, entityId);

            for(u32_t typeI = 0; typeI < entityComponentTypeCount; typeI++) {
                u64_t componentType = entityComponentTypes[typeI];
                flecs::entity entity = world.ensure(entityId);

                context.Deserialize(des, componentType, entity.get_mut(componentType));
            }
        }
    }

    u32_t destroyedEntitiesCount = 0;
    message.Deserialize(des, destroyedEntitiesCount);
    for(u64_t i = 0; i < destroyedEntitiesCount; i++) {
        u64_t id = 0;
        message.Deserialize(des, id);
        if(world.exists(id))
            world.entity(id).destruct();
        else
            std::cout << "Possible d-sync. Server requested removal of entity(" << id << "). Entity not found.\n";
    }

    u32_t destroyedArchetypes = 0;
    message.Deserialize(des, destroyedArchetypes);
    for(u32_t i = 0; i < destroyedArchetypes; i++) {
        u32_t entityComponentTypeCount = 0;
        message.Deserialize(des, entityComponentTypeCount);
        entityComponentTypes.resize(entityComponentTypeCount);
        for (u32_t j = 0; j < entityComponentTypeCount; j++) {
            message.Deserialize(des, entityComponentTypes[j]);
            if (!world.exists(entityComponentTypes[j])) {
                // Invalid component type...
                message.EndDeserialization(des);
                assert(!("Invalid component type was recieved: " + std::to_string(entityComponentTypes[j])).c_str());
                return;
            }
        }

        u32_t entityCount = 0;
        message.Deserialize(des, entityCount);
        for (u32_t j = 0; j < entityCount; j++) {
            u64_t entityId = 0;
            message.Deserialize(des, entityId);

            for (u32_t typeI = 0; typeI < entityComponentTypeCount; typeI++) {
                u64_t componentType = entityComponentTypes[typeI];
                flecs::entity entity = world.ensure(entityId);

                entity.remove(componentType);
            }
        }
    }

    world.enable_range_check(true);
}