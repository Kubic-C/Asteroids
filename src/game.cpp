#include "game.hpp"
#include "states.hpp"


inline void Integrate(flecs::iter& iter, transform_t* transforms, integratable_t* integratables) {
    for (auto i : iter) {
        transform_t& transform = transforms[i];
        integratable_t& integratable = integratables[i];

        transform.SetPos(transform.GetPos() + integratable.GetLinearVelocity() * iter.delta_time());
        transform.SetRot(transform.GetRot() + integratable.GetAngularVelocity() * iter.delta_time());

        if (integratable.IsSameAsLast()) {
            transform.dirty = false;
            integratable.dirty = false;
        }

    }
}

inline void IsDead(flecs::iter& iter, health_t* healths) {
    for (auto i : iter) {
        if (healths[i].GetHealth() < 0.0f) {
            healths[i].SetDestroyed(true);
        }
    }
}

inline void PlayerPlayInputUpdate(flecs::iter& iter, playerComp_t* players, integratable_t* integratables, transform_t* transforms) {
    float deltaTime = iter.delta_time();

    for (auto i : iter) {
        playerComp_t& player = players[i];
        integratable_t& integratable = integratables[i];
        transform_t& transform = transforms[i];

        player.AddTimer(deltaTime);

        if (player.GetMouse() != sf::Vector2f())
            transform.SetRot((transform.GetPos() - player.GetMouse()).angle().asRadians());

        if (player.IsUpPressed()) {
            integratable.AddLinearVelocity({ 0.0f, -playerSpeed });
        }
        if (player.IsDownPressed()) {
            integratable.AddLinearVelocity({ 0.0f, playerSpeed });
        }
        if (player.IsLeftPressed()) {
            integratable.AddLinearVelocity({ -playerSpeed, 0.0f });
        }
        if (player.IsRightPressed()) {
            integratable.AddLinearVelocity({ playerSpeed, 0.0f });
        }

        if (player.IsFirePressed() && player.GetLastFired() > playerFireRate) {
            player.ResetLastFired();
            player.SetIsFiring(true);

            if (iter.world().has<isHost_t>())
                iter.world().entity().set(
                    [&](transform_t& ctransform, integratable_t& integratable, color_t& color, shapeComp_t& shape) {
                        physicsWorld_t& world = *iter.world().get<physicsWorldComp_t>()->physicsWorld;

                        ctransform = transform;
                        color.SetColor(sf::Color::Yellow);

                        sf::Vector2f velocityDir = player.GetMouse() - transform.GetPos();
                        integratable.AddLinearVelocity(velocityDir);

                        shape.shape = world.CreateShape<circle_t>(5.0f);
                    }).add<bulletComp_t>().add<isNetworked_t>();
        }
        else {
            player.SetIsFiring(false);
        }
    }
}

inline void PlayerBlinkUpdate(flecs::iter& iter, playerComp_t* players, health_t* healths, color_t* colors) {
    for (auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];
        color_t& color = colors[i];

        if (player.GetLastBlink() <= 0.0f && healths->IsDestroyed()) {
            player.ResetLastBlink();
            color.SetColor(sf::Color::Blue);
        }
        else if (color.GetColor() != sf::Color::Green) {
            color.SetColor(sf::Color::Green);
        }
    }
}

inline void PlayerReviveUpdate(flecs::iter& iter, sharedLives_t* lives, playerComp_t* players, health_t* healths) {
    for (auto i : iter) {
        playerComp_t& player = players[i];
        health_t& health = healths[i];

        if (health.IsDestroyed() && lives->lives != 0) {
            if (player.GetTimeTillRevive() <= 0.0f) {
                player.ResetTimeTillRevive();
                health.SetDestroyed(false);
                lives->lives--;
            }
        }
    }
}

inline void AsteroidDestroyUpdate(flecs::iter& iter, asteroidComp_t*, health_t* healths) {
    for(auto i : iter) {
        health_t& health = healths[i];

        if(health.IsDestroyed()) {
            iter.entity(i).add<deferDelete_t>();
        }
    }
}

