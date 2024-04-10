#!/usr/bin/env python
import os
import shutil
import version

env = SConscript("godot-cpp/SConstruct")
lib_name = "libgddragonbones"
# For the reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp") + Glob("register_types.cpp")


output_bin_folder = "./bin"
plugin_folder = "./demo/addons/gddragonbones"
plugin_bin_folder = f"{plugin_folder}/bin"

extension_file = "demo/addons/gddragonbones/gddragonbones.gdextension"

if env.debug_features:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])


def add_sources_recursively(dir: str, glob_sources):
    for f in os.listdir(dir):
        sub_dir = os.path.join(dir, f)
        if os.path.isdir(sub_dir):
            glob_sources += Glob(os.path.join(sub_dir, "*.cpp"))
            add_sources_recursively(sub_dir, glob_sources)


add_sources_recursively("src/", sources)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        f'{output_bin_folder}/{lib_name}.{env["platform"]}.{env["target"]}.framework/{lib_name}.{env["platform"]}.{env["target"]}',
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            f'{output_bin_folder}/{lib_name}.{env["platform"]}.{env["target"]}.simulator.a',
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            f'{output_bin_folder}/{lib_name}.{env["platform"]}.{env["target"]}.a',
            source=sources,
        )
else:
    library = env.SharedLibrary(
        f'{output_bin_folder}/{lib_name}{env["suffix"]}{env["SHLIBSUFFIX"]}',
        source=sources,
    )

platform = env["platform"]
compile_target = env["target"]
suffix = env["suffix"]
ios_simulator = env["ios_simulator"]
share_lib_suffix = env["SHLIBSUFFIX"]


def copy_file(from_path, to_path):
    if not os.path.exists(os.path.dirname(to_path)):
        os.makedirs(os.path.dirname(to_path))
    shutil.copyfile(from_path, to_path)


def on_complete(target, source, env):
    if platform == "macos":
        copy_file(
            f"{output_bin_folder}/{lib_name}.{platform}.{compile_target}.framework/{lib_name}.{platform}.{compile_target}",
            f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.framework/{lib_name}.{platform}.{compile_target}",
        )
    elif platform == "ios":
        if ios_simulator:
            copy_file(
                f"{output_bin_folder}/{lib_name}.{platform}.{compile_target}.simulator.a",
                f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.simulator.a",
            )
        else:
            copy_file(
                f"{output_bin_folder}/{lib_name}.{platform}.{compile_target}.a",
                f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.a",
            )
    else:
        copy_file(
            f"{output_bin_folder}/{lib_name}{suffix}{share_lib_suffix}",
            f"{plugin_bin_folder}/{lib_name}{suffix}{share_lib_suffix}",
        )

    copy_file("README.md", os.path.join(plugin_folder, "README.md"))
    copy_file("LICENSE", os.path.join(plugin_folder, "LICENSE"))

    # 更新.gdextension中的版本信息
    f = open(extension_file, "r", encoding="utf8")
    lines = f.readlines()
    f.close()

    for i in range(len(lines)):
        if lines[i].startswith('version = "') and lines[i].endswith('"\n'):
            lines[i] = f'version = "{version.version}"\n'
            break

    f = open(extension_file, "w", encoding="utf8")
    f.writelines(lines)
    f.close()


# Disable scons cache for source files
NoCache(sources)

complete_command = Command("complete", library, on_complete)
Depends(complete_command, library)
Default(complete_command)
