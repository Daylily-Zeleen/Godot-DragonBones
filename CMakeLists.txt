cmake_minimum_required(VERSION 3.6)
project(dragonbones)

# 不使用 includes_CXX.rsp 来提供包含路径, clangd不能正确使用（c_cpp倒是可以）
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

# 生成 compile_command.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# NOTE: these must use `if(NOT DEFINED ...)`, not `if(<var> STREQUAL "")`.
# In CMake, `if(UNDEFINED_VAR STREQUAL "")` compares the *literal name* against the
# empty string, which is false - so an unset `set(<var> ... CACHE ...)` default would
# never run and the variable would stay empty. `if(NOT DEFINED ...)` is the correct form.
if(NOT DEFINED GODOT_GDEXTENSION_DIR)
	set(GODOT_GDEXTENSION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/godot-cpp/gdextension CACHE STRING "Path to GDExtension interface header directory")
endif()

if(NOT DEFINED CPP_BINDINGS_PATH)
	set(CPP_BINDINGS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/godot-cpp CACHE STRING "Path to godot-cpp source (submodule)")
endif()

if(NOT DEFINED BUILD_SHARED)
	set(BUILD_SHARED ON CACHE STRING "If ON, build shared library for GDExtension, if OFF, build static library without C Extence for GDExtension. Default ON.")
endif()

# godot-cpp 10.x resolves the API version and build profile itself, so this project only
# has to point it at the profile; no local trimming step is needed.
#   GODOTCPP_API_VERSION   -> gdextension/extension_api-<x>-<y>.json
#   GODOTCPP_BUILD_PROFILE -> trims that API file before generating bindings
if(NOT DEFINED GODOTCPP_API_VERSION)
	set(GODOTCPP_API_VERSION "4.3" CACHE STRING "Target Godot API version for godot-cpp bindings")
endif()

if(NOT DEFINED GODOTCPP_BUILD_PROFILE)
	set(GODOTCPP_BUILD_PROFILE ${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json CACHE FILEPATH "Path to a feature build profile JSON for godot-cpp (\"\" disables trimming)")
endif()

if(WIN32 AND NOT DEFINED GODOTCPP_USE_STATIC_CPP)
	set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "Link the C++ runtime statically")
endif()

# C++ 单元测试（doctest）。与 SCons 的 `tests=yes` 等价，二者共用 GDDB_TESTS_ENABLED 宏。
# 默认关闭；开启时编译 tests/ 并让 register_types.cpp 响应 --gddb-run-tests。
if(NOT DEFINED GODOT_DRAGONBONES_TESTS)
	set(GODOT_DRAGONBONES_TESTS OFF CACHE BOOL "Build C++ unit tests (doctest)")
endif()

