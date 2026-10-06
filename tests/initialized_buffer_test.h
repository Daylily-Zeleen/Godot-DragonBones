/**************************************************************************/
/*  initialized_buffer_test.h                                             */
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

// 这些 CONFIG 宏必须在任何 TU 首次包含 doctest.h 之前定义；
// test_main.cpp 已定义，这里用 guard 保证单独包含时也成立。
#ifndef DOCTEST_CONFIG_NO_POSIX_SIGNALS
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS
#endif
#ifndef DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#endif

#include <doctest/doctest.h>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <type_traits>
#include <utility>

#include "test_death.h"

#include <initialized_buffer.h>

namespace {

using godot::internal::InitializedBuffer;

// 可平凡构造/析构的负载。
struct Trivial {
	int v = 0;
	bool operator==(const Trivial &p_other) const { return v == p_other.v; }
};

// 记录构造/析构次数的负载，用来验证生命周期语义。
struct Tracked {
	static inline int constructed = 0;
	static inline int destructed = 0;
	static inline int alive() { return constructed - destructed; }
	static inline void reset_counters() {
		constructed = 0;
		destructed = 0;
	}

	int v = 0;
	Tracked() { ++constructed; }
	explicit Tracked(int p_v) :
			v(p_v) { ++constructed; }
	Tracked(const Tracked &p_other) :
			v(p_other.v) { ++constructed; }
	Tracked(Tracked &&p_other) noexcept :
			v(p_other.v) { ++constructed; }
	Tracked &operator=(const Tracked &) = default;
	Tracked &operator=(Tracked &&) = default;
	~Tracked() { ++destructed; }
};

} //namespace

TEST_CASE("InitializedBuffer: default state") {
	InitializedBuffer<Trivial> b;
	CHECK(b.is_empty());
	CHECK(b.size() == 0);
	CHECK(b.get_capacity() == 0);
	CHECK(b.ptr() == nullptr);
	CHECK(b.get_capacity_bytes() == 0);
}

TEST_CASE("InitializedBuffer: reserve grows and keeps existing elements") {
	InitializedBuffer<Trivial> b;
	for (int i = 0; i < 3; ++i) {
		b.push_back([i](Trivial &p_v) { p_v.v = i; });
	}
	CHECK(b.size() == 3);
	const auto cap_before = b.get_capacity();
	REQUIRE(cap_before >= 3);

	b.reserve(cap_before); // 不缩、不增长
	CHECK(b.get_capacity() == cap_before);
	CHECK(b.size() == 3);

	b.reserve(100); // 增长
	CHECK(b.get_capacity() >= 100);
	CHECK(b.size() == 3);
	// 既有元素内容保持
	for (int i = 0; i < 3; ++i) {
		CHECK(b.ptr()[i].v == i);
	}
	CHECK(b.get_capacity_bytes() == b.get_capacity() * sizeof(Trivial));
}

TEST_CASE("InitializedBuffer: tight=true grows exactly") {
	InitializedBuffer<Trivial, uint32_t, /*tight=*/true> b;
	b.reserve(7);
	CHECK(b.get_capacity() == 7);
	b.reserve(7);
	CHECK(b.get_capacity() == 7);
	b.reserve(9);
	CHECK(b.get_capacity() == 9);
}

TEST_CASE("InitializedBuffer: resize grows and shrinks, buffer slots stay valid") {
	InitializedBuffer<Trivial> b;
	b.resize(4);
	CHECK(b.size() == 4);
	CHECK(b.get_capacity() >= 4);
	for (uint32_t i = 0; i < 4; ++i) {
		b.ptr()[i].v = (int)i;
	}

	// 缩小：count 减小，但缓冲槽仍是已构造的有效对象，内容不被销毁。
	b.resize(2);
	CHECK(b.size() == 2);
	CHECK(b.get_capacity() >= 4);
	CHECK(b.ptr()[3].v == 3); // 槽仍有效可读

	// resize(0) 只改 count，不释放
	const auto ptr_before = b.ptr();
	const auto cap_before = b.get_capacity();
	b.resize(0);
	CHECK(b.size() == 0);
	CHECK(b.ptr() == ptr_before);
	CHECK(b.get_capacity() == cap_before);
}

