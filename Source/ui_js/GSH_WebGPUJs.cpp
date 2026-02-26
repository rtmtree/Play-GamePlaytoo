#include "GSH_WebGPUJs.h"
#include "gs/GSH_WebGPU/GSH_WebGPUDraw.h"

CGSH_WebGPUJs::CGSH_WebGPUJs(wgpu::Device device, wgpu::Instance instance, wgpu::Surface surface, const std::string& backend)
{
	printf("GSH_WebGPUJs: initializing with backend '%s'\n", backend.c_str());
	m_context->device   = device;
	m_context->instance = instance;
	m_context->queue    = m_context->device.GetQueue();
	m_context->surface  = surface;
	m_backendName       = backend;
}

CGSH_WebGPU::FactoryFunction CGSH_WebGPUJs::GetFactoryFunction(wgpu::Device device, wgpu::Instance instance, wgpu::Surface surface, const std::string& backend)
{
	return [device, instance, surface, backend]() {
		return new CGSH_WebGPUJs(device, instance, surface, backend);
	};
}

void CGSH_WebGPUJs::InitializeImpl()
{
	printf("GSH_WebGPUJs: InitializeImpl\n");

	// Set surface format
	m_context->swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;

	// Configure the surface before calling base (which creates the pipelines)
	ConfigureSurface();

	// Base class sets up GS memory, CLUT, swizzle tables, backend
	CGSH_WebGPU::InitializeImpl();
}

void CGSH_WebGPUJs::ReleaseImpl()
{
	CGSH_WebGPU::ReleaseImpl();
}

void CGSH_WebGPUJs::SetPresentationParams(const PRESENTATION_PARAMS& presentationParams)
{
	CGSH_WebGPU::SetPresentationParams(presentationParams);

	// Keep context dimensions in sync so the shader can build the right projection
	m_context->currentSurfaceWidth  = presentationParams.windowWidth  > 0 ? presentationParams.windowWidth  : 480;
	m_context->currentSurfaceHeight = presentationParams.windowHeight > 0 ? presentationParams.windowHeight : 360;

	if(m_context->surface)
	{
		ConfigureSurface();
	}
}

void CGSH_WebGPUJs::ConfigureSurface()
{
	if(!m_context->surface) return;

	uint32_t width  = std::max(1u, m_context->currentSurfaceWidth);
	uint32_t height = std::max(1u, m_context->currentSurfaceHeight);

	printf("GSH_WebGPUJs: ConfigureSurface %ux%u fmt=%d\n",
	       width, height, static_cast<int>(m_context->swapChainFormat));

	wgpu::SurfaceConfiguration config = {};
	config.device      = m_context->device;
	config.format      = m_context->swapChainFormat;
	config.usage       = wgpu::TextureUsage::RenderAttachment;
	config.alphaMode   = wgpu::CompositeAlphaMode::Auto;
	config.width       = width;
	config.height      = height;
	config.presentMode = wgpu::PresentMode::Fifo;

	m_context->surface.Configure(&config);
}

void CGSH_WebGPUJs::PresentBackbuffer()
{
	// End any open render pass
	if(m_context->currentRenderPass)
	{
		m_context->currentRenderPass.End();
		m_context->currentRenderPass = nullptr;
	}

	// Submit command buffer
	if(m_context->currentEncoder)
	{
		wgpu::CommandBufferDescriptor cbDesc = {};
		cbDesc.label = "Frame CB";
		wgpu::CommandBuffer cb = m_context->currentEncoder.Finish(&cbDesc);
		m_context->queue.Submit(1, &cb);
		m_context->currentEncoder = nullptr;
	}

	// Present — browser compositor picks up the surface texture automatically
	// when using requestAnimationFrame; call Present() explicitly for Dawn/Emscripten.
	m_context->surface.Present();

	// Notify CDraw that the frame is done
	if(m_backend)
	{
		// Cast is safe — CVulkanBackend holds a shared DrawPtr internally
		// We rely on MarkNewFrame propagating through the backend
	}
}
