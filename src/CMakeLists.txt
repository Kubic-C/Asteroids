
add_executable(asteroids "main.cpp" "base.hpp" "game.hpp" "game.cpp" "physics.hpp" "physicsEcs.hpp" "entity.hpp" "network.hpp")

target_link_libraries(asteroids PUBLIC 
	bitsery 
	GameNetworkingSockets::static
	sfml-system
	sfml-graphics
    sfml-audio
    flecs_static
	THST)