#pragma once

#include "gs/GSH_WebGPU/GSH_WebGPU.h"
#include <emscripten/html5.h>

class CGSH_WebGPUJs : public CGSH_WebGPU
{
public:
	CGSH_WebGPUJs(wgpu::Device, wgpu::Instance, wgpu::Surface, const std::string&);
	virtual ~CGSH_WebGPUJs() = default;

	static FactoryFunction GetFactoryFunction(wgpu::Device, wgpu::Instance, wgpu::Surface, const std::string&);

	void InitializeImpl() override;
	void ReleaseImpl() override;
	void SetPresentationParams(const PRESENTATION_PARAMS&) override;
	void PresentBackbuffer() override;
	void ConfigureSurface() override;

private:
};
