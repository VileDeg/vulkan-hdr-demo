import os

glsl = "glslc"

shader_dir = os.path.join("assets", "shaders")
shader_bin_dir = os.path.join(shader_dir, "bin")

src_ext = (".frag", ".vert", ".geom", ".tesc", ".tese", ".comp")
bin_ext = (".spv")

for f in os.listdir(shader_dir):
	if f.endswith(src_ext):
		path = os.path.join(shader_dir, f)
		print(path)
		os.system(glsl + " " + path + " -o " + path + ".spv")
	else:
		continue

for f in os.listdir(shader_dir):
	if f.endswith(bin_ext):
		path = os.path.join(shader_dir, f)
		cmd = "move " + path + " " + shader_bin_dir
		print(cmd)
		os.system(cmd)
	else:
		continue