# 后处理所依赖的路径（与 SConstruct 的同名变量保持一致）。
set(GDDB_PLUGIN_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/demo/addons/godot_dragon_bones.daylily-zeleen" CACHE PATH "demo addon directory that receives the built library")
set(GDDB_EXTENSION_FILE "${GDDB_PLUGIN_FOLDER}/godot_dragon_bones.gdextension" CACHE FILEPATH "The .gdextension file whose version / compatibility_minimum are refreshed")
set(GDDB_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version" CACHE FILEPATH "Single source of truth for the version string")

# 库基名：与 SCons 的 lib_name 一致，产物名必须与 .gdextension 的 [libraries] 引用相符，
# 否则拷进插件目录后 Godot 也加载不到。
set(GDDB_LIB_BASENAME "libgddragonbones")

# .gdextension 的 compatibility_minimum 必须与绑定实际所用的 API 版本一致
# （见 .trellis/spec/build/build-systems.md）。custom_api_file 优先，与 godot-cpp 自身的
# 取值优先级相同；解析工作交给 post_build.py，此处只负责定位那个 JSON。
if(GODOTCPP_CUSTOM_API_FILE)
	set(GDDB_API_JSON "${GODOTCPP_CUSTOM_API_FILE}")
else()
	string(REPLACE "." "-" _api_dashed "${GODOTCPP_API_VERSION}")
	set(GDDB_API_JSON "${GODOT_GDEXTENSION_DIR}/extension_api-${_api_dashed}.json")
endif()

# ---------------------------------------------------------------------------
# godot-cpp
# ---------------------------------------------------------------------------
# Build godot-cpp from the submodule with its own CMake target. Earlier revisions
# expected a prebuilt static library at CPP_BINDINGS_BUILD_PATH (default
# ./thirdparty/godot-cpp/build), a directory the SCons workflow never creates, so the
# link step could not resolve. Building from source also guarantees the generated
# bindings match this build's Godot version and build profile.
add_subdirectory("${CPP_BINDINGS_PATH}" godot-cpp)

# 命名

string(STRIP "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE )
# default debug
if(CMAKE_BUILD_TYPE STREQUAL "")
	set(CMAKE_BUILD_TYPE Debug)
endif()

# Platform
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} ARCH)

if(ARCH MATCHES ".?86\$")
	set(ARCH x86_32)
endif()

# Archtechure Aliases
if(ARCH MATCHES "x64|amd64")
	set(ARCH x86_64)
elseif(ARCH MATCHES "armv7")
	set(ARCH arm32)
elseif(ARCH MATCHES "armv8|armv64v8|aarch64")
	set(ARCH arm64)
elseif(ARCH MATCHES "rv|riscv|riscv64")
	set(ARCH rv64)
elseif(ARCH MATCHES "ppcle|ppc")
	set(ARCH ppc32)
elseif(ARCH MATCHES "ppc64le")
	set(ARCH ppc64)
endif()

if(NOT(ARCH MATCHES "universal|x86_32|x86_64|arm32|arm64|rv64|ppc32|ppc64|wasm32"))
	message(SEND_ERROR "Unsupport architechture: ${ARCH}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
	set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
	set(PLATFORM linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
	set(CMAKE_STATIC_LIBRARY_PREFIX "")
	set(PLATFORM windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(PLATFORM macos)

	if(NOT(ARCH MATCHES "universal|arm64|x86_64"))
		message(SEND_ERROR "Unsupport architechture: ${ARCH}")
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
	set(PLATFORM android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
	set(PLATFORM ios)

	if(NOT(ARCH MATCHES "universal|arm64|x86_64"))
		message(SEND_ERROR "Unsupport architechture: ${ARCH}")
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "WASI")
	set(PLATFORM web)

	if(NOT(ARCH MATCHES "wams32"))
		message(SEND_ERROR "Unsupport architechture: ${ARCH}")
	endif()
endif()

# Target
# 与 SCons 的 env.debug_features（target in [editor, template_debug]）保持一致：
# 凡是调试构建，一律带 TOOLS_ENABLED 并编译 src/editor/*.cpp —— 编辑器插件在
# register_types.cpp 中由 #ifdef TOOLS_ENABLED 门控注册，宏与源文件必须同进同出。
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(TARGET template_debug)
	set(GODOT_DEBUG_FEATURES ON)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	set(TARGET template_release)
	set(GODOT_DEBUG_FEATURES OFF)
else()
	set(TARGET editor)
	set(GODOT_DEBUG_FEATURES ON)
endif()

# ####
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME)

set(TARGET_PATH ${PLATFORM})

# Set the c++ standard to c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GODOT_COMPILE_FLAGS CACHE STRING "Additional godot compile flags.")
set(GODOT_LINKER_FLAGS CACHE STRING "Additional godot linker flags.")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
	set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /WX") # /GF /MP
	set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /DTYPED_METHOD_BIND")

	if(CMAKE_BUILD_TYPE MATCHES Debug)
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /MDd") # /Od /RTC1 /Zi
	elsE()
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /MD /O2") # /Oy /GL /Gy
		STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
		string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	endif(CMAKE_BUILD_TYPE MATCHES Debug)

	# Disable conversion warning, truncation, unreferenced var, signed mismatch
	set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /wd4244 /wd4305 /wd4101 /wd4018 /wd4267")

	add_definitions(-DNOMINMAX)

# Unkomment for warning level 4
# if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
# string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# endif()
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "linux")
		set(GODOT_LINKER_FLAGS "-Wl,-R, '$$ORIGIN'")
	endif()

	set(GODOT_COMPILE_FLAGS "-g -Wwrite-strings")

	if(CMAKE_BUILD_TYPE MATCHES Debug)
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} -fno-omit-frame-pointer -O0")
	else()
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} -O3")
	endif(CMAKE_BUILD_TYPE MATCHES Debug)
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GUN")
	# set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /DTYPED_METHOD_BIND")
	add_definitions(-DNOMINMAX)
	add_definitions(-DTYPED_METHOD_BIND)