TEST_CASE("InitializedBuffer: clear keeps capacity and does not destruct") {
	Tracked::reset_counters();
	{
		InitializedBuffer<Tracked> b;
		b.resize(3);
		b.ptr()[0].v = 10;
		b.ptr()[1].v = 11;
		const auto ptr_before = b.ptr();
		const auto cap_before = b.get_capacity();
		const int alive_before = Tracked::alive();

		b.clear();
		CHECK(b.size() == 0);
		CHECK(b.ptr() == ptr_before); // 不释放
		CHECK(b.get_capacity() == cap_before);
		CHECK(Tracked::alive() == alive_before); // 不析构
		// 缓冲槽仍有效可读
		CHECK(b.ptr()[0].v == 10);
	}
}

TEST_CASE("InitializedBuffer: reset frees and zeroes state") {
	Tracked::reset_counters();
	{
		InitializedBuffer<Tracked> b;
		b.resize(3);
		CHECK(Tracked::alive() == 3);

		b.reset();
		CHECK(b.ptr() == nullptr);
		CHECK(b.get_capacity() == 0);
		CHECK(b.size() == 0); // D4 回归：reset 必须归零 count
		CHECK(Tracked::alive() == 0); // 全部析构
	}
	CHECK(Tracked::alive() == 0); // 析构函数不会二次 reset 出问题
}

TEST_CASE("InitializedBuffer: push_back overloads") {
	InitializedBuffer<Trivial> b;
	const Trivial by_const{ 1 };
	b.push_back(by_const); // const&
	b.push_back(Trivial{ 2 }); // &&
	b.push_back([](Trivial &p_v) { p_v.v = 3; }); // initializer
	REQUIRE(b.size() == 3);
	CHECK(b.ptr()[0].v == 1);
	CHECK(b.ptr()[1].v == 2);
	CHECK(b.ptr()[2].v == 3);
}

TEST_CASE("InitializedBuffer: push_back beyond initial capacity preserves order") {
	InitializedBuffer<Trivial> b;
	for (int i = 0; i < 50; ++i) {
		b.push_back([i](Trivial &p_v) { p_v.v = i; });
	}
	REQUIRE(b.size() == 50);
	for (int i = 0; i < 50; ++i) {
		CHECK(b.ptr()[i].v == i);
	}
}

TEST_CASE("InitializedBuffer: insert at begin/middle/end/empty") {
	SUBCASE("empty") {
		InitializedBuffer<Trivial> b;
		b.insert(0, [](Trivial &p_v) { p_v.v = 42; });
		REQUIRE(b.size() == 1);
		CHECK(b.ptr()[0].v == 42);
	}
	SUBCASE("at begin") {
		InitializedBuffer<Trivial> b;
		for (int i = 0; i < 3; ++i) {
			b.push_back([i](Trivial &p_v) { p_v.v = i; });
		}
		b.insert(0, [](Trivial &p_v) { p_v.v = 7; });
		REQUIRE(b.size() == 4);
		CHECK(b.ptr()[0].v == 7);
		CHECK(b.ptr()[1].v == 0);
		CHECK(b.ptr()[2].v == 1);
		CHECK(b.ptr()[3].v == 2);
	}
	SUBCASE("in middle") {
		InitializedBuffer<Trivial> b;
		for (int i = 0; i < 4; ++i) {
			b.push_back([i](Trivial &p_v) { p_v.v = i; });
		}
		b.insert(1, [](Trivial &p_v) { p_v.v = 99; });
		REQUIRE(b.size() == 5);
		CHECK(b.ptr()[0].v == 0);
		CHECK(b.ptr()[1].v == 99);
		CHECK(b.ptr()[2].v == 1);
		CHECK(b.ptr()[3].v == 2);
		CHECK(b.ptr()[4].v == 3);
	}
	SUBCASE("at end") {
		InitializedBuffer<Trivial> b;
		for (int i = 0; i < 3; ++i) {
			b.push_back([i](Trivial &p_v) { p_v.v = i; });
		}
		b.insert(3, [](Trivial &p_v) { p_v.v = 5; });
		REQUIRE(b.size() == 4);
		CHECK(b.ptr()[3].v == 5);
	}
}

