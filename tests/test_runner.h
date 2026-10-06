/**************************************************************************/
/*  test_runner.h                                                         */
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

// 测试入口：仅在 GDDB_TESTS_ENABLED 下由 register_types.cpp 调用。
// doctest 的输出默认走 stdout，在部分平台的 runtime 库内会静默失败，
// 因此这里注册自定义 reporter，把结果经 Godot 的 print 落到日志。

#include <cstdlib>

#ifdef GDDB_TESTS_ENABLED

#define DOCTEST_CONFIG_NO_POSIX_SIGNALS
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include <doctest/doctest.h>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "test_death.h"

namespace gddb {
namespace tests {

namespace detail {

using godot::String;
using godot::vformat;

class ConsoleReporter final : public doctest::IReporter {
	static inline bool s_case_header_logged = false;
	static inline const char *s_current_case_name = "";

public:
	explicit ConsoleReporter(const doctest::ContextOptions &) {}

	void test_case_start(const doctest::TestCaseData &p_data) override {
		s_case_header_logged = false;
		s_current_case_name = p_data.m_name;
	}
	void test_case_reenter(const doctest::TestCaseData &) override { s_case_header_logged = false; }

	void log_message(const doctest::MessageData &p_msg) override {
		log_case_header(p_msg.m_file, p_msg.m_line);
		print_line(p_msg.m_file, p_msg.m_line, "MESSAGE: " + String(p_msg.m_string.c_str()));
	}

	void log_assert(const doctest::AssertData &p_assert) override {
		log_case_header(p_assert.m_file, p_assert.m_line);
		if (p_assert.m_failed) {
			String text = "ERROR: " + String(p_assert.m_expr);
			if (p_assert.m_threw) {
				text += " threw: " + String(p_assert.m_exception.c_str());
			} else if (p_assert.m_decomp.c_str() && *p_assert.m_decomp.c_str()) {
				text += " values: " + String(p_assert.m_decomp.c_str());
			}
			print_line(p_assert.m_file, p_assert.m_line, text);
		}
	}

	void test_case_exception(const doctest::TestCaseException &p_exc) override {
		print_raw(vformat("ERROR: test case %s: %s",
						  p_exc.is_crash ? "CRASHED" : "THREW exception",
						  String(p_exc.error_string.c_str())));
	}

	void test_run_end(const doctest::TestRunStats &p_stats) override {
		print_raw(vformat(
				"[doctest] test cases: %d | %d passed | %d failed | 0 skipped\n"
				"[doctest] assertions: %d | %d passed | %d failed |\n%s",
				(int64_t)p_stats.numTestCasesPassingFilters,
				(int64_t)(p_stats.numTestCasesPassingFilters - p_stats.numTestCasesFailed),
				(int64_t)p_stats.numTestCasesFailed,
				p_stats.numAsserts,
				p_stats.numAsserts - p_stats.numAssertsFailed,
				p_stats.numAssertsFailed,
				p_stats.numTestCasesFailed == 0 && p_stats.numAssertsFailed == 0
						? "[doctest] Status: SUCCESS!\n"
						: "[doctest] Status: FAILURE!\n"));
	}

	// 未使用的回调
	void report_query(const doctest::QueryData &) override {}
	void test_run_start() override {}
	void test_case_end(const doctest::CurrentTestCaseStats &) override {}
	void subcase_start(const doctest::SubcaseSignature &) override {}
	void subcase_end() override {}
	void test_case_skipped(const doctest::TestCaseData &) override {}

private:
	static void print_raw(const String &p_text) {
		godot::UtilityFunctions::print(p_text);
	}

	static void log_case_header(const char *p_file, int p_line) {
		if (s_case_header_logged) {
			return;
		}
		s_case_header_logged = true;
		print_raw("===============================================================================");
		print_raw(vformat("[TEST CASE] %s (%s:%d)", s_current_case_name, p_file, p_line));
	}

	static void print_line(const char *p_file, int p_line, const String &p_text) {
		print_raw(vformat("%s(%d): %s", p_file, p_line, p_text));
	}
};

} //namespace detail

// 测试入口：由 src/dragon_bones_registration.cpp 在模块初始化时调用。
// 只负责两件事——「子进程死亡用例派发」与「运行 doctest」——
// 具体用例一律不在此处出现（见 test_death.h）。
inline void try_run() {
	using namespace godot;

	// 若本次进程是被当作死亡用例的子进程启动的，直接执行并结束，不再跑正常测试。
	if (try_run_death_case()) {
		return;
	}

	bool run_tests = false;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg == "--gddb-run-tests") {
			run_tests = true;
			break;
		}
	}
	if (!run_tests) {
		return;
	}

	UtilityFunctions::print("[gddb] running tests");
	doctest::registerReporter<detail::ConsoleReporter>("direct", 0, true);
	const char *argv[] = { "gddb", "--reporters=direct", "--no-colors" };
	doctest::Context context;
	context.applyCommandLine(2, argv);
	const int exit_code = context.run();

	UtilityFunctions::print("[gddb] running tests result: ", exit_code);
	// 测试构建以进程退出码反馈结果。SceneTree 不在 build_profile 中（加它会牵入
	// Node 等一串依赖），故不走 engine 的优雅退出，直接以退出码结束进程。
	std::exit(exit_code);
}

} //namespace tests
} //namespace gddb

#endif // GDDB_TESTS_ENABLED
