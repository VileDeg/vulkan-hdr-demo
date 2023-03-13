set compiler=C:\VulkanSDK\1.3.236.0\Bin\glslc.exe
set shaderpath=assets/shaders
set shadername=shader

for %f in (.\*.(vert|frag)) do @echo %f

%compiler% %shaderpath%/%shadername%.vert -o %shaderpath%/vert.spv
%compiler% %shaderpath%/%shadername%.frag -o %shaderpath%/frag.spv
pause