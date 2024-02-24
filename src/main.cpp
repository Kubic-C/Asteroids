#include "game.hpp"

/**
 * Current Bugs:
 *   - None
 */

struct LivesComponent {
     int lives = 0;
};

struct SpeedComponent {
    float speed = 10.0f;
};

struct SomeTag {};
struct FooComponent {
 int x = 0;
};

struct Entity {};

int main(int argc, char* argv[]) {
    ae::setConfigApplyCallback([](ae::Config& config){
        ticksPerSecond = (float)ae::dvalue(config, "ticksPerSecond", 60.0);
        playerSpeed = (float)ae::dvalue(config, "playerSpeed", 1.0);
        playerFireRate = (float)ae::dvalue(config, "playerFireRate", 1.0f);
        playerBulletRecoilMultiplier = (float)ae::dvalue(config, "playerBulletRecoilMultiplier", 0.2);
        playerBulletSpeed = (float)ae::dvalue(config, "playerBulletSpeed", 40.0);
        playerBaseHealth = (float)ae::dvalue(config, "playerBaseHealth", 1.0);
        blinkResetTime = (float)ae::dvalue(config, "blinkResetTime", 1.0);
        reviveImmunityTime = (float)ae::dvalue(config, "reviveImmunityTime", 5.0);
        initialLives = (int)ae::dvalue(config, "initialLives", 3);
        turretPrice = (i32)ae::dvalue(config, "turretPrice", 100);
        maxTurrets = (u32)ae::dvalue(config, "maxTurrets", 20);
        turretPlaceCooldown = (float)ae::dvalue(config, "turretPlaceCooldown", 1.0);
        turretRange = (float)ae::dvalue(config, "turretRange", 100.0);
        timePerAsteroidSpawn = (float)ae::dvalue(config, "timePerAsteroidSpawn", 2.0);
        timeToRemovePerAsteroidSpawn = (float)ae::dvalue(config, "timeToRemovePerAsteroidSpawn", 0.01);
        scorePerAsteroid = (u32)ae::dvalue(config, "scorePerAsteroid", 10);
        initialAsteroidStage = (u32)ae::dvalue(config, "initialAsteroidStage", 4);
        asteroidScalar = (float)ae::dvalue(config, "asteroidScalar", 8.0);
        asteroidDestroySpeedMultiplier = (float)ae::dvalue(config, "asteroidDestroySpeedMultiplier", 2.0);
        defaultHostPort = (int)ae::dvalue(config, "defaultHostPort", 9999);
        inputUPS = (float)ae::dvalue(config, "inputUPS", 30.0);
        stateUPS = (float)ae::dvalue(config, "stateUPS", 20.0);

        ae::log("Retrieved config file:\n");
        std::cout << std::setw(2) << config << std::endl;
    });
    ae::applyConfig();

    global = std::make_shared<Global>();
    if(!global->loadResources()) {
        ae::log(ae::ERROR_SEVERITY_FATAL, "Failed to load resources\n");
    }

	/* ALL NETWORKED COMPONENTS MUST BE DECLARED HERE! */
	ae::NetworkStateManager& networkStateManager = ae::getNetworkStateManager();
    networkStateManager.registerComponent<SharedLivesComponent>(ae::ComponentPiority::High);
    networkStateManager.registerComponent<ScoreComponent>(ae::ComponentPiority::High);
    networkStateManager.registerComponent<HealthComponent>();
    networkStateManager.registerComponent<ColorComponent>(ae::ComponentPiority::High);
    networkStateManager.registerComponent<PlayerColorComponent>(ae::ComponentPiority::High);
    networkStateManager.registerComponent<PlayerComponent>();
    networkStateManager.registerComponent<AsteroidComponent>();
    networkStateManager.registerComponent<BulletComponent>();
    networkStateManager.registerComponent<MapSizeComponent>(ae::ComponentPiority::High);
    networkStateManager.registerComponent<TurretComponent>();
    //networkStateManager.registerComponent<EnemyPlayerComponent>();

    {
        flecs::world& world = ae::getEntityWorld();

        world.prefab<prefabs::Player>()
            .override<PlayerComponent>()
            .override<HealthComponent>()
            .override<ae::IntegratableComponent>()
            .override<PlayerColorComponent>()
            .override<ae::ShapeComponent>()
            .set_override(ae::TransformComponent({ 300, 200 }))
            .set_override(ColorComponent(sf::Color::Red));

        world.prefab<prefabs::Turret>()
            .override<TurretComponent>()
            .override<ae::TransformComponent>();

        world.prefab<prefabs::Asteroid>()
            .override<AsteroidComponent>()
            .override<ae::TransformComponent>()
            .override<HealthComponent>()
            .override<ae::IntegratableComponent>()
            .set_override(ColorComponent(sf::Color::Red))
            .override<AsteroidComponent>();

        world.prefab<prefabs::Bullet>()
            .override<ae::NetworkedEntity>()
            .override<ae::TransformComponent>()
            .override<ae::IntegratableComponent>()
            .override<ae::ShapeComponent>()
            .set_override(ae::TimedDeleteComponent(1.0f))
            .set_override(ColorComponent(sf::Color::Yellow));
    }

    ae::registerState<MainMenuState>();
    ae::registerState<ConnectingState>();
    ae::registerState<ConnectedState>();
    ae::registerState<StartState>();
    ae::registerState<PlayState, PlayStateModule>();
    ae::registerState<GameOverState>();
    ae::registerNetworkInterfaceStateModule<ServerInterface, StartState, HostStartStateModule>();
    ae::registerNetworkInterfaceStateModule<ServerInterface, PlayState, HostPlayStateModule>();
    ae::registerNetworkInterfaceStateModule<ServerInterface, GameOverState, HostGameOverStateModule>();
    ae::transitionState<MainMenuState>();

    sf::Font font;
    if(!font.loadFromFile("./res/times.ttf"))
        ae::log(ae::ERROR_SEVERITY_FATAL, "Times new roman failed\n");

    sf::Text text(font);

    sf::VertexArray array(sf::PrimitiveType::Triangles);
	
    size_t ticks = 0;
    size_t totalWrite = 0;
    size_t totalRead = 0;
    ae::Ticker<void(float)> deltaNetworkStatsTicker;
    deltaNetworkStatsTicker.setFunction([&](float){
        ae::NetworkManager& networkManager = ae::getNetworkManager();

        ticks++;
        totalWrite += networkManager.getWrittenByteCount();
        totalRead += networkManager.getReadByteCount();
        networkManager.clearStats();
    });

    deltaNetworkStatsTicker.setRate(ticksPerSecond);

    float debugNetworkLogCooldown = 1.0f;
    float reapplyJSONCooldown = 1.0f;
    ae::Ticker<void(float)> inputUpdate;
    inputUpdate.setFunction([&](float dt) {
        if(ticks == 0)
            ticks = 1;

        debugNetworkLogCooldown -= dt;
        if(debugNetworkLogCooldown < 0.0f) {
            std::string lastNetworkReport = 
                ae::formatString("<cyan, bold>NETWORK<reset> (AVG by TICK) <green>WB: %llu<reset> <red>RB: %llu<reset>\n", totalWrite / ticks, totalRead / ticks);

            ae::log(lastNetworkReport);
            totalWrite = 0;
            totalRead = 0;
            ticks = 0;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F4)) {
                ae::log(ae::getNetworkStateManager().getNetworkedEntityInfo());
            }

            debugNetworkLogCooldown = 1.0f;
        }

        reapplyJSONCooldown -= dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F5) && reapplyJSONCooldown < 0.0f) {
            ae::applyConfig();
            reapplyJSONCooldown = 1.0f;
        }
    });

    inputUpdate.setRate(60.0f);

     ae::setUpdateCallback([&](){
        using namespace ae;
        flecs::world& world = getEntityWorld();
        PhysicsWorld& physicsWorld = getPhysicsWorld();
        sf::RenderWindow& window = getWindow();

        deltaNetworkStatsTicker.update();
        inputUpdate.update();

        bool debugShow = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F2);

        sf::Color outlineColor = sf::Color(54, 69, 79);
        world.each([&](flecs::entity e, TurretComponent& turret, TransformComponent& transform) {
            sf::CircleShape base;
            base.setPosition(transform.getPos());
            base.setFillColor(sf::Color::Yellow);
            base.setRadius(20.0f);
            base.setOutlineColor(outlineColor);
            base.setOutlineThickness(-2.0f);
            base.setOrigin(base.getGeometricCenter());

            sf::RectangleShape rectangle;
            rectangle.setSize(sf::Vector2f(20.0f, 10.0f));
            rectangle.setFillColor(sf::Color::Magenta);
            rectangle.setRotation(sf::radians(transform.getRot()));
            rectangle.setPosition(transform.getPos());
            rectangle.setOrigin(rectangle.getGeometricCenter());
            rectangle.setOutlineColor(outlineColor);
            rectangle.setOutlineThickness(-2.0f);

            if(debugShow) {
                sf::RectangleShape outline;
                outline.setFillColor(sf::Color::Transparent);
                outline.setOutlineColor(sf::Color::Red);
                outline.setOutlineThickness(-2.5f);
                outline.setPosition(transform.getPos() - sf::Vector2f(turretRange, turretRange));
                outline.setSize(sf::Vector2f(turretRange, turretRange) * 2.0f);
                window.draw(outline);
            }

            window.draw(base);
            window.draw(rectangle);
            });

        world.each([&](flecs::entity e, ShapeComponent& shape, ColorComponent& color) {
            if (!physicsWorld.doesShapeExist(shape.shape)) {
                ae::log("Invalid shape id %u - %u\n", e.id(), shape.shape);
                return;
            }

            Shape& physicsShape = physicsWorld.getShape(shape.shape);

            switch (physicsShape.getType()) {
            case ShapeEnum::Polygon: {
                Polygon& polygon = dynamic_cast<Polygon&>(physicsShape);

                ae::Polygon::vertices_t vertices = polygon.getWorldVertices();
                for (u8 i = 1; i + 1 < polygon.getVerticeCount(); i++) { 
                    array.append(sf::Vertex(vertices[0], color.getColor(), sf::Vector2f()));
                    array.append(sf::Vertex(vertices[i + 0], color.getColor(), sf::Vector2f()));
                    array.append(sf::Vertex(vertices[i + 1], color.getColor(), sf::Vector2f()));
                }

            } break;
            case ShapeEnum::Circle: {
                Circle& circle = dynamic_cast<Circle&>(physicsShape);

                sf::CircleShape sfShape(circle.getRadius());
                sfShape.setFillColor(color.getColor());
                sfShape.setPosition(circle.getPos());

                sfShape.setOutlineColor(outlineColor);
                sfShape.setOutlineThickness(-2.0f);

                window.draw(sfShape);
            } break;
            }

            if(debugShow) {
                if(e.has<NetworkedEntity>())
                    text.setFillColor(sf::Color::Green);
                else
                    text.setFillColor(sf::Color::Red);

                text.setString(ae::formatString("Entity: %lu", e.id() & ECS_ENTITY_MASK));
                text.setPosition(physicsShape.getPos());
                window.draw(text);
                text.setFillColor(sf::Color::Black);
            }
        });

        window.draw(array);
        array.clear();
	
        if (debugShow) {
            //u32 serverTick = ae::getCurrentTick();
            u32 networkCount = world.count<ae::NetworkedEntity>();

            //if(getNetworkManager().hasNetworkInterface<ae::ClientInterface>()) {
            //    serverTick = getNetworkManager().getNetworkInterface<ae::ClientInterface>().getCurrentServerTick();
            //}

            text.setPosition(sf::Vector2f((float)window.getSize().x / 2.0f, 0.0f));
            text.setOutlineThickness(0.5f);
            text.setOutlineColor(sf::Color::White);
            text.setFillColor(sf::Color::Black);
            text.setString(ae::formatString("Entity count: %u", (unsigned int)networkCount));
            window.draw(text);
        }
     });

    ae::mainLoop();

	return 0;
}