inline void TransformWrap(flecs::iter& iter, mapSize_t* size, transform_t* transforms) {
    for (auto i : iter) {
        transform_t& transform = transforms[i];
        sf::Vector2f pos = transform.GetPos();

        if (pos.x >= size->GetWidth()) {
            pos.x = 0.0f;
        }
        else if (pos.x <= 0.0f) {
            pos.x = size->GetWidth();
        }

        if (pos.y >= size->GetHeight()) {
            pos.y = 0.0f;
        }
        else if (pos.y <= 0.0f) {
            pos.y = size->GetHeight();
        }

        if (pos != transform.GetPos())
            transform.SetPos(pos);
    }
}

inline void HostPlayerInputUpdate(flecs::iter& iter, gameKeyboard_t* keyboard, playerComp_t* players) {
    for (auto i : iter) {
        playerComp_t& player = players[i];

        player.SetMouse(keyboard->mouse);
        player.SetKeys(keyboard->keys);
    }
}

inline void GameKeyboardUpdate(flecs::iter& iter, gameKeyboard_t* keyboard, gameWindow_t* window) {
    if (!window->window->hasFocus()) {
        keyboard->keys = 0;
        return;
    }

    uint8_t keys = 0;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
        keys |= inputFlagBits_t::LEFT;
    }
    else {
        keys &= ~inputFlagBits_t::LEFT;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
        keys |= inputFlagBits_t::RIGHT;
    }
    else {
        keys &= ~inputFlagBits_t::RIGHT;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
        keys |= inputFlagBits_t::UP;
    }
    else {
        keys &= ~inputFlagBits_t::UP;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
        keys |= inputFlagBits_t::DOWN;
    }
    else {
        keys &= ~inputFlagBits_t::DOWN;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::R)) {
        keys |= inputFlagBits_t::READY;
    }
    else {
        keys &= ~inputFlagBits_t::READY;
    }
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        keys |= inputFlagBits_t::FIRE;
    }
    else {
        keys &= ~inputFlagBits_t::FIRE;
    }

    keyboard->keys = keys;
    keyboard->mouse = (sf::Vector2f)sf::Mouse::getPosition(*window->window);
}

