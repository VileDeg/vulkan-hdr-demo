import os
import sys

cmake_source_dir = sys.argv[1]
os.chdir(cmake_source_dir)

glsl = "glslc"

shader_dir     = os.path.join("assets", "shaders")
shader_bin_dir = os.path.join(shader_dir, "bin")

src_ext = (".frag", ".vert", ".geom", ".tesc", ".tese", ".comp")
bin_ext = (".spv")

for f in os.listdir(shader_dir):
	if f.endswith(src_ext):
		path = os.path.join(shader_dir, f)
		os.system(glsl + " " + path + " -o " + path + ".spv")
	else:
		continue

for f in os.listdir(shader_dir):
	if f.endswith(bin_ext):
		os.replace(os.path.join(shader_dir, f), os.path.join(shader_bin_dir, f))
	else:
		continue