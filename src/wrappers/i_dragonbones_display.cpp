#include "i_dragonbones_display.h"
#include "../dragonbones_slot.h"

namespace godot {

IDragonBonesDisplay::~IDragonBonesDisplay() {
	if (slot) {
		slot->clear_display();
		slot = nullptr;
	}
}

} //namespace godot