inline void AsteroidAddUpdate(flecs::iter& iter, physicsWorldComp_t* physicsWorld, mapSize_t* mapSize, asteroidTimer_t* timer) {
    timer->current -= iter.delta_time();
    if(timer->current < 0.0f) {
        timer->current = timer->resetTime;
        timer->resetTime -= timeToRemovePerAsteroidSpawn;
        
        float spawnX = (RandomFloat() * mapSize->GetWidth());
        float spawnY = (RandomFloat() * mapSize->GetHeight());

        float wallX = 0.0f;
        float distX = 0.0f;
        float distToLeft = spawnX;
        float distToRight = mapSize->GetWidth() - spawnX;
        if (distToLeft < distToRight) {
            wallX = mapSize->GetWidth() - 0.01f;
            distX = distToRight;
        }
        else {
            distX = distToLeft;
        }

        float wallY = 0.0f;
        float distY = 0.0f;
        float distToUp = spawnY;
        float distToDown = mapSize->GetHeight() - spawnY;
        if (distToUp < distToDown) {
            wallY = mapSize->GetHeight() - 0.1f;
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

        iter.world().entity()
            .add<isNetworked_t>()
            .add<asteroidComp_t>()
            .add<transform_t>()
            .add<health_t>()
            .add<integratable_t>()
            .add<color_t>()
            .add<asteroidComp_t>()
            .set([&](shapeComp_t& shape, transform_t& transform, integratable_t& integratable, color_t& color){
                    transform.SetPos({ spawnX, spawnY });

                    sf::Vector2f center = mapSize->GetSize() / 2.0f;
                    sf::Vector2f velToCenter = (transform.GetPos() - center).normalized();
                    integratable.AddLinearVelocity(velToCenter * 10.0f);

                    color.SetColor(sf::Color::Red);
                
                    shape.shape = physicsWorld->physicsWorld->CreateShape<polygon_t>();
                    polygon_t& polygon = physicsWorld->physicsWorld->GetPolygon(shape.shape);

                    std::vector<sf::Vector2f> vertices = GenerateRandomConvexShape(8, 8.0f);
                    polygon.SetVertices(vertices.size(), vertices.data());
                    polygon.SetPos(transform.GetPos());
                });
    }
}

void ObservePlayerCollision(flecs::iter& iter, size_t i, shapeComp_t&) {
    flecs::entity entity = iter.entity(i);
    collisionEvent_t& event = *iter.param<collisionEvent_t>();
    flecs::entity other = event.entityOther;

    if(other.has<asteroidComp_t>())
        entity.set([](health_t& health){ health.SetHealth(0.0f); health.SetDestroyed(true); });
}

void ObserveBulletCollision(flecs::iter& iter, size_t i, shapeComp_t&) {
    flecs::entity entity = iter.entity(i);
    collisionEvent_t& event = *iter.param<collisionEvent_t>();
    flecs::entity other = event.entityOther;

    if(other.has<asteroidComp_t>()) {
        other.set([&](health_t& health) {
            health.SetHealth(health.GetHealth() - entity.get<bulletComp_t>()->damage);
        });

        entity.add<deferDelete_t>();
    }
}

void DeferDeleteUpdate(flecs::iter& iter) {
    for(auto i : iter) {
        iter.entity(i).destruct();
    }
}

inline void ImportSystems(flecs::world& world) {
    world.system<transform_t, integratable_t>().iter(Integrate);
    world.system<health_t>().iter(IsDead);
    world.system<playerComp_t, integratable_t, transform_t>().iter(PlayerPlayInputUpdate);
    world.system<playerComp_t, health_t, color_t>().iter(PlayerBlinkUpdate);
    world.system<sharedLives_t, playerComp_t, health_t>().term_at(1).singleton().iter(PlayerReviveUpdate);
    world.system<mapSize_t, transform_t>().term_at(1).singleton().iter(TransformWrap);
    world.system<gameKeyboard_t, playerComp_t>().term_at(1).singleton().term<hostPlayer_t>().iter(HostPlayerInputUpdate);
    world.system<gameKeyboard_t, gameWindow_t>().term_at(1).singleton().term_at(2).singleton().iter(GameKeyboardUpdate);
    world.system<physicsWorldComp_t, mapSize_t, asteroidTimer_t>().term_at(1).singleton().term_at(2).singleton().term_at(3).singleton().iter(AsteroidAddUpdate);
    world.system<asteroidComp_t, health_t>().iter(AsteroidDestroyUpdate);

    world.system().term<deferDelete_t>().kind(flecs::PostUpdate).iter(DeferDeleteUpdate);

    world.observer<shapeComp_t>().event<collisionEvent_t>().with<playerComp_t>().each(ObservePlayerCollision);
    world.observer<shapeComp_t>().event<collisionEvent_t>().with<bulletComp_t>().each(ObserveBulletCollision);
}

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
            break;
        }

        if(game->GetState().Enum() == gameStateEnum_t::connecting) {
            game->TransitionState<connected_t>();
        }

        break;
    case k_ESteamNetworkingConnectionState_Connected:
        game->AddNewClient(pInfo->m_hConn);
        break;
    default:
        break;
    }
}

