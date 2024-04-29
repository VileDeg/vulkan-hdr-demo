import os
import sys
import shutil

if len(sys.argv) != 2:
	print("Usage: python compile_shaders.py <path to cmake source dir>")
	sys.exit(1)

cmake_source_dir = sys.argv[1]
os.chdir(cmake_source_dir)

glslc = shutil.which("glslc")

if glslc is None:
	if sys.platform == "win32":
		glslc = os.path.join("external", "shaderc", "glslc")
	elif sys.platform == "linux":
		print("Error: glslc not found")
		sys.exit(1)

# Check if exists
if not os.path.exists(glslc):
	print("Error: glslc not found")
	sys.exit(1)

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
		if not os.path.exists(bin_dir):
			os.makedirs(bin_dir)
		command = f"{glslc} -g {src_path} -o {dst_path}.spv"
		print(command)
		os.system(command)
	else:
		continue
