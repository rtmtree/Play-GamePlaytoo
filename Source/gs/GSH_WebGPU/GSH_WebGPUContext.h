#pragma once

#include <webgpu/webgpu_cpp.h>
#include <memory>

namespace GSH_WebGPU
{
	struct Context
	{
		wgpu::Instance instance;
		wgpu::Device device;
		wgpu::Queue queue;
		wgpu::Surface surface;
		wgpu::TextureFormat swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;

		// Frame management
		wgpu::CommandEncoder currentEncoder;
		wgpu::RenderPassEncoder currentRenderPass;
		wgpu::SurfaceTexture currentSurfaceTexture;

		// Shared buffers
		wgpu::Buffer vertexParamsBuffer;
		wgpu::Buffer fragmentParamsBuffer;
		
		// Memory buffer (for EE memory emulation)
		wgpu::Buffer memoryBuffer;
		
		bool IsValid() const { return device != nullptr; }
	};

	typedef std::shared_ptr<Context> ContextPtr;
}
