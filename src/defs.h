#pragma once


#define ARRAY_SIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*(_ARR))))     // Size of a static C-style array. Don't use on pointers!

//Macros for printing error messages
#define HHHPR(_msg, _stream) do{ _stream << _msg << std::endl; }while(0)
#define HHPR(_msg, _type, _stream) do{ _stream << "[" << __FILE__ << " " << __func__ << " " << __LINE__ << "] " << _type << ": "; HHHPR(_msg, _stream); }while(0)
#define HPR(_msg, _type) HHPR(_msg, _type, std::cerr)
#define HPRINFO(_msg, _type) HHPR(_msg, _type, std::cout)

#define PRERR(_msg) HPR(_msg, "Error");
#define PRWRN(_msg) HPR(_msg, "Warning");
#define PRABRT(_msg) HPR(_msg, "Abort");

#define PRINF(_msg) HPRINFO(_msg, "Info");

#define pr(_msg) HHHPR(_msg, std::cout)

//Exit macro
#define EXIT(_x) do{ PRERR("program exit with code: " << _x); exit(_x); }while(0)

//Macro for breaking into the debugger or aborting the program
#ifdef NDEBUG
    #define TRAP() do{ PRABRT(""); abort(); }while(0)
#else
    #ifdef _WIN32
        #ifdef _MSC_VER
            #define TRAP() __debugbreak()
        #else
            #include <intrin.h>
            #define TRAP() __debugbreak()
        #endif // _MSC_VER
    #else
        #include <csignal>
        #define TRAP() std::raise(SIGTRAP)
    #endif // _WIN32
#endif // NDEBUG

//Assert macros for boolean expressions
#define HASSERT(_x, _tru) do{ if ((_x) != _tru) { TRAP(); } }while(0)
#define HASSERTMSG(_x, _tru, _msg) do{ if ((_x) != _tru) { PRERR(_msg); TRAP(); } }while(0)

//Assert macros for functions returning bool
#define ASSERT(_x) HASSERT(_x, true)
#define ASSERT_MSG(_x, _msg) HASSERTMSG(_x, true, _msg)

//Assert macros for functions returning VkResult
#define VK_ASSERT(_x) HASSERT(_x, VK_SUCCESS)
#define VK_ASSERTMSG(_x, _msg) HASSERTMSG(_x, VK_SUCCESS, _msg)

//Assert macros for functions returning GLFWbool
#define GLFWASSERTMSG(_x, _msg) HASSERTMSG(_x, GLFW_TRUE, _msg)

//Macro for dynamic load of extension functions
#define DYNAMIC_LOAD(_varname, _instance)\
        _varname = (PFN_##_varname)vkGetInstanceProcAddr(_instance, #_varname);\
        if (_varname == nullptr) { throw std::runtime_error("Failed to load function " #_varname); }

//Print macros for glm types
#define V4PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << ", " << _v.w << " "
#define V3PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << " "
#define V2PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << " "


#define MAX_FRAMES_IN_FLIGHT 3

#define ASSET_PATH std::string("assets/")

#define IMAGE_PATH  ASSET_PATH + std::string("images/")
#define MODEL_PATH  ASSET_PATH + std::string("models/")
#define SCENE_PATH  ASSET_PATH + std::string("scenes/")

#define SHADER_PATH ASSET_PATH + std::string("shaders/bin/")
#define SKYBOX_PATH IMAGE_PATH + std::string("skybox/")
