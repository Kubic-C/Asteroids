#include "game.hpp"
#include "component.hpp"
#include "statesDefine.hpp"

/**
 * Current Bugs:
 *  - When in the GameOverState and all players have pressed 'R',
 *  it will transition to PlayState then back to GameOverState the again back
 * to PlayState where it will stop.
 * 
 *  - When all players have died and its transitioned to a GameOverState,
 * when all players have pressed 'R' and then it transitions to a PlayState,
 * some players may still be dead for the first frame meaning that the reviveSystem kicks
 * in and they lose a life.
 */

int main(int argc, char* argv[]) {
    global = std::make_shared<Global>();
    if(!global->loadResources()) {
        ae::log(ae::ERROR_SEVERITY_FATAL, "Failed to load resources\n");
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

	/* ALL NETWORKED COMPONENTS MUST BE DECLARED HERE! */
	ae::EntityWorldNetworkManager& networkManager = ae::getEntityWorldNetworkManager();
	networkManager.registerComponent<HealthComponent>();
	networkManager.registerComponent<ColorComponent>();
	networkManager.registerComponent<PlayerColorComponent>();
	networkManager.registerComponent<PlayerComponent>();
	networkManager.registerComponent<AsteroidComponent>();
	networkManager.registerComponent<SharedLivesComponent>();
	networkManager.registerComponent<BulletComponent>();
	networkManager.registerComponent<MapSizeComponent>();
	networkManager.registerComponent<TurretComponent>();
	networkManager.registerComponent<ScoreComponent>();

	ae::setUpdateCallback([](){
        using namespace ae;
        flecs::world& world = getEntityWorld();
        PhysicsWorld& physicsWorld = getPhysicsWorld();
        sf::RenderWindow& window = getWindow();

        sf::Color outlineColor = sf::Color(54, 69, 79);
        world.each([&](flecs::entity e, ShapeComponent& shape, ColorComponent& color) {
            if (!physicsWorld.doesShapeExist(shape.shape))
                return;

            Shape& physicsShape = physicsWorld.getShape(shape.shape);

            switch (physicsShape.getType()) {
            case ShapeEnum::Polygon: {
                Polygon& polygon = dynamic_cast<Polygon&>(physicsShape);

                sf::ConvexShape sfShape(polygon.getVerticeCount());
                sfShape.setFillColor(color.getColor());
                for (u8 i = 0; i < polygon.getVerticeCount(); i++) {
                    sfShape.setPoint(i, polygon.getWorldVertices()[i]);
                }

                sfShape.setOutlineColor(outlineColor);
                sfShape.setOutlineThickness(-2.0f);

                window.draw(sfShape);
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
        });

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

            window.draw(base);
            window.draw(rectangle);
        });
	});

    ae::mainLoop();

	return 0;
}