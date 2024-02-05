#include "game.hpp"

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