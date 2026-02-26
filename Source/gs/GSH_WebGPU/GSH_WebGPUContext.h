#pragma once

#include <webgpu/webgpu_cpp.h>
#include <memory>
#include <array>

namespace GSH_WebGPU
{
	// GS RAM size (4MB)
	static constexpr uint32_t RAMSIZE = 0x400000;
	// CLUT entry count (max 256 entries × 32 cache slots)
	static constexpr uint32_t CLUTENTRYCOUNT = 256;
	static constexpr uint32_t CLUT_CACHE_SIZE = 32;

	struct Context
	{
		wgpu::Instance instance;
		wgpu::Device device;
		wgpu::Queue queue;
		wgpu::Surface surface;
		wgpu::TextureFormat swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;

		// Frame management
		wgpu::CommandEncoder  currentEncoder;
		wgpu::RenderPassEncoder currentRenderPass;
		wgpu::TextureView     currentBackbufferView;
		uint32_t currentSurfaceWidth  = 480;
		uint32_t currentSurfaceHeight = 360;

		// Shared uniform buffers (written every frame via queue.WriteBuffer)
		wgpu::Buffer vertexParamsBuffer;
		wgpu::Buffer fragmentParamsBuffer;

		// GS Memory buffer (4MB, read by draw shaders via storage buffer)
		wgpu::Buffer memoryBuffer;

		// Staging buffer for CPU→GPU GS memory uploads
		wgpu::Buffer memoryStagingBuffer;

		// CLUT buffer (256 entries × CLUT_CACHE_SIZE slots × 4 bytes)
		wgpu::Buffer clutBuffer;

		// Swizzle lookup tables (one per pixel format, stored as R32Uint textures)
		// Indexed: PSMCT32, PSMCT16, PSMCT16S, PSMT8, PSMT4, PSMZ32, PSMZ16, PSMZ16S
		static constexpr int SWIZZLE_TABLE_COUNT = 8;
		std::array<wgpu::Texture, SWIZZLE_TABLE_COUNT>     swizzleTables;
		std::array<wgpu::TextureView, SWIZZLE_TABLE_COUNT> swizzleTableViews;

		// Bind group layout for the swizzle + memory storage resources
		// (shared across draw pipelines that need GS memory access)
		wgpu::BindGroupLayout memoryBindGroupLayout;
		wgpu::BindGroup       memoryBindGroup;

		bool IsValid() const { return device != nullptr; }
	};

	typedef std::shared_ptr<Context> ContextPtr;
}
