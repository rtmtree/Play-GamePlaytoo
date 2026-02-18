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
	m_instance = wgpu::CreateInstance();
	wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
	canvasDesc.selector = "#outputCanvas";
	wgpu::SurfaceDescriptor surfaceDesc = {};
	surfaceDesc.nextInChain = &canvasDesc;
	m_surface = m_instance.CreateSurface(&surfaceDesc);

	wgpu::SurfaceConfiguration config = {};
	config.device = this->CGSH_WebGPU::m_device;
	config.format = m_swapChainFormat;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;
	config.width = m_presentationParams.windowWidth;
	config.height = m_presentationParams.windowHeight;
	config.presentMode = wgpu::PresentMode::Fifo;

	m_surface.Configure(&config);

	CGSH_WebGPU::InitializeImpl();
}

void CGSH_WebGPUJs::ReleaseImpl()
{
	CGSH_WebGPU::ReleaseImpl();
}

void CGSH_WebGPUJs::PresentBackbuffer()
{
}
