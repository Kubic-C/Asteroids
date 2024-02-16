#include "game.hpp"
#include "component.hpp"
#include "statesDefine.hpp"

/**
 * Current Bugs:
 *  - If entities are deleted in between world snapshots (20 times a second) they could get sync
 * up wrong client side!
 */

int main(int argc, char* argv[]) {
    ae::MessageBuffer test;

    global = std::make_shared<Global>();
    if(!global->loadResources()) {
        ae::log(ae::ERROR_SEVERITY_FATAL, "Failed to load resources\n");
    }

	/* ALL NETWORKED COMPONENTS MUST BE DECLARED HERE! */
	ae::EntityWorldNetworkManager& networkManager = ae::getEntityWorldNetworkManager();
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

    ecs.defer_begin();

    flecs::entity e = ecs.entity();
    u32 id = e.id() & ECS_ENTITY_MASK;
    e.destruct();
    e = ecs.entity();
    u32 newId = e.id() & ECS_ENTITY_MASK;
    if(id == newId) { // generations can change within a single frame??
        ae::log("Generations can change within a single frame\n");
    }

    ae::log("OLD vs NEW: %u : %u\n", id, newId);

    ecs.defer_end();

    sf::Font font;
    if(!font.loadFromFile("./res/times.ttf"))
        ae::log(ae::ERROR_SEVERITY_FATAL, "Times new roman failed\n");

    sf::Text text(font);

    sf::VertexArray array(sf::PrimitiveType::Triangles);
	
     ae::setUpdateCallback([&](){
        using namespace ae;
        flecs::world& world = getEntityWorld();
        PhysicsWorld& physicsWorld = getPhysicsWorld();
        sf::RenderWindow& window = getWindow();

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
            if (!physicsWorld.doesShapeExist(shape.shape))
                return;

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
            u32 networkCount = world.count<ae::NetworkedEntity>();

            text.setPosition(sf::Vector2f(window.getSize().x / 2, 0.0f));
            text.setOutlineThickness(5.0f);
            text.setOutlineColor(sf::Color::White);
            text.setFillColor(sf::Color::Black);
            text.setString(ae::formatString("Entity count: %u", networkCount));
            window.draw(text);
        }

     });

    ae::mainLoop();

	return 0;
}