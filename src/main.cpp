#include "states.hpp"

/** TODO
 * Add game state:
 * 
 * QT:
 * In order for the game to progress, from joining to starting to playing then ending we need game state.
 * This allows a defined set of behaviour when the game is said to be is start or play. For example, in a start state
 * the game will wait for R to be pressed by both players, after which it will transistion the state to play.
 * 
 * State
 *	- Enum
 *  - Attached systems. (Active only when the state is active)
 *  - Cleanup.
 * 
 * Heres my aprroach:
 * Attach a set of systems to some state via members or the use of flecs modules. When the state
 * becomes active, so will those systems. Those systems are responsible for the behaviour of the state.
 */

int main() {
	try {
		game = new global_t();

		game->Init();

		while (!game->ShouldAppClose()) {
			game->Update();
		}

		delete game;
	} catch(std::runtime_error& err) {
		std::cout << err.what();
	}

	return 0;
}