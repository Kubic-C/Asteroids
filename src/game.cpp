#include "game.hpp"
#include "statesDefine.hpp"

void Integrate(flecs::iter& iter, transform_t* transforms, integratable_t* integratables) {
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

void DeferDeleteUpdate(flecs::iter& iter) {
    for(auto i : iter) {
        iter.entity(i).destruct();
    }
}

void HostPlayerInputUpdate(flecs::iter& iter, gameKeyboard_t* keyboard, playerComp_t* players) {
    for (auto i : iter) {
        playerComp_t& player = players[i];

        player.SetMouse(keyboard->mouse);
        player.SetKeys(keyboard->keys);
    }
}

void GameKeyboardUpdate(flecs::iter& iter, gameKeyboard_t* keyboard, gameWindow_t* window) {
    if (!window->window->hasFocus()) {
        keyboard->keys = 0;
        return;
    }

    uint8_t keys = 0;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        keys |= inputFlagBits_t::LEFT;
    } else {
        keys &= ~inputFlagBits_t::LEFT;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        keys |= inputFlagBits_t::RIGHT;
    } else {
        keys &= ~inputFlagBits_t::RIGHT;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        keys |= inputFlagBits_t::UP;
    } else {
        keys &= ~inputFlagBits_t::UP;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
        keys |= inputFlagBits_t::DOWN;
    } else {
        keys &= ~inputFlagBits_t::DOWN;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R)) {
        keys |= inputFlagBits_t::READY;
    } else {
        keys &= ~inputFlagBits_t::READY;
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H)) {
        keys |= inputFlagBits_t::PLACE_TURRET;
    } else {
        keys &= ~inputFlagBits_t::PLACE_TURRET;
    }

    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        keys |= inputFlagBits_t::FIRE;
    } else {
        keys &= ~inputFlagBits_t::FIRE;
    }

    keyboard->keys = keys;
    keyboard->mouse = (sf::Vector2f)sf::Mouse::getPosition(*window->window);
}

void TimedDeleteUpdate(flecs::iter& iter, timedDelete_t* timers) {
    for(auto i : iter) {
        timers[i].timer -= iter.delta_time();
        if(timers[i].timer <= 0.0f) {
            iter.entity(i).add<deferDelete_t>();
        }
    }
}

inline void ImportSystems(flecs::world& world) {
    // Global systems. 
    world.system<transform_t, integratable_t>().iter(Integrate);
    world.system<gameKeyboard_t, playerComp_t>().term_at(1).singleton().term<hostPlayer_t>().iter(HostPlayerInputUpdate);
    world.system<gameKeyboard_t, gameWindow_t>().term_at(1).singleton().term_at(2).singleton().iter(GameKeyboardUpdate);
    world.system<timedDelete_t>().iter(TimedDeleteUpdate);
    world.system().term<deferDelete_t>().kind(flecs::PostUpdate).iter(DeferDeleteUpdate);
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

void global_t::TransitionState(gameStateEnum_t stateEnum) {
	if(state->Enum() == stateEnum) {
        return;
    }

    gameState_t& prevState = *state;
    gameState_t& newState = *states[stateEnum];

    prevState.moduleEntity.disable();
    prevState.OnDeTransition();
    newState.OnTransition();
    newState.moduleEntity.enable();
    state = states[stateEnum]; // cant get the addr of newState because the state var is std::shared_ptr

}

void global_t::EnableEntity(flecs::entity e) {
    e.enable();
    worldSnapshotBuilder->QueueEnableOrDisableUpdate(e, true);
}

void global_t::DisableEntity(flecs::entity e) {
    e.disable();
    worldSnapshotBuilder->QueueEnableOrDisableUpdate(e, false);
}

void ClientOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
    switch (pInfo->m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_None:
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
        game->TransitionState(gameStateEnum_t::connectionFailed);
        break;

    case k_ESteamNetworkingConnectionState_Connecting:
        game->TransitionState(gameStateEnum_t::connecting);
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        game->TransitionState(gameStateEnum_t::connected);
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
            game->TransitionState(gameStateEnum_t::connected);
        }

        break;
    case k_ESteamNetworkingConnectionState_Connected:
        game->AddNewClient(pInfo->m_hConn);
        break;
    default:
        break;
    }
}

void global_t::StartHosting() {
    SteamNetworkingIPAddr listenAddr;
    listenAddr.SetIPv4(0, defaultHostPort);
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ServerOnSteamNetConnectionStatusChanged);
    SetListen(sockets->CreateListenSocketIP(listenAddr, 1, &opt));

    game->SetNetworkActive(true);
}

bool global_t::TryConnect(std::string ipAddress) {
    SteamNetworkingIPAddr connect_addr;
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ClientOnSteamNetConnectionStatusChanged);

    connect_addr.ParseString(ipAddress.c_str());
    connect_addr.m_port = defaultHostPort;

    HSteamNetConnection connection = sockets->ConnectByIPAddress(connect_addr, 1, &opt);
    if (connection == k_HSteamNetConnection_Invalid) {
        ::exit(EXIT_FAILURE);
    } else {
        game->SetNetworkActive(true);
        AddNewClient(connection);
        return true;
    }
}

template<typename T>
void InitState(std::array<std::shared_ptr<gameState_t>, stateCount>& states, flecs::world& world) {

    std::shared_ptr<T> state = std::make_shared<T>();
    gameStateEnum_t stateEnum = state->Enum();

    states[stateEnum] = std::dynamic_pointer_cast<gameState_t>(state);
    states[stateEnum]->moduleEntity = world.import<T::module_t>();
    states[stateEnum]->moduleEntity.disable();
}

