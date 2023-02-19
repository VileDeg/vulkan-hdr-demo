#pragma once

//Macros for printing error messages
//#define HPR(_msg, _type) do{ fprintf(stderr, "[%s, %s, %d] %s: %s\n", __FILE__, __func__, __LINE__, _type, _msg); }while(0)
#define HPR(_msg, _type) do{ std::cerr << "[" << __FILE__ << " " << __func__ << " " << __LINE__ << "] " << _type << ": " << _msg << std::endl; }while(0)
#define PRERR(_msg) HPR(_msg, "Error");
#define PRWRN(_msg) HPR(_msg, "Warning");
#define PRABRT(_msg) HPR(_msg, "Abort");

//Macro for breaking into the debugger or aborting the program
#ifdef NDEBUG
#define BRK do{ fprintf(stderr, "Abort called in file %s, function %s, on line %d\n", __FILE__, __func__, __LINE__) abort(); }while(0)
#else
#define BRK __debugbreak()
#endif // !NDEBUG

//Assert macros for boolean expressions
#define HASSERT(_x, _tru) do{ if ((_x) != _tru) { BRK; } }while(0)
#define HASSERTMSG(_x, _tru, _msg) do{ if ((_x) != _tru) { PRERR(_msg); BRK; } }while(0)

//Assert macros for functions returning bool
#define ASSERT(_x) HASSERT(_x, true)
#define ASSERTMSG(_x, _msg) HASSERTMSG(_x, true, _msg)

//Assert macros for functions returning VkResult
#define VKASSERT(_x) HASSERT(_x, VK_SUCCESS)
#define VKASSERTMSG(_x, _msg) HASSERTMSG(_x, VK_SUCCESS, _msg)

//Assert macros for functions returning GLFWbool
#define GLFWASSERTMSG(_x, _msg) HASSERTMSG(_x, GLFW_TRUE, _msg)

//Handy macro for reducing code in aggregate initialization of CreateInfoXXX structs
#define HCCP(_type) &(const _type&)_type

//Macro for dynamic load of extension functions
#define DYNAMIC_LOAD(_varname, _instance, _func)\
        PFN_##_func _varname = (PFN_##_func)vkGetInstanceProcAddr(_instance, #_func);\
        if (_varname == nullptr) { throw std::runtime_error("Failed to load function " #_func); }

#define V4PR(_v) #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << ", " << _v.w << " "
#define V3PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << ", " << _v.z << " "
#define V2PR(_v) " " << #_v << ": " << _v.x << ", " << _v.y << " "