TEST_CASE("InitializedBuffer: operator[] read/write") {
	InitializedBuffer<Trivial> b;
	b.resize(2);
	b[0].v = 1;
	b[1].v = 2;
	CHECK(b[0].v == 1);
	CHECK(b[1].v == 2);
	const InitializedBuffer<Trivial> &cb = b;
	CHECK(cb[0].v == 1);
	CHECK(cb[1].v == 2);
}

// ---- 越界死亡用例 ----
// CRASH_BAD_INDEX 会 trap 进程，无法在进程内断言；用 test_death.h 的子进程设施。
// 执行体在命名空间作用域注册，通用入口（try_run）不需要知道它们的存在。

GDDB_DEATH_CASE("insert-out-of-bounds") {
	InitializedBuffer<int> b;
	b.push_back(1);
	b.insert(5, [](int &p_v) { p_v = 9; }); // p_at=5 > count=1 → trap
}

GDDB_DEATH_CASE("index-out-of-bounds") {
	InitializedBuffer<int> b;
	b.resize(1);
	const int v = b[5]; // idx=5 >= count=1 → trap
	(void)v;
}

namespace {

// 断言某个死亡用例确实在子进程里 trap 了（退出码非 0，且带回越界诊断）。
void check_death_case_traps(const char *p_name) {
	if (godot::OS::get_singleton() == nullptr) {
		WARN("OS singleton unavailable; skipping subprocess death test");
		return;
	}
	godot::Array output;
	const int32_t rc = gddb::tests::spawn_death_case(p_name, &output);
	// `-1` 是 OS::execute 的启动失败码（见 spawn_death_case 的空 exe 分支）。
	// 注意：trap 在 Windows 上的退出码是 0x80000003（STATUS_BREAKPOINT，即
	// -2147483645），POSIX 上是 SIGTRAP —— 都是负数，因此**不能**用 `rc < 0`
	// 判断“无法拉起”。
	if (rc == -1) {
		WARN("could not spawn subprocess; skipping death test");
		return;
	}
	// OS::execute 把 stdout（p_read_stderr=true 时含 stderr）并入 output 的**一个** String。
	CHECK(rc != 0);
	const bool has_diag = output.size() > 0 && ((godot::String)output[0]).contains("out of bounds");
	CHECK_MESSAGE(has_diag, "expected an 'out of bounds' diagnostic from CRASH_BAD_INDEX");
}

} //namespace

TEST_CASE("InitializedBuffer: insert out-of-bounds traps (D3)") {
	check_death_case_traps("insert-out-of-bounds");
}

TEST_CASE("InitializedBuffer: operator[] out-of-bounds traps") {
	check_death_case_traps("index-out-of-bounds");
}

TEST_CASE("InitializedBuffer: resize return type is void (D1)") {
	// D1 回归：HEAD 的实现声明为 `U resize(U)` 但没有 return 语句（未定义返回值）。
	// 修复后为 `void`。用 decltype 在编译期锁定该契约，避免将来又引入无人使用的返回值。
	static_assert(std::is_same_v<decltype(std::declval<InitializedBuffer<Trivial> &>().resize(0)), void>,
				  "InitializedBuffer::resize must return void (D1)");
	CHECK(true);
}

TEST_CASE("InitializedBuffer: destructor frees without leaking") {
	Tracked::reset_counters();
	{
		InitializedBuffer<Tracked> b;
		b.resize(5);
		CHECK(Tracked::alive() == 5);
	}
	CHECK(Tracked::alive() == 0);
	CHECK(Tracked::constructed == Tracked::destructed);
}
