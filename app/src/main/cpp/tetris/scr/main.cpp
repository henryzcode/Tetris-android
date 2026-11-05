#include "headers/game.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Starting Tetris application...");

    Game game;

    try {
        if (!game.init()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize game subsystems.");
            return 1;
        }

        game.run();
    } catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unhandled exception in main: %s", e.what());
        return 1;
    } catch (...) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown exception in main.");
        return 1;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Tetris application terminated normally.");
    return 0;
}
