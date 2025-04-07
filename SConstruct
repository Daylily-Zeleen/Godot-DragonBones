# -*- coding: utf-8 -*-
#!/usr/bin/env python
import os
import shutil
import build_version

import os
os.system("chcp 65001")


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
env.Append(CPPPATH=["src/", "thirdparty/"])
sources = Glob("src/*.cpp") + Glob("register_types.cpp")


output_bin_folder = "./bin"
plugin_folder = "./demo/addons/godot_dragon_bones.daylily-zeleen"
plugin_bin_folder = f"{plugin_folder}/bin"

extension_file = "demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension"

generated_doc_data_file :str = "gen/doc_data.cpp"

def add_sources_recursively(dir: str, glob_sources, exclude_folder: list = []):
    for f in os.listdir(dir):
        if f in exclude_folder:
            continue
        sub_dir = os.path.join(dir, f)
        if os.path.isdir(sub_dir):
            glob_sources += Glob(os.path.join(sub_dir, "*.cpp"))
            add_sources_recursively(sub_dir, glob_sources, exclude_folder)


if env.debug_features:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    sources += Glob("src/editor/*.cpp")


add_sources_recursively("src/", sources, ["editor"])
add_sources_recursively("thirdparty/", sources)


def _generate_doc_data() -> list[str]:
    # doc (godot-cpp 4.3 以上)
    if env["target"] in ["editor", "template_debug"]:
        try:
            if not env.GetOption('clean'):
                return env.GodotCPPDocData(generated_doc_data_file, source=env.Glob("doc_classes/*.xml"))
            else:
                return [generated_doc_data_file]
        except AttributeError:
            print("Not including class reference as we're targeting a pre-4.3 baseline.")
    return []


doc_data = _generate_doc_data()
if len(doc_data) > 0:
    sources.append(doc_data)

if env.get("is_msvc", False):
    env.Append(CXXFLAGS=["/bigobj"])


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
            f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.framework/{lib_name}.{platform}.{compile_target}".replace(
                ".dev.", "."
            ),
        )
    elif platform == "ios":
        if ios_simulator:
            copy_file(
                f"{output_bin_folder}/{lib_name}.{platform}.{compile_target}.simulator.a",
                f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.simulator.a".replace(
                    ".dev.", "."
                ),
            )
        else:
            copy_file(
                f"{output_bin_folder}/{lib_name}.{platform}.{compile_target}.a",
                f"{plugin_bin_folder}/{lib_name}.{platform}.{compile_target}.a".replace(
                    ".dev.", "."
                ),
            )
    else:
        copy_file(
            f"{output_bin_folder}/{lib_name}{suffix}{share_lib_suffix}",
            f"{plugin_bin_folder}/{lib_name}{suffix}{share_lib_suffix}".replace(
                ".dev.", "."
            ),
        )

    copied_readme_file_path = os.path.join(plugin_folder, "README.md")
    copied_readme_zh_file_path = os.path.join(plugin_folder, "README.zh.md")

    copy_file("README.md", copied_readme_file_path)
    copy_file("README.zh.md", copied_readme_zh_file_path)
    copy_file("LICENSE", os.path.join(plugin_folder, "LICENSE"))

    # 替换 readme 中图片的路径
    for fp in [copied_readme_file_path, copied_readme_zh_file_path]:
        f = open(fp, "r", encoding="utf8")
        lines = f.readlines()
        f.close()

        for i in range(len(lines)):
            if lines[i].count("(demo/addons/godot_dragon_bones.daylily-zeleen/") > 0:
                lines[i] = lines[i].replace("(demo/addons/godot_dragon_bones.daylily-zeleen/", "(")

        f = open(fp, "w", encoding="utf8")
        f.writelines(lines)
        f.close()

    # 更新.gdextension中的版本信息
    f = open(extension_file, "r", encoding="utf8")
    lines = f.readlines()
    f.close()

    for i in range(len(lines)):
        if lines[i].startswith('version = "') and lines[i].endswith('"\n'):
            lines[i] = f'version = "{build_version.version}"\n'
            break

    f = open(extension_file, "w", encoding="utf8")
    f.writelines(lines)
    f.close()


# Disable scons cache for source files
NoCache(sources)

complete_command = Command("complete", library, on_complete)
Depends(complete_command, library)
Default(complete_command)
