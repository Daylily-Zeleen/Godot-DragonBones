#pragma once

#include <godot_dragonbones.h>

#include <dragonBones/core/DragonBones.h>
#include <dragonBones/event/IEventDispatcher.h>

namespace godot {
class DragonBones : public dragonBones::IEventDispatcher {
private:
	static DragonBones *singleton;

private:
	dragonBones::DragonBones *instance{ memnew(dragonBones::DragonBones(this)) };

public:
	static DragonBones *get_singleton() { return singleton; }

public:
	// IEventDispatcher 
	virtual void addDBEventListener(const std::string &p_type, const std::function<void(dragonBones::EventObject *)> &p_listener) override {}
	virtual void removeDBEventListener(const std::string &p_type, const std::function<void(dragonBones::EventObject *)> &p_listener) override {}
	virtual bool hasDBEventListener(const std::string &p_type) const override { return true; }
	virtual void dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) override {}

public:
	DragonBones();
	~DragonBones();

	void flush() { instance->advanceTime(0.0f); }
	dragonBones::DragonBones *get_dragon_bones_instance() const { return instance; }
};

} //namespace godot
