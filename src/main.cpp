#include "stdafx.h"

#include "Engine.h"

int main()
{
    Engine engine;

    try {
        engine.Init();
    } catch (const std::runtime_error& e) {
        PRERR(e.what());
        return EXIT_FAILURE;
    }

    engine.Run();
	engine.Cleanup();

    return 0;
}
