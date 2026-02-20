#include "GSH_WebGPU.h"
#include <cassert>

using namespace GSH_WebGPU;

CGSH_WebGPU::CGSH_WebGPU()
    : CGSHandler()
{
	m_context = std::make_shared<Context>();
}

CGSH_WebGPU::~CGSH_WebGPU()
{
}

void CGSH_WebGPU::InitializeImpl()
{
	// m_context members (instance/device/surface) are initialized by subclass (GSH_WebGPUJs)
	// We just create the modules here assuming context is ready or will be.
	
	m_draw = std::make_shared<CDraw>(m_context);
	m_transfer = std::make_shared<CTransfer>(m_context);
}

void CGSH_WebGPU::ReleaseImpl()
{
	m_draw.reset();
	m_transfer.reset();
}

void CGSH_WebGPU::ResetImpl()
{
	if(m_draw) m_draw->FlushVertices();
}

void CGSH_WebGPU::NotifyPreferencesChangedImpl()
{
}

void CGSH_WebGPU::FlipImpl(const DISPLAY_INFO& dispInfo)
{
	if(m_draw) 
	{
		m_draw->FlushVertices();
		m_draw->FlushRenderPass();
		m_draw->MarkNewFrame();
	}

	CGSHandler::FlipImpl(dispInfo);
	PresentBackbuffer();
}

void CGSH_WebGPU::ProcessHostToLocalTransfer()
{
	if(!m_transfer) return;
	// Stub
}

void CGSH_WebGPU::ProcessLocalToHostTransfer() {}
void CGSH_WebGPU::ProcessLocalToLocalTransfer() {}
void CGSH_WebGPU::ProcessClutTransfer(uint32 cbp, uint32 csa) {}

Framework::CBitmap CGSH_WebGPU::GetScreenshot()
{
	return Framework::CBitmap();
}

void CGSH_WebGPU::WriteRegisterImpl(uint8 reg, uint64 value)
{
	CGSHandler::WriteRegisterImpl(reg, value);

	if(!m_draw) return;

	switch(reg)
	{
	case GS_REG_PRIM:
		{
			auto prim = make_convertible<PRIM>(value);
			m_primitiveType = prim.nType;
			m_vtxCount = 0;
			// TODO: Set m_draw->SetPipelineCaps based on PRIM and other regs
		}
		break;
	case GS_REG_XYZ2:
	case GS_REG_XYZ3:
	case GS_REG_XYZF2:
	case GS_REG_XYZF3:
		{
			// Vertex Kick Logic
			bool isXYZF = (reg == GS_REG_XYZF2) || (reg == GS_REG_XYZF3);
			auto xyz = make_convertible<XYZ>(value);
			auto xyzf = make_convertible<XYZF>(value);

			if(m_vtxCount < 3)
			{
				m_vtxBuffer[m_vtxCount].position = value;
				m_vtxBuffer[m_vtxCount].rgbaq = m_nReg[GS_REG_RGBAQ];
				m_vtxBuffer[m_vtxCount].uv = m_nReg[GS_REG_UV];
				m_vtxBuffer[m_vtxCount].st = m_nReg[GS_REG_ST];
				m_vtxBuffer[m_vtxCount].fog = isXYZF ? xyzf.nF : 0;
				m_vtxCount++;
			}
			
			bool endKick = false;
			switch(m_primitiveType)
			{
			case PRIM_POINT:
				if(m_vtxCount == 1) { Prim_Point(); endKick = true; }
				break;
			case PRIM_LINE:
				if(m_vtxCount == 2) { Prim_Line(); endKick = true; }
				break;
			case PRIM_TRIANGLE:
				if(m_vtxCount == 3) { Prim_Triangle(); endKick = true; }
				break;
			case PRIM_SPRITE:
				if(m_vtxCount == 2) { Prim_Sprite(); endKick = true; }
				break;
			}

			if(endKick) m_vtxCount = 0;
		}
		break;
	case GS_REG_SCISSOR_1:
	case GS_REG_SCISSOR_2:
		m_draw->SetScissor(static_cast<uint32>(value));
		break;
	}
}

// Helper to convert accumulated vertex to PRIM_VERTEX and push to CDraw
void CGSH_WebGPU::Prim_Point()
{
	CDraw::PRIM_VERTEX v;
	auto xyz = make_convertible<XYZ>(m_vtxBuffer[0].position);
	v.x = xyz.GetX(); v.y = xyz.GetY(); v.z = xyz.nZ;
	v.color = static_cast<uint32>(m_vtxBuffer[0].rgbaq);
	v.s = make_convertible<UV>(m_vtxBuffer[0].uv).GetU();
	v.t = make_convertible<UV>(m_vtxBuffer[0].uv).GetV();
	v.q = 1.0f; v.f = m_vtxBuffer[0].fog;
	m_draw->AddVertices(&v, 1);
}

