#!/bin/sh

echo $1 $2

scons arch=universal ios_simulator=yes platform=ios target=$1 $2
scons arch=arm64 ios_simulator=no platform=ios target=$1 $2

xcodebuild -create-xcframework \
-library ./bin/libgddragonbones.ios.$1.a \
-library ./bin/libgddragonbones.ios.$1.simulator.a \
-output ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgddragonbones.ios.$1.xcframework

xcodebuild -create-xcframework \
-library ./godot-cpp/bin/libgodot-cpp.ios.$1.arm64.a \
-library ./godot-cpp/bin/libgodot-cpp.ios.$1.universal.simulator.a \
-output ./demo/addons/godot_dragon_bones.daylily-zeleen/bin/libgodot-cpp.ios.$1.xcframework