endif()

# Disable exception handling. Godot doesn't use exceptions anywhere, and this
# saves around 20% of binary size and very significant build time (GH-80513).
option(GODOT_DISABLE_EXCEPTIONS ON "Force disabling exception handling code")

if(GODOT_DISABLE_EXCEPTIONS)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} -D_HAS_EXCEPTIONS=0")
	else()
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} -fno-exceptions")
	endif()
else()
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /EHsc")
	endif()
endif()

# MSVC 代码字符集设置为utf-8
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	set(GODOT_COMPILE_FLAGS "${GODOT_COMPILE_FLAGS} /utf-8")
endif()

# Get Sources
# NOTE: godot-cpp is a submodule that is built and linked separately (see target_link_libraries
# below), so it must be excluded here, otherwise its sources are compiled twice and produce
# duplicate symbols at link time.
file(GLOB_RECURSE SOURCES src/*.c** thirdparty/dragonBones/*.c**)

# 与 SCons 的 add_sources_recursively("src/", sources, ["editor"]) 一致：editor 目录先排除，
# 只有调试构建（GODOT_DEBUG_FEATURES）才加入。否则 release 构建会编译编辑器插件，
# 却因缺少 TOOLS_ENABLED 而永不注册（见下方 target_compile_definitions）。
list(FILTER SOURCES EXCLUDE REGEX "/editor/")
if(GODOT_DEBUG_FEATURES)
	file(GLOB_RECURSE EDITOR_SOURCES src/editor/*.c**)
	list(APPEND SOURCES ${EDITOR_SOURCES})
endif()

file(GLOB_RECURSE HEADERS src/*.h** thirdparty/dragonBones/*.h** thirdparty/rapidjson/*.h**)

# Define our library
if(BUILD_SHARED)
	add_library(${PROJECT_NAME} SHARED ${SOURCES} register_types.cpp ${HEADERS})
else()
	add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
endif()

# C++ 单元测试：与 SCons 的 tests=yes 等价（同一宏、同一源集合）。
if(GODOT_DRAGONBONES_TESTS)
	target_compile_definitions(${PROJECT_NAME} PRIVATE GDDB_TESTS_ENABLED)
	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
	file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
	target_sources(${PROJECT_NAME} PRIVATE ${TEST_SOURCES})
endif()

# 调试构建（template_debug / editor）定义 TOOLS_ENABLED，与 SCons 的 env.debug_features 一致。
if(GODOT_DEBUG_FEATURES)
	target_compile_definitions(${PROJECT_NAME} PRIVATE TOOLS_ENABLED)
endif()

target_include_directories(${PROJECT_NAME} SYSTEM
	PRIVATE
	${GODOT_GDEXTENSION_DIR}
	./src
	./thirdparty
)

set(compiler_is_msvc "$<CXX_COMPILER_ID:MSVC>")
target_link_options(${PROJECT_NAME} PRIVATE
	# -static-libgcc / -static-libstdc++ are GCC-driver flags and are rejected by
	# lld-link, so on Windows they are dropped for every compiler.
	$<$<AND:$<NOT:${compiler_is_msvc}>,$<NOT:$<PLATFORM_ID:Windows>>>:
	-static-libgcc
	-static-libstdc++
	>
)

# Create the correct name (godot.os.build_type.system_bits)
# Synchronized with godot-cpp's CMakeLists.txt
set(BITS 32)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(BITS 64)
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(GODOT_CPP_BUILD_TYPE Debug)
else()
	set(GODOT_CPP_BUILD_TYPE Release)
endif()

string(TOLOWER ${GODOT_CPP_BUILD_TYPE} BUILD_TYPE)

if(ANDROID)
	# Added the android abi after system name
	set(PLATFORM ${PLATFORM}.${ANDROID_ABI})
endif()

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	godot::cpp
)

# Add the compile flags
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${GODOT_COMPILE_FLAGS})
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS ${GODOT_LINKER_FLAGS})

# set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
if(ANDROID)
	# Added the android abi after system name
	set(SYSTEM_NAME ${SYSTEM_NAME}.${ANDROID_ABI})

	# Android does not have the bits at the end if you look at the main godot repo build
	set(OUTPUT_NAME "${GDDB_LIB_BASENAME}.${PLATFORM}.${TARGET}")
else()
	set(OUTPUT_NAME "${GDDB_LIB_BASENAME}.${PLATFORM}.${TARGET}.${ARCH}")
endif()

# Change the output directory to the bin directory
set(BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin) # /${TARGET_PATH})
set_target_properties(${PROJECT_NAME}
	PROPERTIES
	CXX_EXTENSIONS OFF
	POSITION_INDEPENDENT_CODE ON
	# PREFIX "" ：OUTPUT_NAME 已含 "lib"，若再叠加平台默认前缀（Linux/macOS 为 "lib"）
	# 会得到 "liblibgddragonbones"。SCons 是显式给全名的，这里对齐。
	PREFIX ""
	ARCHIVE_OUTPUT_DIRECTORY "${BUILD_PATH}"
	LIBRARY_OUTPUT_DIRECTORY "${BUILD_PATH}"
	RUNTIME_OUTPUT_DIRECTORY "${BUILD_PATH}"

	OUTPUT_NAME "${OUTPUT_NAME}"
)

# ---------------------------------------------------------------------------
# 后处理（与 SCons 共用 misc/post_build.py，勿在本文件重复实现）
# ---------------------------------------------------------------------------
# 把产物拷进 demo 插件目录、同步 README/LICENSE、刷新 .gdextension 的
# version / compatibility_minimum。
#
# - linux / windows / android：搬运产物（名字取自 $<TARGET_FILE_NAME>，与 SCons 的
#   {lib_name}.{platform}.{target}.{arch}{ext} 一致）；
# - macOS / iOS：SCons 产出 .framework / .xcframework（后者由 generate_xcframework.sh
#   二次打包），CMake 不生成这些容器，故只同步 README/LICENSE 与版本信息。
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
	set(GDDB_POST_BUILD_MODE "none")
	message(STATUS "CMake 后处理：${CMAKE_SYSTEM_NAME} 的 framework/xcframework 打包不在本脚本范围内，仅同步 README/LICENSE 与版本。")
else()
	set(GDDB_POST_BUILD_MODE "copy")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/misc/post_build.py"
		"--src-lib=$<TARGET_FILE:${PROJECT_NAME}>"
		"--dest-lib=${GDDB_PLUGIN_FOLDER}/bin/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		"--mode=${GDDB_POST_BUILD_MODE}"
		"--plugin-folder=${GDDB_PLUGIN_FOLDER}"
		"--repo-root=${CMAKE_CURRENT_SOURCE_DIR}"
		"--extension-file=${GDDB_EXTENSION_FILE}"
		"--version-file=${GDDB_VERSION_FILE}"
		"--api-json=${GDDB_API_JSON}"
		"--api-version=${GODOTCPP_API_VERSION}"
	COMMENT "Post-build: copy library and sync addon files"
	VERBATIM
)
