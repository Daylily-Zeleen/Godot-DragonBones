#include "GDDisplay.h"
#include "../dragonbones_slot.h"

namespace godot {

GDDisplay::~GDDisplay() {
	if (slot) {
		slot->clear_display();
		slot = nullptr;
	}
}

} //namespace godot