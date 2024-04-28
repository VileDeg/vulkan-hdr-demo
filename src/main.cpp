#include "stdafx.h"
#include "defs.h"

#include "engine.h"

int main(int argc, char* argv[])
{
    // If work dir was supplied as command line argument set it
    if (argc > 1) {
        // Get the absolute path of the supplied working directory (get rid of all '../' './' etc.)
        std::filesystem::path work_dir = std::filesystem::canonical(argv[1]);
        std::filesystem::current_path(work_dir); 
        pr("Setting working directory to: " << work_dir << "\n\n");
    }


    ASSERT_MSG(false, "CRASH_TEST");

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
