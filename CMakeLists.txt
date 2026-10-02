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
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(TARGET template_debug)
	add_definitions(-DTOOLS_ENABLED=1)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	set(TARGET template_release)
else()
	set(TARGET editor)
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

file(GLOB_RECURSE HEADERS src/*.h** thirdparty/dragonBones/*.h** thirdparty/rapidjson/*.h**)

# Define our library
if(BUILD_SHARED)
	add_library(${PROJECT_NAME} SHARED ${SOURCES} register_types.cpp ${HEADERS})
else()
	add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
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
	set(OUTPUT_NAME "${PROJECT_NAME}.${PLATFORM}.${TARGET}")
else()
	set(OUTPUT_NAME "${PROJECT_NAME}.${PLATFORM}.${TARGET}.${ARCH}")
endif()

# Change the output directory to the bin directory
set(BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin) # /${TARGET_PATH})
set_target_properties(${PROJECT_NAME}
	PROPERTIES
	CXX_EXTENSIONS OFF
	POSITION_INDEPENDENT_CODE ON
	ARCHIVE_OUTPUT_DIRECTORY "${BUILD_PATH}"
	LIBRARY_OUTPUT_DIRECTORY "${BUILD_PATH}"
	RUNTIME_OUTPUT_DIRECTORY "${BUILD_PATH}"

	OUTPUT_NAME "${OUTPUT_NAME}"
)