void CGSH_WebGPU::Prim_Line()
{
	CDraw::PRIM_VERTEX v[2];
	for(int i=0; i<2; i++) {
		auto xyz = make_convertible<XYZ>(m_vtxBuffer[i].position);
		v[i].x = xyz.GetX(); v[i].y = xyz.GetY(); v[i].z = xyz.nZ;
		v[i].color = static_cast<uint32>(m_vtxBuffer[i].rgbaq);
		v[i].s = make_convertible<UV>(m_vtxBuffer[i].uv).GetU();
		v[i].t = make_convertible<UV>(m_vtxBuffer[i].uv).GetV();
		v[i].q = 1.0f; v[i].f = m_vtxBuffer[i].fog;
	}
	m_draw->AddVertices(v, 2);
}

void CGSH_WebGPU::Prim_Triangle()
{
	CDraw::PRIM_VERTEX v[3];
	for(int i=0; i<3; i++) {
		auto xyz = make_convertible<XYZ>(m_vtxBuffer[i].position);
		v[i].x = xyz.GetX(); v[i].y = xyz.GetY(); v[i].z = xyz.nZ;
		v[i].color = static_cast<uint32>(m_vtxBuffer[i].rgbaq);
		v[i].s = make_convertible<UV>(m_vtxBuffer[i].uv).GetU();
		v[i].t = make_convertible<UV>(m_vtxBuffer[i].uv).GetV();
		v[i].q = 1.0f; v[i].f = m_vtxBuffer[i].fog;
	}
	m_draw->AddVertices(v, 3);
}

void CGSH_WebGPU::Prim_Sprite()
{
	auto xyz0 = make_convertible<XYZ>(m_vtxBuffer[0].position);
	auto xyz1 = make_convertible<XYZ>(m_vtxBuffer[1].position);
	auto uv0 = make_convertible<UV>(m_vtxBuffer[0].uv);
	auto uv1 = make_convertible<UV>(m_vtxBuffer[1].uv);

	CDraw::PRIM_VERTEX v[6];
	// 2 triangles: 0, 1, 2 and 1, 3, 2 (using quad indices 0,1,2,3)
	// v0(x0,y0), v1(x1,y0), v2(x0,y1), v3(x1,y1)
	
	// Helper lambda or macro to set v
	auto setV = [&](int idx, float x, float y, float u, float v_tex) {
		v[idx].x = x; v[idx].y = y; v[idx].z = xyz1.nZ;
		v[idx].color = static_cast<uint32>(m_vtxBuffer[1].rgbaq);
		v[idx].s = u; v[idx].t = v_tex; v[idx].q = 1.0f; v[idx].f = m_vtxBuffer[1].fog;
	};

	setV(0, xyz0.GetX(), xyz0.GetY(), uv0.GetU(), uv0.GetV());
	setV(1, xyz1.GetX(), xyz0.GetY(), uv1.GetU(), uv0.GetV());
	setV(2, xyz0.GetX(), xyz1.GetY(), uv0.GetU(), uv1.GetV());
	
	setV(3, xyz1.GetX(), xyz0.GetY(), uv1.GetU(), uv0.GetV());
	setV(4, xyz1.GetX(), xyz1.GetY(), uv1.GetU(), uv1.GetV());
	setV(5, xyz0.GetX(), xyz1.GetY(), uv0.GetU(), uv1.GetV());

	m_draw->AddVertices(v, 6);
}

// Debugger Interface overrides
bool CGSH_WebGPU::GetDepthTestingEnabled() const { return true; }
void CGSH_WebGPU::SetDepthTestingEnabled(bool v) { }
bool CGSH_WebGPU::GetAlphaBlendingEnabled() const { return true; }
void CGSH_WebGPU::SetAlphaBlendingEnabled(bool v) { }
bool CGSH_WebGPU::GetAlphaTestingEnabled() const { return true; }
void CGSH_WebGPU::SetAlphaTestingEnabled(bool v) { }
Framework::CBitmap CGSH_WebGPU::GetFramebuffer(uint64) { return Framework::CBitmap(); }
Framework::CBitmap CGSH_WebGPU::GetDepthbuffer(uint64, uint64) { return Framework::CBitmap(); }
Framework::CBitmap CGSH_WebGPU::GetTexture(uint64, uint32, uint64, uint64, uint32) { return Framework::CBitmap(); }
int CGSH_WebGPU::GetFramebufferScale() { return 1; }
const CGSHandler::VERTEX* CGSH_WebGPU::GetInputVertices() const { return nullptr; }
