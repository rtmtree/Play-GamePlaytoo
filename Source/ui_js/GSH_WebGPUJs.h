#pragma once

#include "gs/GSH_WebGPU/GSH_WebGPU.h"
#include <emscripten/html5.h>

class CGSH_WebGPUJs : public CGSH_WebGPU
{
public:
	CGSH_WebGPUJs(WGPUDevice);
	virtual ~CGSH_WebGPUJs() = default;

	static FactoryFunction GetFactoryFunction(WGPUDevice);

	void InitializeImpl() override;
	void ReleaseImpl() override;
	void SetPresentationParams(const PRESENTATION_PARAMS&) override;
	void PresentBackbuffer() override;
	void ConfigureSurface() override;

private:
	WGPUDevice m_device;
};
