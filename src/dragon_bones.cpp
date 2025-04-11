#include "dragon_bones.h"

using namespace godot;

DragonBones *DragonBones::singleton = nullptr;

DragonBones::DragonBones() {
	singleton = this;
}

DragonBones::~DragonBones() {
	flush();
	memdelete(instance);
	clean_static();
	singleton = nullptr;
}

LocalVector<DragonBones::CleanCallback *> DragonBones::clean_callbacks{};

void DragonBones::add_clean_static_callback(CleanCallback *p_func) {
	clean_callbacks.push_back(p_func);
}

void DragonBones::clean_static() {
	for (CleanCallback *cb : clean_callbacks) {
		cb();
	}
	clean_callbacks.clear();
}
