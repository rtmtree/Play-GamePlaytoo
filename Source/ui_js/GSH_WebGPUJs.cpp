#include "GSH_WebGPUJs.h"

CGSH_WebGPUJs::CGSH_WebGPUJs(WGPUDevice device)
    : m_device(device)
{
	this->CGSH_WebGPU::m_device = wgpu::Device::Acquire(device);
	this->CGSH_WebGPU::m_queue = this->CGSH_WebGPU::m_device.GetQueue();
}

CGSH_WebGPU::FactoryFunction CGSH_WebGPUJs::GetFactoryFunction(WGPUDevice device)
{
	return [device]() { return new CGSH_WebGPUJs(device); };
}

void CGSH_WebGPUJs::InitializeImpl()
{
	printf("Initializing WebGPU GS Handler...\r\n");
	m_instance = wgpu::CreateInstance();
	wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
	canvasDesc.selector = "#outputCanvas";
	wgpu::SurfaceDescriptor surfaceDesc = {};
	surfaceDesc.nextInChain = &canvasDesc;
	m_surface = m_instance.CreateSurface(&surfaceDesc);

	m_swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
	printf("WebGPU Format: %d\n", static_cast<int>(m_swapChainFormat));

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
	if(m_surface)
	{
		ConfigureSurface();
	}
}

void CGSH_WebGPUJs::ConfigureSurface()
{
	if(!m_surface) return;

	uint32 width = std::max(1u, m_presentationParams.windowWidth);
	uint32 height = std::max(1u, m_presentationParams.windowHeight);

	printf("Configuring WebGPU surface: %ux%u, format: %d\n", width, height, static_cast<int>(m_swapChainFormat));

	wgpu::SurfaceConfiguration config = {};
	config.device = this->CGSH_WebGPU::m_device;
	config.format = m_swapChainFormat;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;
	config.width = width;
	config.height = height;
	config.presentMode = wgpu::PresentMode::Fifo;

	m_surface.Configure(&config);
}

void CGSH_WebGPUJs::PresentBackbuffer()
{
}
