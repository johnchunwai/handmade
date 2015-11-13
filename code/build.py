import subprocess
import argparse
import os
import shutil


def execute(command, cwd):    
    popen = subprocess.Popen(command, stdout=subprocess.PIPE, cwd=cwd)
    lines_iterator = iter(popen.stdout.readline, b"")
    for line in lines_iterator:
        print(line.decode("utf-8")) # yield line


def bool_2_on_off(bool_val):
    return "on" if bool_val else "off"
        
parser = argparse.ArgumentParser()
parser.add_argument("-p", "--platform",
                    choices=["win32", "linux"], required=True)
parser.add_argument("-b", "--build",
                    choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"],
                    default="Debug")
parser.add_argument("-c", "--clean", action="store_true")
parser.add_argument("-s", "--sdl", action="store_true")
parser.add_argument("-d", "--diagnostic", action="store_true")
parser.add_argument("-i", "--internal_build", action="store_true")
args = parser.parse_args()

build_dir = os.path.join("..", "..", "build")
handmade_build_dir = os.path.join(build_dir, "handmade")

if args.clean:
    shutil.rmtree(handmade_build_dir)

for dir in [build_dir, handmade_build_dir]:
    try:
        os.makedirs(dir)
    except OSError:
        if not os.path.isdir(dir):
            raise


cmake_gen = ["cmake", os.path.join("..", "..", "handmade")]
cmake_build = ["cmake", "--build", "."]

if args.platform == "win32":
    cmake_gen.extend(['-G', 'Visual Studio 14 2015 Win64'])
    cmake_build.extend(["--config", args.build])
elif args.platform == "linux":
    # alternative is GNU/AppleClang/MSVC
    cmake_gen.extend(
        ["-DCMAKE_BUILD_TYPE={}".format(args.build)])#,
#         "-DCMAKE_CXX_COMPILER_ID=Clang"])

cmake_gen.append("-Duse_sdl={}".format(bool_2_on_off(args.sdl)))
cmake_gen.append("-Ddiagnostic={}".format(bool_2_on_off(args.diagnostic)))
cmake_gen.append("-Dinternal_build={}".format(bool_2_on_off(args.internal_build)))

print(cmake_gen)
print(cmake_build)
execute(cmake_gen, cwd=handmade_build_dir)
execute(cmake_build, cwd=handmade_build_dir)
