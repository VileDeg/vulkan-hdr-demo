import os
import sys

cmake_source_dir = sys.argv[1]
os.chdir(cmake_source_dir)

#glsl = "glslangValidator"
glsl = "glslc"

base_dir = os.path.join("assets", "shaders")

src_dir = os.path.join(base_dir, "src")
bin_dir = os.path.join(base_dir, "bin")

src_ext = (".frag", ".vert", ".geom", ".tesc", ".tese", ".comp")
bin_ext = (".spv")

debug_flags = "-gVS"

for f in os.listdir(src_dir):
	if f.endswith(src_ext):
		src_path = os.path.join(src_dir, f)
		dst_path = os.path.join(bin_dir, f)
		#os.system(f"{glsl} -e main {debug_flags} -V {src_path} -o {dst_path}.spv")
		os.system(f"{glsl} -g {src_path} -o {dst_path}.spv")
	else:
		continue
