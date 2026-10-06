/**************************************************************************/
/*  test_death.h                                                          */
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

// 子进程「死亡测试」基础设施。
//
// CRASH_BAD_INDEX / ERR_FAIL 之类的不可恢复失败会 trap 进程，无法在进程内断言。
// 通用做法：让被测代码在**子进程**里跑，父进程检查子进程的退出码。
//
// 关键点：通用测试入口（test_runner.h 的 try_run）**不应知道任何具体用例**。
// 用例自己在命名空间作用域用 GDDB_DEATH_CASE("<name>") { ... } 注册子进程执行体；
// 入口只按命令行 `--gddb-death=<name>` 查表派发。新增用例无需改动入口。

#include <cstdlib>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace gddb {
namespace tests {

// 子进程执行体：正常路径下应在内部 trap，不返回。
using DeathBody = void (*)();

// 注册表节点必须是 POD 且不含堆分配：注册发生在**命名空间作用域的静态初始化**阶段，
// 那时 Godot 的内存系统尚未就绪（用 LocalVector/Godot 容器会抛异常，导致 DLL 加载
// 失败 —— Windows 上表现为 "Error 1114: DLL 初始化例程失败"）。
// 这里用侵入式单链表串起各注册项，节点本身就存在各 TU 的静态存储里。
struct DeathCase {
	const char *name;
	DeathBody body;
	DeathCase *next;
};

// 链表头：零初始化（不是动态初始化），因此不依赖任何运行时构造。
inline DeathCase *&death_case_head() {
	static DeathCase *head = nullptr;
	return head;
}

inline void register_death_case(DeathCase *p_case) {
	p_case->next = death_case_head();
	death_case_head() = p_case;
}

} //namespace tests
} //namespace gddb

// 在命名空间作用域注册一个子进程执行体。
//   GDDB_DEATH_CASE("my-case") { /* 会 trap 的代码 */ }
#define GDDB_DEATH_CASE_NAME2(a, b) a##b
#define GDDB_DEATH_CASE_NAME(a, b) GDDB_DEATH_CASE_NAME2(a, b)
#define GDDB_DEATH_CASE(name)                                                                       \
	static void GDDB_DEATH_CASE_NAME(_gddb_death_body_, __LINE__)();                                \
	static ::gddb::tests::DeathCase GDDB_DEATH_CASE_NAME(_gddb_death_node_, __LINE__) = {           \
		name, &GDDB_DEATH_CASE_NAME(_gddb_death_body_, __LINE__), nullptr                           \
	};                                                                                              \
	namespace {                                                                                     \
	struct GDDB_DEATH_CASE_NAME(_gddb_death_reg_, __LINE__) {                                       \
		GDDB_DEATH_CASE_NAME(_gddb_death_reg_, __LINE__)() {                                        \
			::gddb::tests::register_death_case(&GDDB_DEATH_CASE_NAME(_gddb_death_node_, __LINE__)); \
		}                                                                                           \
	};                                                                                              \
	const GDDB_DEATH_CASE_NAME(_gddb_death_reg_, __LINE__)                                          \
			GDDB_DEATH_CASE_NAME(_gddb_death_reg_inst_, __LINE__);                                  \
	}                                                                                               \
	static void GDDB_DEATH_CASE_NAME(_gddb_death_body_, __LINE__)()

namespace gddb {
namespace tests {

inline constexpr char DEATH_CASE_ARG_PREFIX[] = "--gddb-death=";

// 子进程侧的派发：命令行里出现 `--gddb-death=<name>` 时执行对应执行体。
// 返回 true 表示「本次进程就是被当作子进程启动的」（调用方不应再继续走正常测试流程）。
inline bool try_run_death_case() {
	for (const godot::String &arg : godot::OS::get_singleton()->get_cmdline_args()) {
		if (!arg.begins_with(DEATH_CASE_ARG_PREFIX)) {
			continue;
		}
		const godot::String name = arg.substr(sizeof(DEATH_CASE_ARG_PREFIX) - 1);

		for (const DeathCase *c = death_case_head(); c != nullptr; c = c->next) {
			if (name == c->name) {
				// 进入标记必须**立即 flush**：执行体会 trap（SIGILL/SIGTRAP）终止进程，
				// 而未 flush 的 stdio 缓冲会随之丢失，父进程将收不到任何输出。
				// 父进程以「有进入标记、且无未-trap 标记」判定用例通过。
				godot::UtilityFunctions::print("[gddb] death case: ", name);
				godot::_err_flush_stdout();

				c->body(); // 正常路径下不返回

				godot::UtilityFunctions::print("[gddb] death case did NOT trap: ", name);
				godot::_err_flush_stdout();
				std::exit(EXIT_SUCCESS);
			}
		}

		godot::UtilityFunctions::print("[gddb] unknown death case: ", name);
		godot::_err_flush_stdout();
		std::exit(EXIT_SUCCESS);
	}
	return false;
}

// 父进程侧：以 `<name>` 启动自身子进程，输出写入 r_output。
inline int32_t spawn_death_case(const char *p_name, godot::Array *r_output = nullptr) {
	const godot::String exe = godot::OS::get_singleton()->get_executable_path();
	if (exe.is_empty()) {
		return -1;
	}
	// 子进程需要与父进程相同的工程路径（`--path res://` 不是合法工程路径）。
	const godot::String project_dir = godot::ProjectSettings::get_singleton()->globalize_path("res://");

	godot::PackedStringArray args;
	args.push_back("--path");
	args.push_back(project_dir);

	// 必须继承 headless：无显示设备的环境（CI）下子进程若尝试创建显示服务会直接失败退出，
	// 扩展初始化（因而测试入口）根本不会执行，标记也就无从产生。
	for (const godot::String &arg : godot::OS::get_singleton()->get_cmdline_args()) {
		if (arg == "--headless") {
			args.push_back("--headless");
			break;
		}
	}

	args.push_back(godot::String(DEATH_CASE_ARG_PREFIX) + p_name);

	godot::Array output;
	const int32_t rc = godot::OS::get_singleton()->execute(exe, args, output, true);
	if (r_output != nullptr) {
		*r_output = output;
	}
	return rc;
}

// 子进程的完整 stdout 合并为一个字符串（OS::execute 的行为：整个管道内容作为单个元素）。
inline godot::String death_case_output(const godot::Array &p_output) {
	return p_output.size() > 0 ? (godot::String)p_output[0] : godot::String();
}

} //namespace tests
} //namespace gddb
