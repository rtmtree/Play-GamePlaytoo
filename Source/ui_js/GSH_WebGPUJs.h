#pragma once

#include "gs/GSH_WebGPU/GSH_WebGPU.h"
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

class CGSH_WebGPUJs : public CGSH_WebGPU
{
public:
	CGSH_WebGPUJs(WGPUDevice);
	virtual ~CGSH_WebGPUJs() = default;

	static FactoryFunction GetFactoryFunction(WGPUDevice);

	void InitializeImpl() override;
	void ReleaseImpl() override;
	void PresentBackbuffer() override;

private:
	WGPUDevice m_device;
};
