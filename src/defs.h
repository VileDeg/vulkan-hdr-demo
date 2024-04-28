#pragma once

#include "abort_log.h"

//////////////////////////////////////////////////////////////////////////////////////////
// Size of a static C-style array. Don't use on pointers!
#define ARRAY_SIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*(_ARR))))     

//////////////////////////////////////////////////////////////////////////////////////////

// Macros for printing error messages
#define HHHPR(_msg, _stream)       do{ _stream << _msg << std::endl; }while(0)
#define HHPR(_msg, _type, _stream) do{ \
        _stream << "[" << __FILE__ << " " << __func__ << " " << __LINE__ << "] " << _type << ": "; HHHPR(_msg, _stream); \
    }while(0)
#define HPR(_msg, _type)     HHPR(_msg, _type, std::cerr)
#define HPRINFO(_msg, _type) HHPR(_msg, _type, std::cout)

#define PRERR(_msg)  HPR(_msg, "Error");
#define PRWRN(_msg)  HPR(_msg, "Warning");
#define PREXIT(_msg) HPR(_msg, "Exit");
#define PRABRT(_msg) HPR(_msg, "Abort");

#define PRINF(_msg) HPRINFO(_msg, "Info");

// Shortcut for simple printing
#define pr(_msg) HHHPR(_msg, std::cout)

//////////////////////////////////////////////////////////////////////////////////////////

// Exit macro
#define EXIT(_x) do{ std::cerr << "Program exit with code: " << _x; exit(_x); }while(0)

#define TEXT_TRAP_MSG \
    ("ASSERTION FAILED at [" + std::string(__FILE__) + " " + std::string(__func__) + " " + std::to_string(__LINE__) + "]")

#define HTRAP(_msg) do{ \
            PREXIT(_msg << "\n" << TEXT_TRAP_MSG << "\n\tGENERATED " << STR_ABORT_LOG_FILENAME << " in working directory."); \
            std::ostringstream _buf; \
            _buf << _msg; \
            dump_to_log_file(_buf.str()); \
        } while(0)

// Macro for breaking into the debugger or exiting the program
#ifdef NDEBUG
    #define TRAP(_msg) do{ HTRAP(_msg); EXIT(1); } while(0)
#else
    #ifdef _WIN32
        #ifdef _MSC_VER
            #define TRAP(_msg) do{ HTRAP(_msg); __debugbreak(); EXIT(1); } while(0)
        #else
            #include <intrin.h>
            #define TRAP(_msg) do{ HTRAP(_msg); __debugbreak(); EXIT(1); } while(0)
        #endif // _MSC_VER
    #else
        #include <csignal>
        #define TRAP(_msg) do { HTRAP(_msg); std::raise(SIGTRAP); EXIT(1); } while (0)
    #endif // _WIN32
#endif // NDEBUG
//////////////////////////////////////////////////////////////////////////////////////////

// Assert macros for boolean expressions
#define HASSERT(_x, _tru) do{ if ((_x) != _tru) { TRAP(""); } }while(0)

#define HASSERTMSG(_x, _tru, _msg) do{ \
        if ((_x) != _tru) { TRAP(_msg); } \
    } while(0)

// Assert macros for functions returning bool
#define ASSERT(_x) HASSERT((_x), true)
#define ASSERT_MSG(_x, _msg) HASSERTMSG((_x), true, _msg)

// Assert macros for functions returning VkResult
#define VK_ASSERT(_x) HASSERT((_x), VK_SUCCESS)
#define VK_ASSERT_MSG(_x, _msg) HASSERTMSG((_x), VK_SUCCESS, _msg)

// Assert macros for functions returning GLFWbool
#define GLFW_ASSERT_MSG(_x, _msg) HASSERTMSG((_x), GLFW_TRUE, _msg)

//////////////////////////////////////////////////////////////////////////////////////////

// Macro for dynamic load of Vulkan extension functions
#define DYNAMIC_LOAD(_varname, _instance)\
        _varname = (PFN_##_varname)vkGetInstanceProcAddr(_instance, #_varname);\
        if (_varname == nullptr) { throw std::runtime_error("Failed to load function " #_varname); }

//////////////////////////////////////////////////////////////////////////////////////////
// Print macros for glm types
#define V4PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << ", " << _v.w << " "
#define V3PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << " "
#define V2PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << " "
//////////////////////////////////////////////////////////////////////////////////////////
// Number of swapchain images
#define MAX_FRAMES_IN_FLIGHT 3
//////////////////////////////////////////////////////////////////////////////////////////

#define ASSET_PATH std::string("assets/")

#define IMAGE_PATH  ASSET_PATH + std::string("images/")
#define MODEL_PATH  ASSET_PATH + std::string("models/")
#define SCENE_PATH  ASSET_PATH + std::string("scenes/")

#define SHADER_PATH ASSET_PATH + std::string("shaders/bin/")
#define SKYBOX_PATH IMAGE_PATH + std::string("skybox/")
