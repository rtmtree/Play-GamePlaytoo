project(gsh_webgpu)

file(GLOB GSH_WEBGPU_SRCS "*.cpp" "*.h")

add_library(gsh_webgpu STATIC ${GSH_WEBGPU_SRCS})

target_include_directories(gsh_webgpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(gsh_webgpu PUBLIC PlayCore)
