#
# Cmake configuration file
#

############################# INITIAL SECTION ##########################
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(lgspkctl C)

set(PACKAGE_VERSION_MAJOR	1)
set(PACKAGE_VERSION_MINOR	0)
set(PACKAGE_VERSION_PATCH	0)

set(PACKAGE_NAME		"${PROJECT_NAME}")
set(PACKAGE_VERSION		"${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_URL			"https://github.com/rozhuk-im/lgspkctl")
set(PACKAGE_BUGREPORT		"https://github.com/rozhuk-im/lgspkctl")
set(PACKAGE_STRING 		"${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_DESCRIPTION		"Tool to control and configure LG sounbar")
set(PACKAGE_TARNAME		"${PACKAGE_NAME}-${PACKAGE_VERSION}")

############################# OPTIONS SECTION ##########################
option(ENABLE_COVERAGE		"Build with code coverage options [default: OFF]"	OFF)
option(ENABLE_FULL_DEBUG	"Build with all possible debug [default: OFF]"		OFF)

############################# INCLUDE SECTION ##########################
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCCompilerFlag)


find_library(PTHREAD_LIBRARY pthread)
list(APPEND CMAKE_REQUIRED_LIBRARIES ${PTHREAD_LIBRARY})

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})
link_directories(${OPENSSL_LIBRARIES})
list(APPEND CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})

############################# MACRO SECTION ############################
macro(try_c_flag prop flag)
	# Try flag once on the C compiler
	check_c_compiler_flag("-Werror ${flag}" C_FLAG_${prop})
	if (C_FLAG_${prop})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif()
endmacro()

macro(try_linker_flag prop flag)
	# Check with the C compiler
	set(CMAKE_REQUIRED_FLAGS ${flag})
	check_c_compiler_flag(${flag} LINKER_FLAG_${prop})
	set(CMAKE_REQUIRED_FLAGS "")
	if (LINKER_FLAG_${prop})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${flag}")
	endif()
endmacro()

############################# CONFIG SECTION ###########################
# Prefer local include dirs to system ones.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_SOURCE_DIR}/lib/json"
		"${CMAKE_BINARY_DIR}/src")

# Platform specific configuration.
set(TAR "tar")
if (CMAKE_SYSTEM_NAME MATCHES "^.*BSD$|DragonFly")
	set(TAR "gtar")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(TAR "gnutar")
endif()


try_c_flag(PIPE "-pipe")

# Process with warn flags.
if (NOT CMAKE_C_OPT_FLAGS AND ENABLE_FULL_DEBUG)
	try_c_flag(W			"-W")
	try_c_flag(WALL			"-Wall")
	try_c_flag(WPOINTER		"-Wpointer-arith")
	try_c_flag(WPARAM		"-Wno-unused-parameter")
	try_c_flag(WFUNCTION		"-Wno-unused-function")
	try_c_flag(WUNUSED_VAR		"-Wno-unused-variable")
	try_c_flag(WPOINTER_SIGN	"-Wno-pointer-sign")
	try_c_flag(WSIGN_COMPARE	"-Wno-sign-compare")
	try_c_flag(WSTRICT_PROTOTYPES	"-Wstrict-prototypes")
	try_c_flag(PEDANTIC		"-pedantic")
	try_c_flag(WNO_UNUSED_CONST	"-Wno-unused-const-variable")
	try_c_flag(WNULL_DEREFERENCE	"-Wnull-dereference")
	try_c_flag(WDUPLICATED_COND	"-Wduplicated-cond")
	try_c_flag(WIMPLICIT_FALLTHROUGH "-Wimplicit-fallthrough")
endif()

if (NOT "${CMAKE_C_COMPILER_ID}" MATCHES SunPro)
	try_c_flag(STD11		"-std=c11")
	if (NOT C_FLAG_STD11)
		try_c_flag(STD99	"-std=c99")
	endif()
endif()


# Hardening flags
try_c_flag(FORTIFY_SOURCE2		"-D_FORTIFY_SOURCE=2")
try_c_flag(FSTACK_PROTECTOR_ALL		"-fstack-protector-all")
try_c_flag(FSANITIZE_SAFE_STACK		"-fsanitize=safe-stack")
try_c_flag(FSANITIZE_CFI		"-fsanitize=cfi")
try_c_flag(MRETPOLINE			"-mretpoline")
try_c_flag(MFUNCTION_RETURN		"-mfunction-return=thunk")
try_c_flag(MINDIRECT_BRANCH		"-mindirect-branch=thunk")
try_c_flag(FWRAPV			"-fwrapv")
try_c_flag(FPIE				"-fPIE")
if (C_FLAG_FPIE)
	try_linker_flag(PIE		"-pie")
endif()
try_linker_flag(RETPOLINEPLT		"-Wl,-z,retpolineplt")
try_linker_flag(ZRELRO			"-Wl,-z,relro")
try_linker_flag(ZNOW			"-Wl,-z,now")
try_linker_flag(ZNOEXECSTACK		"-Wl,-z,noexecstack")


# Optimization flags
if (ENABLE_COVERAGE)
	set(CMAKE_C_OPT_FLAGS "-g -O0 -fno-strict-aliasing")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
else()
	if (NOT CMAKE_C_OPT_FLAGS)
		if (ENABLE_FULL_DEBUG)
			set(CMAKE_C_OPT_FLAGS "-g -O0 -fstrict-aliasing")
		else()
			set(CMAKE_C_OPT_FLAGS "-g -O2 -fstrict-aliasing")
		endif()
	endif()
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_OPT_FLAGS} ${CMAKE_C_FLAGS}")
# Silently strip whitespace
string(STRIP "${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
string(STRIP "${CMAKE_MODULE_LINKER_FLAGS}" CMAKE_MODULE_LINKER_FLAGS)
string(STRIP "${CMAKE_SHARED_LINKER_FLAGS}" CMAKE_SHARED_LINKER_FLAGS)

################################ SUBDIRS SECTION #######################
include(lib/liblcb/CMakeLists.txt)
add_subdirectory(src)

############################ TARGETS SECTION ###########################
add_custom_target(dist ${CMAKE_CURRENT_SOURCE_DIR}/dist.sh
	"${CMAKE_BINARY_DIR}/${PACKAGE_TARNAME}.tar.xz" "${TAR}"
	COMMENT "Create source distribution"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

##################### INSTALLATION #####################################

