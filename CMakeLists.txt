cmake_minimum_required(VERSION 3.16)

# CMake 4 removed compatibility with projects that request CMake < 3.5.
# Some third-party libraries (for example gl2d) still use an older minimum.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(mygame)

set(CMAKE_CXX_STANDARD 17)

# ************************
# Set this to ON instead of OFF to ship the game
# This will change the RESOURCES_PATH to be the local path
# ************************
set(PRODUCTION_BUILD OFF CACHE BOOL "Make this a production build" FORCE)
# delete the "out" folder after changing if VSCode doesn't recognize the change

# SIMD flags. This has to come AFTER project(): MSVC, MINGW and the other
# compiler-detection variables only exist once CMake has tested the compiler,
# so a check before project() would always fall through to the GCC/Clang flags.
#   - MSVC and clang-cl accept /arch:AVX2.
#   - GCC/Clang accept -mavx2 -mfma, but only when targeting x86; those flags
#     do not exist on other architectures (ARM, Apple Silicon, ...), so they
#     are skipped there instead of failing the build.
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options(/arch:AVX2) # make sure SIMD optimizations take place
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|i[3-6]86)")
	add_compile_options(-mavx2 -mfma) # make sure SIMD optimizations take place
else()
	message(STATUS "No AVX2 flags for ${CMAKE_SYSTEM_PROCESSOR} - skipping SIMD optimizations")
endif()

# ************************
# Third-party libraries
# ************************
# All libraries are installed in the "thirdparty" folder. Every library is
# checked before it is downloaded, so an existing installation is reused.
set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

if(NOT EXISTS "${THIRDPARTY_DIR}")
	message(STATUS "Creating thirdparty directory: ${THIRDPARTY_DIR}")
	file(MAKE_DIRECTORY "${THIRDPARTY_DIR}")
endif()

find_package(Git QUIET)

# Clone a library from git into thirdparty/<name>.
# <marker> is a file that must exist for the library to be considered installed.
function(install_thirdparty name repository tag marker)
	if(EXISTS "${THIRDPARTY_DIR}/${name}/${marker}")
		message(STATUS "${name} is already installed - skipping download")
		return()
	endif()

	if(NOT Git_FOUND)
		message(FATAL_ERROR
			"${name} is not installed and Git could not be found. "
			"Install Git or place ${name} manually in ${THIRDPARTY_DIR}/${name}.")
	endif()

	message(STATUS "Installing ${name} into ${THIRDPARTY_DIR}/${name}")

	# Remove a broken/partial installation before cloning again.
	if(EXISTS "${THIRDPARTY_DIR}/${name}")
		file(REMOVE_RECURSE "${THIRDPARTY_DIR}/${name}")
	endif()

	execute_process(
		COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${tag}" "${repository}" "${THIRDPARTY_DIR}/${name}"
		RESULT_VARIABLE git_result
		OUTPUT_VARIABLE git_output
		ERROR_VARIABLE git_output
	)

	if(NOT git_result EQUAL 0)
		file(REMOVE_RECURSE "${THIRDPARTY_DIR}/${name}")
		message(FATAL_ERROR "Failed to install ${name}:\n${git_output}")
	endif()
endfunction()

install_thirdparty(glfw "https://github.com/glfw/glfw.git" "3.4" "CMakeLists.txt")
install_thirdparty(glm "https://github.com/g-truc/glm.git" "1.0.3" "CMakeLists.txt")
install_thirdparty(gl2d "https://github.com/meemknight/gl2d.git" "master" "gl2d/CMakeLists.txt")

# gl2d already ships glad and the stb headers, so reuse those instead of
# downloading duplicates.
function(install_gl2d_dependency name marker)
	if(EXISTS "${THIRDPARTY_DIR}/${name}/${marker}")
		message(STATUS "${name} is already installed - skipping download")
		return()
	endif()

	message(STATUS "Installing ${name} from gl2d")
	file(REMOVE_RECURSE "${THIRDPARTY_DIR}/${name}")
	if(NOT EXISTS "${THIRDPARTY_DIR}/gl2d/thirdparty/${name}")
		message(FATAL_ERROR "gl2d does not contain the bundled dependency '${name}'.")
	endif()
	file(COPY "${THIRDPARTY_DIR}/gl2d/thirdparty/${name}" DESTINATION "${THIRDPARTY_DIR}")