struct xyz_t {};

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
    
    physicsWorldSnapshotBuilder = std::make_shared<physicsWorldSnapshotBuilder_t>(world);

    ImportNetwork(world, context, worldSnapshotBuilder, networkPipeline);
    ImportSystems(world);

    world.import<physicsModule_t>();

    world.set([&](mapSize_t& size){ size.SetSize(window->getSize()); });
    world.add<sharedLives_t>();
    world.set([&](gameWindow_t& w){ w.window = window; });
    world.add<gameKeyboard_t>();
    
    physicsWorld = std::make_shared<physicsWorld_t>();
    world.set([&](physicsWorldComp_t& world){world.physicsWorld = physicsWorld;});

    prefabs.player = world.prefab()
        .add<isNetworked_t>()
        .add<playerComp_t>()
        .add<transform_t>()
        .add<health_t>()
        .add<integratable_t>()
        .add<color_t>()
        .add<shapeComp_t>()
        .set([&](color_t& color){ color.SetColor(sf::Color::Red); })
        .set([](transform_t& transform){ transform.SetPos({300, 200}); });

    if(!isUserHost){
        world.set_entity_range(clientEntityStartRange, 0);
        world.enable_range_check(true);
    } else {
        // Initial game stuff
        AddPlayerComponents(world.entity()).add<hostPlayer_t>().set([](color_t& color){ color.SetColor(sf::Color::Green); });

        world.add<asteroidTimer_t>();
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
    sockets->RunCallbacks();

    sf::Event event;
    if (window->pollEvent(event)) {
        // handle window events ...
        switch (event.type) {
        case event.Closed:
            exit = true;
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

    world.each([&](flecs::entity e, shapeComp_t& shape, color_t& color){
        if(!physicsWorld->DoesShapeExist(shape.shape))
            return;

        shape_t& physicsShape = physicsWorld->GetShape(shape.shape);
        switch(physicsShape.GetType()) {
        case shapeEnum_t::polygon: {
            polygon_t& polygon = dynamic_cast<polygon_t&>(physicsShape);

            sf::ConvexShape sfShape(polygon.GetVerticeCount());
            sfShape.setFillColor(color.GetColor());
            for (u8_t i = 0; i < polygon.GetVerticeCount(); i++) {
                sfShape.setPoint(i, polygon.GetWorldVertices()[i]);
            }

            window->draw(sfShape);
        } break;
        case shapeEnum_t::circle: {
            circle_t& circle = dynamic_cast<circle_t&>(physicsShape);

            sf::CircleShape sfShape(circle.GetRadius());
            sfShape.setFillColor(color.GetColor());
            sfShape.setPosition(circle.GetPos());

            window->draw(sfShape);
        } break;
        }
    });

    sf::Text eid(res.font);
    world.each([&](flecs::entity e, transform_t& transform, color_t& color) {
        sf::CircleShape shape(5.0f);
        shape.setFillColor(color.GetColor());
        shape.setPosition(transform.GetPos());
        window->draw(shape);

        eid.setPosition(transform.GetPos());
        eid.setString(std::to_string(e.id()));
        window->draw(eid);
        });


    sf::Text deltaTimeText(res.font, std::to_string(time.deltaTime) + " | NE: " + std::to_string(world.count<isNetworked_t>()) + " | SC " + std::to_string(world.count<shapeComp_t>()));
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

u64_t FixFields(u32_t id) {
    return id;
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

    std::vector<u32_t> entityComponentTypes; // placing outside of the for loop saves calls to malloc and free
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
            u32_t entityId = 0;
            message.Deserialize(des, entityId);

            for(u32_t typeI = 0; typeI < entityComponentTypeCount; typeI++) {
                u64_t componentType = FixFields(entityComponentTypes[typeI]);
                flecs::entity entity = world.ensure(FixFields(entityId)).add<isNetworked_t>();

                context.Deserialize(des, componentType, entity.get_mut(componentType));
            }
        }
    }

    u32_t destroyedEntitiesCount = 0;
    message.Deserialize(des, destroyedEntitiesCount);
    for(u64_t i = 0; i < destroyedEntitiesCount; i++) {
        u32_t id = 0;
        message.Deserialize(des, id);
        if(world.exists(FixFields(id)))
            world.entity(FixFields(id)).destruct();
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
            u32_t entityId = 0;
            message.Deserialize(des, entityId);

            for (u32_t typeI = 0; typeI < entityComponentTypeCount; typeI++) {
                u64_t componentType = FixFields(entityComponentTypes[typeI]);
                flecs::entity entity = world.ensure(FixFields(entityId));

                entity.remove(componentType);
            }
        }
    }

    world.enable_range_check(true);
}

void global_t::DeserializePhysicsSnapshot(physicsWorld_t& physicsWorld, message_t& message, deserializer_t& des) {
    
    // polygons are deserialized first then circles

    u32_t listSize;
    message.Deserialize(des, listSize);
    for(u32_t i = 0; i < listSize; i++) {
        u32_t shapeId;
        message.Deserialize(des, shapeId);

        polygon_t* polygon;

        if(physicsWorld.DoesShapeExist(shapeId)) {
            polygon = &physicsWorld.GetPolygon(shapeId);
        } else {
            polygon = &physicsWorld.GetPolygon(physicsWorld.InsertShape<polygon_t>(shapeId));
        }

        message.Deserialize(des, *polygon);
        polygon->MarkLocalDirty();
    }

    message.Deserialize(des, listSize);
    for (u32_t i = 0; i < listSize; i++) {
        u32_t shapeId;
        message.Deserialize(des, shapeId);

        circle_t* circle;

        if (physicsWorld.DoesShapeExist(shapeId)) {
            circle = &physicsWorld.GetCircle(shapeId);
        }
        else {
            circle = &physicsWorld.GetCircle(physicsWorld.InsertShape<circle_t>(shapeId));
        }

        message.Deserialize(des, *circle);
        circle->MarkLocalDirty();
    }

}