#!/bin/sh

echo $1 $2

# scons arch=universal ios_simulator=yes platform=ios target=$1 $2
scons arch=arm64 ios_simulator=no platform=ios target=$1 $2

xcodebuild -create-xcframework \
-library ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgddragonbones.ios.$1.a \
-output ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgddragonbones.ios.$1.xcframework
# -library ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgddragonbones.ios.$1.simulator.a \

xcodebuild -create-xcframework \
-library ./godot-cpp/bin/libgodot-cpp.ios.$1.arm64.a \
-output ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgodot-cpp.ios.$1.xcframework
# -library ./godot-cpp/bin/libgodot-cpp.ios.$1.universal.simulator.a \