void global_t::LoadRestOfState() {
    if(IsHost()) {
        world.add<isHost_t>();
    }

    ImportNetwork(world, context, worldSnapshotBuilder, networkPipeline);
    InitState<connecting_t>(states, world);
    InitState<connectionFailed_t>(states, world);
    InitState<connected_t>(states, world);
    InitState<start_t>(states, world);
    InitState<play_t>(states, world);
    InitState<gameOver_t>(states, world);

    if(IsHost()) {
        world.add<asteroidTimer_t>();
        world.add<isNetworked_t>();
        world.set([&](mapSize_t& size) { size.SetSize(window->getSize()); });
        world.add<sharedLives_t>();
        world.add<score_t>();

        AddPlayerComponents(world.entity()).add<hostPlayer_t>().set([](color_t& color) { color.SetColor(sf::Color::Red); });
    } else {
        world.set_entity_range(clientEntityStartRange, 0);
        world.enable_range_check(true);
    }

    world.set([&](gameWindow_t& w) { w.window = window; });
    world.add<gameKeyboard_t>();
}

int global_t::Init() {
    if (!LoadResources()) {
        std::cout << "Failed to load resources.\n";
        return false;
    }
   
    NetworkInit();
    CreateWindow((u32_t)windowWidth, (u32_t)windowHeight, "Asteroids BY Kubic0x43");
    
    // Components must be defined FIRST
    world.component<isHost_t>();

    // Systems second..
    physicsWorldSnapshotBuilder = std::make_shared<physicsWorldSnapshotBuilder_t>(world);
    world.import<physicsModule_t>();
    physicsWorld = std::make_shared<physicsWorld_t>();
    world.set([&](physicsWorldComp_t& world) {world.physicsWorld = physicsWorld; });

    ImportSystems(world);
    
    // Start the game
    InitState<initialGame_t>(states, world);
    InitState<unknown_t>(states, world);
    state = states[unknown];
    TransitionState(gameStateEnum_t::initialGame);

    return true;
}

void global_t::Update() {
    sockets->RunCallbacks();

    // WINDOW EVENT

    sf::Event event;
    while (window->pollEvent(event)) {
        switch (event.type) {
        case event.Closed:
            exit = true;
            break;
        default:
            break;
        }

        gui->handleEvent(event);
    }

    // TICK

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
        time.tps++;
    }

    // UPDATE

    window->clear();
    state->OnUpdate();

    sf::Color outlineColor = sf::Color(54, 69, 79);
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

            sfShape.setOutlineColor(outlineColor);
            sfShape.setOutlineThickness(-2.0f);

            window->draw(sfShape);
        } break;
        case shapeEnum_t::circle: {
            circle_t& circle = dynamic_cast<circle_t&>(physicsShape);

            sf::CircleShape sfShape(circle.GetRadius());
            sfShape.setFillColor(color.GetColor());
            sfShape.setPosition(circle.GetPos());

            sfShape.setOutlineColor(outlineColor);
            sfShape.setOutlineThickness(-2.0f);

            window->draw(sfShape);
        } break;
        }
    });
    world.each([&](flecs::entity e, turretComp_t& turret, transform_t& transform) {
        sf::CircleShape base;
        base.setPosition(transform.GetPos());
        base.setFillColor(sf::Color::Yellow);
        base.setRadius(20.0f);
        base.setOutlineColor(outlineColor);
        base.setOutlineThickness(-2.0f);
        base.setOrigin(base.getGeometricCenter());

        sf::RectangleShape rectangle;
        rectangle.setSize(sf::Vector2f(20.0f, 10.0f));
        rectangle.setFillColor(sf::Color::Magenta);
        rectangle.setRotation(sf::radians(transform.GetRot()));
        rectangle.setPosition(transform.GetPos());
        rectangle.setOrigin(rectangle.getGeometricCenter());
        rectangle.setOutlineColor(outlineColor);
        rectangle.setOutlineThickness(-2.0f);

    #ifndef NDEBUG
        //sf::RectangleShape outline;
        //outline.setFillColor(sf::Color::Transparent);
        //outline.setOutlineColor(sf::Color::Red);
        //outline.setOutlineThickness(-2.5f);
        //outline.setPosition(transform.GetPos() - sf::Vector2f(turretRange, turretRange));
        //outline.setSize(sf::Vector2f(turretRange, turretRange) * 2.0f);
        //window->draw(outline);
    #endif

        window->draw(base);
        window->draw(rectangle);
    });

    /* GUI DRAW */
    gui->draw();

    window->display();
    time.fps++;

    if (networkActive) {
        if (IsHost()) {
            HostNetworkUpdate(time.frameTime);
        } else {
            ClientNetworkUpdate(time.frameTime);
        }

        messageManager.Update(clients, utils, sockets);
    }

    // Stats
    if(time.nextFrameCount < NowSeconds()) {
        time.nextFrameCount = NowSeconds() + 1.0f;
        
        stats.nups = time.nups;
        stats.fps = time.fps;
        stats.tps = time.tps;

        time.nups = 0;
        time.fps = 0;
        time.tps = 0;
    }
}

u64_t FixFields(u32_t id) {
    return id;
}

void global_t::DeserializeWorldSnapshot(flecs::world& world, networkEcsContext_t& context, message_t& message, deserializer_t& des) {
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

    u32_t enabledOrDisabledCount = 0;
    message.Deserialize(des, enabledOrDisabledCount);
    for(u32_t i = 0; i < enabledOrDisabledCount; i++) {
        std::pair<u32_t, u8_t> enabledOrDisabledEntity;
        message.Deserialize(des, enabledOrDisabledEntity);

        if(enabledOrDisabledEntity.second) {
            world.ensure(FixFields(enabledOrDisabledEntity.first)).enable();
        } else {
            world.ensure(FixFields(enabledOrDisabledEntity.first)).disable();
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
