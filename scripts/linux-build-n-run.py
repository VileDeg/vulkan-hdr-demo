import subprocess
import os
import shutil
import argparse

EXE_NAME = "vkdemo"
LINUX_BUILD_DIR = "linux-build"

CMAKE_CACHE_FILE = "CMakeCache.txt"

BUILD_CONFIGURATION = "Release"

def handleRemoveReadonly(func, path, exc_info):
    """
    From: https://stackoverflow.com/questions/1213706/what-user-do-python-scripts-run-as-in-windows
    Error handler for ``shutil.rmtree``.

    If the error is due to an access error (read only file)
    it attempts to add write permission and then retries.

    If the error is for another reason it re-raises the error.
    
    Usage : ``shutil.rmtree(path, onerror=onerror)``
    """
    import stat
    # Is the error an access error?
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        func(path)
    else:
        raise


if __name__ == "__main__":

    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Build and run the program on Linux.')
    
    parser.add_argument('-wsl', '--linux-is-wsl', action='store_true', default=False, help="Whether to use WSL to build and run the program.")
    
    args = parser.parse_args()

    # Delete cmake cache
    if os.path.exists(CMAKE_CACHE_FILE):
        os.remove(CMAKE_CACHE_FILE)

    # Delete build directory
    if os.path.exists(LINUX_BUILD_DIR):
        shutil.rmtree(LINUX_BUILD_DIR, ignore_errors=False, onerror=handleRemoveReadonly)

    cmd = f"cmake -S . -B {LINUX_BUILD_DIR} -DCMAKE_BUILD_TYPE={BUILD_CONFIGURATION}"

    if args.linux_is_wsl:
        cmd = f"wsl {cmd}"

    ret = subprocess.run(cmd)
    if ret.returncode != 0:
        print("CMake failed:")
        print(ret.stderr)
        exit(1)

    os.chdir(LINUX_BUILD_DIR)

    cmd = "make"

    if args.linux_is_wsl:
        cmd = f"wsl {cmd}"

    ret = subprocess.run("wsl make")
    if ret.returncode != 0:
        # print the error message that was returned
        print("Make failed:")
        print(ret.stderr)
        exit(1)

    if not os.path.exists(EXE_NAME):
        print(f"{EXE_NAME} not found in {LINUX_BUILD_DIR}")
        exit(1)

    cmd = f"./{EXE_NAME} .."

    if args.linux_is_wsl:
        cmd = f"wsl {cmd}"

    # Run in background
    ret = subprocess.run(cmd)
    if ret.returncode != 0:
        # print the error message that was returned
        print(f"{EXE_NAME} failed:")
        print(ret.stderr)
        exit(1)