endfunction()

install_gl2d_dependency(glad "src/glad.c")
install_gl2d_dependency(stb_image "src/stb_image.cpp")
install_gl2d_dependency(stb_truetype "src/stb_truetype.cpp")

# gl2d's bundled stb_image uses std::memcpy without including <cstring>.
# Patch it once after installation so it also builds with modern compilers.
set(STB_IMAGE_HEADER "${THIRDPARTY_DIR}/stb_image/include/stb_image/stb_image.h")
if(EXISTS "${STB_IMAGE_HEADER}")
	file(READ "${STB_IMAGE_HEADER}" STB_IMAGE_HEADER_CONTENT)
	string(FIND "${STB_IMAGE_HEADER_CONTENT}" "#include <cstring>" STB_CSTRING_INCLUDE)
	if(STB_CSTRING_INCLUDE EQUAL -1)
		string(REPLACE "#include <memory>" "#include <memory>\n#include <cstring>"
			STB_IMAGE_HEADER_CONTENT "${STB_IMAGE_HEADER_CONTENT}")
		file(WRITE "${STB_IMAGE_HEADER}" "${STB_IMAGE_HEADER_CONTENT}")
	endif()
endif()

# GLFW build options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory("${THIRDPARTY_DIR}/glfw")
add_subdirectory("${THIRDPARTY_DIR}/glm")

# glad and the stb libraries ship with gl2d. Build them here so gl2d can link
# against the standard target names.
add_library(glad STATIC "${THIRDPARTY_DIR}/glad/src/glad.c")
target_include_directories(glad PUBLIC "${THIRDPARTY_DIR}/glad/include")

add_library(stb_image STATIC "${THIRDPARTY_DIR}/stb_image/src/stb_image.cpp")
target_include_directories(stb_image PUBLIC "${THIRDPARTY_DIR}/stb_image/include")
# gl2d's copy of stb_image uses std::memcpy but does not include <cstring>.
if(MSVC)
	target_compile_options(stb_image PRIVATE /FIcstring)
else()
	target_compile_options(stb_image PRIVATE -include cstring)
endif()

add_library(stb_truetype STATIC "${THIRDPARTY_DIR}/stb_truetype/src/stb_truetype.cpp")
target_include_directories(stb_truetype PUBLIC "${THIRDPARTY_DIR}/stb_truetype/include")

# gl2d itself lives in thirdparty/gl2d/gl2d
add_subdirectory("${THIRDPARTY_DIR}/gl2d/gl2d")

# gl2d uses strcat/strlen without including <cstring>.
if(MSVC)
	target_compile_options(gl2d PRIVATE /FIcstring)
else()
	target_compile_options(gl2d PRIVATE -include cstring)
endif()


# MY_SOURCES is defined to be a list of all the source files for my game 
# DON'T ADD THE SOURCES BY HAND, they are already added with this macro
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable("${CMAKE_PROJECT_NAME}")

set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 17)

if(PRODUCTION_BUILD)
	# setup the ASSETS_PATH macro to be in the root folder of your exe
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/") 

	# remove the option to debug asserts.
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1) 

else()
	# This is useful to get an ASSETS_PATH in your IDE during development
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0) 

endif()

target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES} )


if(MSVC) # If using the VS compiler...

	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)

	if(PRODUCTION_BUILD)
		#YOU CAN REMOVE THE CONSOLE WITH THIS LINE!
		set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
	endif()

elseif(MINGW) # Windows / MinGW GCC/Clang
	if(PRODUCTION_BUILD)
		# Remove console on Windows under MinGW
		target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE
			-mwindows
		)
	endif()

elseif(APPLE OR UNIX) # macOS / Linux
	if(PRODUCTION_BUILD)
		target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE -s)
	endif()
endif()

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
	glfw
	glad
	glm
	gl2d
	stb_image
	stb_truetype
)
