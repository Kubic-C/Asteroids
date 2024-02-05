
add_executable(asteroids "main.cpp" "base.hpp" "game.hpp" "game.cpp" "physics.hpp" "physicsEcs.hpp" "entity.hpp" "network.hpp" "state.hpp" "statesDefine.hpp" "statesDefine.cpp")

target_link_libraries(asteroids PUBLIC 
	bitsery 
	GameNetworkingSockets::static
	sfml-system
	sfml-graphics
    sfml-audio
    flecs_static
	THST
	tgui)

add_custom_command(TARGET asteroids POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:asteroids> $<TARGET_FILE_DIR:asteroids>
  COMMAND_EXPAND_LISTS
  )