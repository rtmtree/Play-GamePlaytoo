#include "GSH_WebGPUJs.h"

CGSH_WebGPUJs::CGSH_WebGPUJs(wgpu::Device device, wgpu::Instance instance, wgpu::Surface surface, const std::string& backend)
{
	// Base class constructor already created m_context (Vulkan style)
	// If backend is "opengl", we might want to do something else or set a flag.
	// For now, let's just initialize the context as before, assuming "vulkan" style is default.
	printf("GSH_WebGPUJs initialized with backend: %s\n", backend.c_str());
	m_context->device = device;
	m_context->instance = instance;
	m_context->queue = m_context->device.GetQueue();
	m_context->surface = surface;
	m_backendName = backend; // Store backend preference
}

CGSH_WebGPU::FactoryFunction CGSH_WebGPUJs::GetFactoryFunction(wgpu::Device device, wgpu::Instance instance, wgpu::Surface surface, const std::string& backend)
{
	return [device, instance, surface, backend]() { return new CGSH_WebGPUJs(device, instance, surface, backend); };
}

void CGSH_WebGPUJs::InitializeImpl()
{
	printf("Initializing WebGPU GS Handler...\r\n");

	m_context->swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
	printf("WebGPU Format: %d\n", static_cast<int>(m_context->swapChainFormat));

	ConfigureSurface();

	CGSH_WebGPU::InitializeImpl();
}

void CGSH_WebGPUJs::ReleaseImpl()
{
	CGSH_WebGPU::ReleaseImpl();
}

void CGSH_WebGPUJs::SetPresentationParams(const PRESENTATION_PARAMS& presentationParams)
{
	CGSH_WebGPU::SetPresentationParams(presentationParams);
	if(m_context->surface)
	{
		ConfigureSurface();
	}
}

void CGSH_WebGPUJs::ConfigureSurface()
{
	if(!m_context->surface) return;

	uint32 width = std::max(1u, m_presentationParams.windowWidth);
	uint32 height = std::max(1u, m_presentationParams.windowHeight);

	printf("Configuring WebGPU surface: %ux%u, format: %d\n", width, height, static_cast<int>(m_context->swapChainFormat));

	wgpu::SurfaceConfiguration config = {};
	config.device = m_context->device;
	config.format = m_context->swapChainFormat;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;
	config.width = width;
	config.height = height;
	config.presentMode = wgpu::PresentMode::Fifo;

	m_context->surface.Configure(&config);
}

void CGSH_WebGPUJs::PresentBackbuffer()
{
	// Presenting is handled by the browser compositor implicitly when using requestAnimationFrame loop
	// but strictly speaking wgpu Surface present happens on 'End' or automatically.
	// In Emscripten using requestAnimationFrame loop, the swap happens at the end of the frame.
}
