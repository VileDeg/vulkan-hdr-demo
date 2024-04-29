import subprocess
import shutil
import os

EXE_NAME = "vkdemo.exe"

ret = subprocess.run("cmake -S . -B .")
if ret.returncode != 0:
    print("CMake failed:")
    print(ret.stderr)
    exit(1)

msbuild_path = shutil.which("MsBuild.exe")

if msbuild_path is None:
    print("MsBuild.exe not found in PATH")
    msbuild_path = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"

    if not os.path.exists(msbuild_path):
        print(f"MsBuild.exe not found at location {msbuild_path}")
        exit(1)

ret = subprocess.run(msbuild_path + " vulkan-hdr-demo.sln /t:Build /p:Configuration=Release")
if ret.returncode != 0:
    # print the error message that was returned
    print("MSBuild failed:")
    print(ret.stderr)
    exit(1)

ret = subprocess.run([EXE_NAME])
if ret.returncode != 0:
    # print the error message that was returned
    print(f"{EXE_NAME} failed:")
    print(ret.stderr)
    exit(1)
