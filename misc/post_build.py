#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""构建后处理：SCons 与 CMake **共用同一实现**。

由 SConstruct 的 on_complete 与 CMakeLists.txt 的 POST_BUILD 分别调用，避免这份逻辑
在两处各维护一份而逐渐漂移。职责：

1. 把产物放到 demo 插件目录（dev 构建文件名里的 ".dev." 去除）
2. 同步 README.md / README.zh.md / LICENSE 到插件目录，并改写 README 中的图片路径
3. 用 version 文件刷新 .gdextension 的 version，用 API JSON 刷新 compatibility_minimum
"""

import argparse
import json
import os
import shutil
import sys

ADDON_PREFIX = "demo/addons/godot_dragon_bones.daylily-zeleen/"


def read_api_version(api_json, fallback):
    """从 extension_api.json 的 header 读出 "major.minor"，失败则返回 fallback。"""
    try:
        with open(api_json, "r", encoding="utf-8") as f:
            header = json.load(f)["header"]
    except Exception as e:
        print(f"Warning: cannot read '{api_json}' ({e}); falling back to {fallback}.")
        return fallback
    return f"{header['version_major']}.{header['version_minor']}"


def place_lib(src, dest, move):
    """把库放到 dest。".dev." 从目标路径中去除，与 godot-cpp 的 dev 后缀对应。"""
    dest = dest.replace(".dev.", ".")
    if os.path.abspath(src) == os.path.abspath(dest):
        return
    dest_dir = os.path.dirname(dest)
    if dest_dir:
        os.makedirs(dest_dir, exist_ok=True)
    if move:
        shutil.move(src, dest)
        print(f'Move library to "{dest}"')
    else:
        shutil.copyfile(src, dest)
        print(f'Copy library to "{dest}"')


def sync_addon_files(repo_root, plugin_folder):
    """同步 README / LICENSE，并改写 README 中的图片路径。"""
    for name in ("README.md", "README.zh.md", "LICENSE"):
        shutil.copyfile(os.path.join(repo_root, name), os.path.join(plugin_folder, name))

    # 插件目录里的 README 位于 addon 根，图片与其同级，故去掉 demo/addons/<addon>/ 前缀。
    for name in ("README.md", "README.zh.md"):
        path = os.path.join(plugin_folder, name)
        with open(path, "r", encoding="utf8") as f:
            lines = f.readlines()
        for i in range(len(lines)):
            if ADDON_PREFIX in lines[i]:
                lines[i] = lines[i].replace(ADDON_PREFIX, "")
        with open(path, "w", encoding="utf8") as f:
            f.writelines(lines)


def update_extension(extension_file, version_file, min_godot_version):
    """.gdextension 的 version 与 compatibility_minimum 都是生成物，勿手改。"""
    with open(version_file, "r", encoding="utf8") as f:
        version = f.readline().strip()

    with open(extension_file, "r", encoding="utf8") as f:
        lines = f.readlines()

    for i in range(len(lines)):
        if lines[i].startswith('version = "') and lines[i].endswith('"\n'):
            lines[i] = f'version = "{version}"\n'
            break

    for i in range(len(lines)):
        if lines[i].startswith("compatibility_minimum"):
            lines[i] = f"compatibility_minimum = {min_godot_version}\n"
            break

    with open(extension_file, "w", encoding="utf8") as f:
        f.writelines(lines)

    print(f'Update version number in "godot_dragon_bones.gdextension", {version}')
    print(f"Update compatibility_minimum to {min_godot_version}")


def main():
    parser = argparse.ArgumentParser(description="Godot-DragonBones post-build processing")
    parser.add_argument("--src-lib", default="", help="源库文件（macOS 为 framework 容器内的可执行文件）")
    parser.add_argument("--dest-lib", default="", help="目标路径（完整文件名）")
    parser.add_argument("--mode", choices=("copy", "move", "none"), default="copy",
                        help="copy=拷贝到插件目录（默认）；move=原地改名（iOS 静态库）；none=不搬库")
    parser.add_argument("--plugin-folder", required=True, help="demo 插件目录")
    parser.add_argument("--repo-root", required=True, help="仓库根目录")
    parser.add_argument("--extension-file", required=True, help="待刷新的 .gdextension")
    parser.add_argument("--version-file", required=True, help="version 文件（版本单一事实来源）")
    parser.add_argument("--api-json", required=True, help="本次构建实际使用的 extension_api.json")
    parser.add_argument("--api-version", required=True, help="--api-json 不可读时的回退版本")
    args = parser.parse_args()

    print("Begin post-build process.")

    if args.mode != "none":
        place_lib(args.src_lib, args.dest_lib, args.mode == "move")

    sync_addon_files(args.repo_root, args.plugin_folder)

    min_godot_version = read_api_version(args.api_json, args.api_version)
    update_extension(args.extension_file, args.version_file, min_godot_version)

    return 0


if __name__ == "__main__":
    sys.exit(main())
