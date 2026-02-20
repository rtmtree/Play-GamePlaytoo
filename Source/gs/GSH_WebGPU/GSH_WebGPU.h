#pragma once

#include "../GSHandler.h"
#include "../GsDebuggerInterface.h"
#include "GSH_WebGPUContext.h"
#include "GSH_WebGPUDraw.h"
#include "GSH_WebGPUTransfer.h"

// Forward declare modules
namespace GSH_WebGPU {
	class CDraw;
	class CTransfer;
}

class CGSH_WebGPU : public CGSHandler, public CGsDebuggerInterface
{
public:
	CGSH_WebGPU();
	virtual ~CGSH_WebGPU();

	void ProcessHostToLocalTransfer() override;
	void ProcessLocalToHostTransfer() override;
	void ProcessLocalToLocalTransfer() override;
	void ProcessClutTransfer(uint32, uint32) override;

	Framework::CBitmap GetScreenshot() override;

	void WriteRegisterImpl(uint8, uint64) override;

	//Debugger Interface
	bool GetDepthTestingEnabled() const override;
	void SetDepthTestingEnabled(bool) override;

	bool GetAlphaBlendingEnabled() const override;
	void SetAlphaBlendingEnabled(bool) override;

	bool GetAlphaTestingEnabled() const override;
	void SetAlphaTestingEnabled(bool) override;

	Framework::CBitmap GetFramebuffer(uint64) override;
	Framework::CBitmap GetDepthbuffer(uint64, uint64) override;
	Framework::CBitmap GetTexture(uint64, uint32, uint64, uint64, uint32) override;
	int GetFramebufferScale() override;

	const VERTEX* GetInputVertices() const override;

protected:
	void InitializeImpl() override;
	void ReleaseImpl() override;
	void ResetImpl() override;
	void NotifyPreferencesChangedImpl() override;
	void FlipImpl(const DISPLAY_INFO&) override;

	virtual void PresentBackbuffer() = 0;
	virtual void ConfigureSurface() {}

protected:
	// New architecture modules
	GSH_WebGPU::ContextPtr m_context;
	GSH_WebGPU::DrawPtr m_draw;
	GSH_WebGPU::TransferPtr m_transfer;
	
private:
	// Temporary state to adapt CGSHandler to new modules if needed
	VERTEX m_vtxBuffer[3];
	uint32 m_vtxCount = 0;
	uint32 m_primitiveType = 0;
	PRIM m_primitiveMode;
	
	void Prim_Point();
	void Prim_Line();
	void Prim_Triangle();
	void Prim_Sprite();
};
