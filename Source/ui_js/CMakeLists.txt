cmake_minimum_required(VERSION 3.18)

set(CMAKE_MODULE_PATH
	${CMAKE_CURRENT_SOURCE_DIR}/../../deps/Dependencies/cmake-modules
	${CMAKE_MODULE_PATH}
)
include(Header)

project(Play)

if(NOT TARGET PlayCore)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../
		${CMAKE_CURRENT_BINARY_DIR}/Source
	)
endif()
list(APPEND UI_JS_PROJECT_LIBS PlayCore)

if(NOT TARGET ui_shared)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../ui_shared
		${CMAKE_CURRENT_BINARY_DIR}/ui_shared
	)
endif()
list(APPEND UI_JS_PROJECT_LIBS ui_shared)

if(NOT TARGET gsh_opengl)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../gs/GSH_OpenGL
		${CMAKE_CURRENT_BINARY_DIR}/gs/GSH_OpenGL
	)
endif()
list(APPEND UI_JS_PROJECT_LIBS gsh_opengl)

if(NOT TARGET gsh_webgpu)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../gs/GSH_WebGPU
		${CMAKE_CURRENT_BINARY_DIR}/gs/GSH_WebGPU
	)
endif()
list(APPEND UI_JS_PROJECT_LIBS gsh_webgpu)

if(NOT TARGET sh_openal)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../sound/SH_OpenAL
		${CMAKE_CURRENT_BINARY_DIR}/SH_OpenAL
	)
endif()
list(APPEND UI_JS_PROJECT_LIBS sh_openal)

add_executable(Play
	GSH_OpenGLJs.cpp
	GSH_OpenGLJs.h
	GSH_WebGPUJs.cpp
	GSH_WebGPUJs.h
	InputProviderEmscripten.cpp
	InputProviderEmscripten.h
	Main.cpp
	Ps2VmJs.cpp
	Ps2VmJs.h
)
# Set output file name to Play900.js and Play900.wasm
set_target_properties(Play PROPERTIES OUTPUT_NAME "Play900")
target_link_libraries(Play PUBLIC ${UI_JS_PROJECT_LIBS})

target_link_options(Play PRIVATE "--bind")
target_link_options(Play PRIVATE "-sMODULARIZE=1")
target_link_options(Play PRIVATE "-sEXPORT_ES6=1")
target_link_options(Play PRIVATE "-sEXPORTED_FUNCTIONS=['_main', '_initVm', '_EmptyBlockHandler', '_MemoryUtils_GetByteProxy', '_MemoryUtils_GetHalfProxy', '_MemoryUtils_GetWordProxy', '_MemoryUtils_GetDoubleProxy', '_MemoryUtils_SetByteProxy', '_MemoryUtils_SetHalfProxy', '_MemoryUtils_SetWordProxy', '_MemoryUtils_SetDoubleProxy', '_LWL_Proxy', '_LWR_Proxy', '_LDL_Proxy', '_LDR_Proxy', '_SWL_Proxy', '_SWR_Proxy', '_SDL_Proxy', '_SDR_Proxy']")
target_link_options(Play PRIVATE "-sEXPORTED_RUNTIME_METHODS=['ccall', 'FS']")
target_link_options(Play PRIVATE "-sFORCE_FILESYSTEM")
target_link_options(Play PRIVATE "-sALLOW_TABLE_GROWTH")
# Fixed memory mode to fix iOS 26.2 shared memory issues with pthreads + growable memory
# See: https://github.com/emscripten-core/emscripten/issues/25905
# With ALLOW_MEMORY_GROWTH=0, memory is fixed at INITIAL_MEMORY size (no growth)
# target_link_options(Play PRIVATE "-sALLOW_MEMORY_GROWTH=1")
target_link_options(Play PRIVATE "-sALLOW_MEMORY_GROWTH=0")

# Testing showed ~300MB (314572800 bytes) is needed for stable operation
# Lower values cause "unwind" errors during initVm due to insufficient memory
target_link_options(Play PRIVATE "-sINITIAL_MEMORY=314572800")
# # 600
# target_link_options(Play PRIVATE "-sINITIAL_MEMORY=629145600")
1000
target_link_options(Play PRIVATE "-sINITIAL_MEMORY=943718400")
# 150
# target_link_options(Play PRIVATE "-sINITIAL_MEMORY=157286400")



#target_link_options(Play PRIVATE "-sASSERTIONS=2")
target_link_options(Play PRIVATE "-sEXPORT_NAME=Play900")
target_link_options(Play PRIVATE "-sPTHREAD_POOL_SIZE=2")
target_link_options(Play PRIVATE "-sWASM_BIGINT")
target_link_options(Play PRIVATE "-sOFFSCREEN_FRAMEBUFFER=1")
target_link_options(Play PRIVATE "-sENVIRONMENT=web,worker")
target_link_options(Play PRIVATE "-sUSE_WEBGL2=1")
target_link_options(Play PRIVATE "--use-port=emdawnwebgpu")
# WebGL performance optimizations
#target_link_options(Play PRIVATE "-sGL_POOL_TEMP_BUFFERS=0")
#target_link_options(Play PRIVATE "-sGL_TRACK_ERRORS=0")
#target_link_options(Play PRIVATE "-sGL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0")

# Preload GameConfig.xml into the virtual filesystem
# get_filename_component(GAMECONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../GameConfig.xml" ABSOLUTE)
# target_link_options(Play PRIVATE "--preload-file=${GAMECONFIG_PATH}@/GameConfig.xml")