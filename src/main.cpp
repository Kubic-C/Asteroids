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

#ifdef NDEBUG
#define BUILD_MODE_STR "Release"
#else 
#define BUILD_MODE_STR "Debug"
#endif

int main(int argc, char* argv[]) {
    ae::log("<red, bold>Note:\n  -<reset> This software was created by <green,bold>Sawyer Porter (Kubic0x43)<reset>\n");
    ae::log("<red, bold>  -<reset, bold> All files located alongside the Asteroids executable are welcome to be copied and/or modified<reset>\n");
    ae::log("<red, bold>  -<reset> <cyan>Find source code at: <it>https://github.com/Kubic-C/Asteroids<reset>\n");
    ae::log("<red, bold>  -<reset> This version of Asteroids was created in <yellow>%s at %s<reset>\n", BUILD_MODE_STR, __TIMESTAMP__);

    ae::setConfigApplyCallback([](ae::Config& jConfig){
        config.playerSpeed = (float)ae::dvalue(jConfig, "playerSpeed", 1.0);
        config.playerFireRate = (float)ae::dvalue(jConfig, "playerFireRate", 1.0f);
        config.playerBulletRecoilMultiplier = (float)ae::dvalue(jConfig, "playerBulletRecoilMultiplier", 0.2);
        config.playerBulletSpeed = (float)ae::dvalue(jConfig, "playerBulletSpeed", 40.0);
        config.playerBaseHealth = (float)ae::dvalue(jConfig, "playerBaseHealth", 1.0);
        config.blinkResetTime = (float)ae::dvalue(jConfig, "blinkResetTime", 1.0);
        config.reviveImmunityTime = (float)ae::dvalue(jConfig, "reviveImmunityTime", 5.0);
        config.initialLives = (int)ae::dvalue(jConfig, "initialLives", 3);
        config.turretPrice = (i32)ae::dvalue(jConfig, "turretPrice", 100);
        config.maxTurrets = (u32)ae::dvalue(jConfig, "maxTurrets", 20);
        config.turretPlaceCooldown = (float)ae::dvalue(jConfig, "turretPlaceCooldown", 1.0);
        config.turretRange = (float)ae::dvalue(jConfig, "turretRange", 100.0);
        config.timePerAsteroidSpawn = (float)ae::dvalue(jConfig, "timePerAsteroidSpawn", 2.0);
        config.timeToRemovePerAsteroidSpawn = (float)ae::dvalue(jConfig, "timeToRemovePerAsteroidSpawn", 0.01);
        config.scorePerAsteroid = (u32)ae::dvalue(jConfig, "scorePerAsteroid", 10);
        config.initialAsteroidStage = (u32)ae::dvalue(jConfig, "initialAsteroidStage", 4);
        config.asteroidScalar = (float)ae::dvalue(jConfig, "asteroidScalar", 8.0);
        config.asteroidDestroySpeedMultiplier = (float)ae::dvalue(jConfig, "asteroidDestroySpeedMultiplier", 2.0);
        config.defaultHostPort = (int)ae::dvalue(jConfig, "defaultHostPort", 9999);
        config.inputUPS = (float)ae::dvalue(jConfig, "inputUPS", 30.0);
        config.stateUPS = (float)ae::dvalue(jConfig, "stateUPS", 20.0);
        config.maxAsteroids = (u32)ae::dvalue(jConfig, "maxAsteroids", 2000);
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

    deltaNetworkStatsTicker.setRate(ae::getConfigValue<double>("tps"));

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
                outline.setPosition(transform.getPos() - sf::Vector2f(config.turretRange, config.turretRange));
                outline.setSize(sf::Vector2f(config.turretRange, config.turretRange) * 2.0f);
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
                    sf::Vertex vertex;
                    vertex.color = color.getColor();
                    vertex.texCoords = sf::Vector2f();
                    vertex.position = vertices[0];

                    array.append(vertex);
                    vertex.position = vertices[i + 0];
                    array.append(vertex);
                    vertex.position = vertices[i + 1];
                    array.append(vertex);
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
            u32 networkCount = world.count<ae::NetworkedEntity>();

            text.setPosition(sf::Vector2f((float)window.getSize().x / 2.0f, 0.0f));
            text.setOutlineThickness(0.5f);
            text.setOutlineColor(sf::Color::White);
            text.setFillColor(sf::Color::Black);
            text.setStyle(sf::Text::Bold);
            text.setString(ae::formatString("Entity count: %u", (unsigned int)networkCount));
            window.draw(text);
            text.setStyle(sf::Text::Regular);
        }
     });

    ae::mainLoop();

	return 0;
}