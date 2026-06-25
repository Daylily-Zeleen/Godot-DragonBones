/**************************************************************************/
/*  dragon_bones.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           Godot-DragonBones                            */
/*        https://github.com/Daylily-Zeleen/Godot-DragonBones             */
/**************************************************************************/
/* Copyright (c) 2024-present 忘忧の (Daylily-Zeleen)                      */
/*               - Contact: daylily-zeleen@foxmail.com                    */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <godot_dragon_bones.h>

#include <dragonBones/core/DragonBones.h>
#include <dragonBones/event/IEventDispatcher.h>

#include <godot_cpp/templates/local_vector.hpp>

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

public:
	using CleanCallback = void();
	static void add_clean_static_callback(CleanCallback *p_func);

private:
	void clean_static();
	static LocalVector<CleanCallback *> clean_callbacks;
};

} //namespace godot
