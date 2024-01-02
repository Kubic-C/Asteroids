#include "states.hpp"

int main() {
	game = new global_t();

	game->Init();

	while (!game->ShouldAppClose()) {
		game->Update();
	}

	delete game;

	return 0;
}