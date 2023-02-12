
#include "Enigne.h"
#include <iostream>

int main()
{
    try {
        Engine engine;
        engine.Init();
        engine.Run();
	}
	catch (vk::Error& e) {
		std::cout << "Failed because of Vulkan exception: " << e.what() << std::endl;
	}
	catch (std::exception& e) {
		std::cout << "Failed because of exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "Failed because of unspecified exception." << std::endl;
	}
    return 0;
}
