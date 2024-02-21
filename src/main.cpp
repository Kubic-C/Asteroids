#include "game.hpp"

/**
 * Current Bugs:
 *   - None
 */

int main(int argc, char* argv[]) {
    ae::MessageBuffer test;

    global = std::make_shared<Global>();
    if(!global->loadResources()) {
        ae::log(ae::ERROR_SEVERITY_FATAL, "Failed to load resources\n");
    }

	/* ALL NETWORKED COMPONENTS MUST BE DECLARED HERE! */
	ae::NetworkStateManager& networkManager = ae::getNetworkStateManager();
    networkManager.registerComponent<HealthComponent>();
    networkManager.registerComponent<ColorComponent>(ae::ComponentPiority::High);
    networkManager.registerComponent<PlayerColorComponent>(ae::ComponentPiority::High);
    networkManager.registerComponent<PlayerComponent>();
    networkManager.registerComponent<AsteroidComponent>();
    networkManager.registerComponent<SharedLivesComponent>(ae::ComponentPiority::High);
    networkManager.registerComponent<BulletComponent>();
    networkManager.registerComponent<MapSizeComponent>(ae::ComponentPiority::High);
    networkManager.registerComponent<TurretComponent>();
    networkManager.registerComponent<ScoreComponent>(ae::ComponentPiority::High);

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

    flecs::world& ecs = ae::getEntityWorld();

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

    ae::Ticker<void(float)> networkStatsLog;
    networkStatsLog.setFunction([&](float) {
        if(ticks == 0)
            ticks = 1;

        std::string lastNetworkReport = 
            ae::formatString("<cyan, bold>NETWORK<reset> (AVG by TICK) <green>WB: %llu<reset> <red>RB: %llu<reset>\n", totalWrite / ticks, totalRead / ticks);

        ae::log(lastNetworkReport);
        totalWrite = 0;
        totalRead = 0;
        ticks = 0;
    });

    networkStatsLog.setRate(1.0f);

    float debugLogCooldown = 1.0f;

     ae::setUpdateCallback([&](){
        using namespace ae;
        flecs::world& world = getEntityWorld();
        PhysicsWorld& physicsWorld = getPhysicsWorld();
        sf::RenderWindow& window = getWindow();

        deltaNetworkStatsTicker.update();
        networkStatsLog.update();

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

        debugLogCooldown -= 0.001f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F4) && debugLogCooldown < 0.0f) {
            ae::log(getNetworkStateManager().getNetworkedEntityInfo());
            debugLogCooldown = 1.0f;
        }
     });

    ae::mainLoop();

	return 0;
}