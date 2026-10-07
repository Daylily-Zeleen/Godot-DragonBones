# -*- coding: utf-8 -*-
#!/usr/bin/env python

# /**************************************************************************/
# /*  SConstruct                                                            */
# /**************************************************************************/
# /*                         This file is part of:                          */
# /*                           Godot-DragonBones                            */
# /*        https://github.com/Daylily-Zeleen/Godot-DragonBones             */
# /**************************************************************************/
# /* Copyright (c) 2024-present 忘忧の (Daylily-Zeleen)                      */
# /*               - Contact: daylily-zeleen@foxmail.com                    */
# /*                                                                        */
# /* Permission is hereby granted, free of charge, to any person obtaining  */
# /* a copy of this software and associated documentation files (the        */
# /* "Software"), to deal in the Software without restriction, including    */
# /* without limitation the rights to use, copy, modify, merge, publish,    */
# /* distribute, sublicense, and/or sell copies of the Software, and to     */
# /* permit persons to whom the Software is furnished to do so, subject to  */
# /* the following conditions:                                              */
# /*                                                                        */
# /* The above copyright notice and this permission notice shall be         */
# /* included in all copies or substantial portions of the Software.        */
# /*                                                                        */
# /* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
# /* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
# /* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
# /* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
# /* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
# /* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
# /* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
# /**************************************************************************/

import os
import subprocess
import sys

import os
os.system("chcp 65001")

from SCons.Script import ARGUMENTS

# godot-cpp 10.x ships one API JSON per Godot version (gdextension/
# extension_api-4-3.json ... 4-7.json), so the target must be stated explicitly.
# "4.3" is the lowest godot-cpp 10.x can target, and it is also the runtime floor:
# the version is baked into the generated version.hpp and GDExtensionBinding::init()
# refuses to load in older Godot.
API_VERSION = ARGUMENTS.get("api_version", "4.3")

# Resolve the API JSON actually used for this build so the runtime floor declared in
# the .gdextension can never drift from the bindings it was compiled against.
# `custom_api_file` takes precedence over `api_version`, matching godot-cpp's own
# precedence (tools/godotcpp.py:572-575).
API_JSON = ARGUMENTS.get("custom_api_file") or os.path.join(
    "thirdparty", "godot-cpp", "gdextension",
    f"extension_api-{API_VERSION.replace('.', '-')}.json")

# Apply the trimmed binding set automatically; an explicit CLI argument still wins.
ARGUMENTS.setdefault("build_profile", Dir("#").File("build_profile.json").abspath)

# C++ 单元测试（doctest）：默认关闭；开启后编译 tests/ 并定义 GDDB_TESTS_ENABLED。
# 该宏由 tests/test_runner.h 与 tests/test_main.cpp 消费；src/dragon_bones_registration.cpp
# 据此调用测试入口（--gddb-run-tests）。
#
# NOTE: `tests` 由本项目的 SConstruct 自行解释并显示在 `scons -h` 中（见下方 Help）。
# godot-cpp 的 SConstruct 会用它自建的 `Variables`（只收集它自己 opts.Add 的项）调用
# `opts.UnknownVariables()`，因此 `scons ... tests=yes` 仍会额外打印一行
# "Unknown SCons variables ... tests=yes"。该警告是误导性的：选项**确实生效**。
# 这与参考项目 GodotJS-Ext 的行为一致（见其 SConstruct 的 custom options 与
# godot-cpp 的同名警告），无需为消除它重构出「自建 Environment + opts」。
_GDDB_TESTS_ENABLED = str(ARGUMENTS.get("tests", "no")).lower() in ("1", "yes", "true", "on")

Help("""
tests: Build and run the C++ unit tests (doctest) (yes|no)
    default: no
    actual: {}
""".format("yes" if _GDDB_TESTS_ENABLED else "no"))

env = SConscript("thirdparty/godot-cpp/SConstruct", {"api_version": API_VERSION})
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


add_sources_recursively("src/", sources, ["editor"])
# godot-cpp is built and linked separately by its own SConstruct (env.GodotCPP()),
# so it must not be globbed in here again (duplicate symbols at link time).
add_sources_recursively("thirdparty/", sources, ["godot-cpp"])

# C++ 单元测试（doctest）：仅在 tests=yes 时编译 tests/ 并定义宏。
# tests/ 不参与常规 glob，故默认构建完全不受影响。
if _GDDB_TESTS_ENABLED:
    env.Append(CPPDEFINES=["GDDB_TESTS_ENABLED"])
    env.Append(CPPPATH=["tests/"])
    sources += Glob("tests/*.cpp")

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

# 确保调试构建包含编辑器内容
if env.debug_features:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    sources += Glob("src/editor/*.cpp")


if env.editor_build:
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

# 后处理实现由 SCons 与 CMake 共用（misc/post_build.py），避免同一套逻辑维护两份。
POST_BUILD_SCRIPT = os.path.join(Dir("#").abspath, "misc", "post_build.py")


def _post_build_mode():
    # ios 的静态库不在插件目录，只需就地去掉 ".dev."；其余平台拷进插件目录。
    return "move" if platform == "ios" else "copy"


def _post_build_paths():
    if platform == "macos":
        name = f"{lib_name}.{platform}.{compile_target}.framework/{lib_name}.{platform}.{compile_target}"
        return f"{output_bin_folder}/{name}", f"{plugin_bin_folder}/{name}"
    if platform == "ios":
        sim = ".simulator" if ios_simulator else ""
        name = f"{lib_name}.{platform}.{compile_target}{sim}.a"
        return f"{output_bin_folder}/{name}", f"{output_bin_folder}/{name}"
    name = f"{lib_name}{suffix}{share_lib_suffix}"
    return f"{output_bin_folder}/{name}", f"{plugin_bin_folder}/{name}"


def on_complete(target, source, env):
    src_lib, dest_lib = _post_build_paths()
    cmd = [
        sys.executable, POST_BUILD_SCRIPT,
        "--src-lib", src_lib,
        "--dest-lib", dest_lib,
        "--mode", _post_build_mode(),
        "--plugin-folder", plugin_folder,
        "--repo-root", Dir("#").abspath,
        "--extension-file", extension_file,
        "--version-file", os.path.join(Dir("#").abspath, "version"),
        "--api-json", API_JSON,
        "--api-version", API_VERSION,
    ]
    subprocess.run(cmd, check=True)


# Disable scons cache for source files
NoCache(sources)

complete_command = Command("complete", library, on_complete)
Depends(complete_command, library)
Default(complete_command)
