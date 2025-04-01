#include "dragonbones.h"

using namespace godot;

DragonBones *DragonBones::singleton = nullptr;

DragonBones::DragonBones() {
	singleton = this;
}

DragonBones::~DragonBones() {
	flush();
	memdelete(instance);
}
