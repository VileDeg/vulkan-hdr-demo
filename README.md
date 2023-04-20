# vulkan-hdr-demo

## About
This repository contains a simple renderer, that uses Vulkan API and is written in C++.

For cross-platform compilation it uses CMake build system. Platforms currently supported: `Windows, Linux`.

Current functionality:
* Camera movement `W, A, S, D, Shift(down), Space(up)`
* Model loading: `.obj`, `without textures`.
* Texture loading: `RGBA`.
* Phong lighting model: `several point lights with attenuation`.
* Tone mapping algorithms: `Reinhart`.

## Compilation

### Configure the project with CMake:
```
cmake -S . -B build
```

### Compile:

On Linux:
```
cd build
make
```
On Windows:

Open generated Visual Studio solution and build the project or open `x64 Native Tools Command Prompt for VS 20XX` and run:
```
msbuild vulkan-hdr-demo.sln
```
then to run:
```
build\Debug\vkdemo.exe
```
But be aware that the solution or executable name may change in future.
## Usage
The compiled executable is `vkdemo` or `vkdemo.exe`.

You can run it from command line, or on Windows also from Visual Studio.

## Libraries/Resources Used
### Libraries
* [Vulkan SDK](https://vulkan.lunarg.com/)
* [GLFW](https://www.glfw.org/)
* [GLM](https://glm.g-truc.net/0.9.9/index.html)
* [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
* [tiny_obj_loader](https://github.com/tinyobjloader/tinyobjloader)
* [tinygltf](https://github.com/syoyo/tinygltf)
* [Vulkan Memory Allocator by AMD](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)

### Web pages
As this is mainly a personal learning project, a lot of resources where used to learn Vulkan API.

Here are the main ones:
* [vulkan-guide by vblanco20-1](https://vkguide.dev/)
* [Vulkan Tutorial in Czech language by Jan Pečiva](https://www.root.cz/serialy/tutorial-vulkan/)
* [Vulkan Tutorial by Alexander Overvoorde](https://vulkan-tutorial.com/)
* [LearnOpenGL by Joey de Vries](https://learnopengl.com/)

Following are the repositories from which some code was taken as-is or with modifications. Authors are also attributed in the source code:
* [vulkan-guide repository by vblanco20-1](https://github.com/vblanco20-1/vulkan-guide)
* [VulkanTutorial repository by Jan Pečiva](https://github.com/pc-john/VulkanTutorial)

### Assets
Assets used and the information about them can be found in `assets` folder and subfolders.

Usually each model's folder contains some `(LICENSE|copyright).txt` file.
