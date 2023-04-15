#include "stdafx.h"

#include "Engine.h"

int main()
{
    Engine engine;
    engine.Init();
    engine.Run();
	engine.Cleanup();

    return 0;
}
