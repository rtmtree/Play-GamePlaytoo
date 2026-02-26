#include "GSH_WebGPU.h"
#include "GSH_WebGPU_VulkanBackend.h"
#include "GSH_WebGPU_OpenGLBackend.h"
#include "../GsPixelFormats.h"
#include <cassert>
#include <cstring>
#include <vector>

using namespace GSH_WebGPU;

// ---------------------------------------------------------------------------
// Swizzle table helpers
// ---------------------------------------------------------------------------

// Build a flat array of uint32 page offsets for a given GS pixel format.
// The Vulkan backend fills CImage textures with these offsets; we do the same
// but upload into a wgpu::Texture.
template <typename StorageFormat>
static std::vector<uint32_t> BuildSwizzleOffsets()
{
	auto offsets = CGsPixelFormats::CPixelIndexor<StorageFormat>::GetPageOffsets();
	// GetPageOffsets returns a pointer into a static array of (PAGEWIDTH*PAGEHEIGHT) uint32s
	size_t count = StorageFormat::PAGEWIDTH * StorageFormat::PAGEHEIGHT;
	return std::vector<uint32_t>(offsets, offsets + count);
}

static wgpu::Texture CreateSwizzleTexture(const wgpu::Device& device,
                                          uint32_t width, uint32_t height,
                                          const uint32_t* data)
{
	wgpu::TextureDescriptor texDesc = {};
	texDesc.usage     = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
	texDesc.dimension = wgpu::TextureDimension::e2D;
	texDesc.size      = {width, height, 1};
	texDesc.format    = wgpu::TextureFormat::R32Uint;
	texDesc.mipLevelCount   = 1;
	texDesc.sampleCount     = 1;

	wgpu::Texture tex = device.CreateTexture(&texDesc);

	// Emscripten Dawn exposes WriteTexture via the queue directly
	wgpu::TexelCopyTextureInfo dst = {};
	dst.texture  = tex;
	dst.mipLevel = 0;
	dst.origin   = {0, 0, 0};
	dst.aspect   = wgpu::TextureAspect::All;

	wgpu::TexelCopyBufferLayout layout = {};
	layout.offset       = 0;
	layout.bytesPerRow  = width * sizeof(uint32_t);
	layout.rowsPerImage = height;

	wgpu::Extent3D extent = {width, height, 1};
	device.GetQueue().WriteTexture(&dst, data, width * height * sizeof(uint32_t), &layout, &extent);

	return tex;
}

// ---------------------------------------------------------------------------
// CGSH_WebGPU
// ---------------------------------------------------------------------------

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
	// m_context device/queue/surface/instance are set by subclass (GSH_WebGPUJs)
	// before calling this. Verify.
	assert(m_context->device);

	auto& device = m_context->device;
	auto& queue  = m_context->queue;

	// -----------------------------------------------------------------------
	// GS Memory buffer (4MB Storage+CopySrc+CopyDst)
	// -----------------------------------------------------------------------
	{
		wgpu::BufferDescriptor desc = {};
		desc.label = "GS Memory";
		desc.size  = RAMSIZE;
		desc.usage = wgpu::BufferUsage::Storage
		           | wgpu::BufferUsage::CopyDst
		           | wgpu::BufferUsage::CopySrc;
		m_context->memoryBuffer = device.CreateBuffer(&desc);

		// Zero-initialise up front
		std::vector<uint8_t> zeros(RAMSIZE, 0);
		queue.WriteBuffer(m_context->memoryBuffer, 0, zeros.data(), RAMSIZE);
	}

	// Staging buffer for CPU → GS Memory uploads (mappable host-visible)
	{
		wgpu::BufferDescriptor desc = {};
		desc.label = "GS Memory Staging";
		desc.size  = RAMSIZE;
		desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite;
		m_context->memoryStagingBuffer = device.CreateBuffer(&desc);
	}

	// -----------------------------------------------------------------------
	// CLUT buffer (256 entries × CLUT_CACHE_SIZE × 4 bytes = 32 768 bytes)
	// -----------------------------------------------------------------------
	{
		uint32_t clutSize = CLUTENTRYCOUNT * sizeof(uint32_t) * CLUT_CACHE_SIZE;
		wgpu::BufferDescriptor desc = {};
		desc.label = "CLUT Buffer";
		desc.size  = clutSize;
		desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
		m_context->clutBuffer = device.CreateBuffer(&desc);
	}

	// -----------------------------------------------------------------------
	// Swizzle lookup tables (one R32Uint texture per GS pixel format)
	// -----------------------------------------------------------------------
	{
		auto buildAndCreate = [&](uint32_t pageW, uint32_t pageH, uint32_t* offsets)
			-> std::pair<wgpu::Texture, wgpu::TextureView>
		{
			auto tex = CreateSwizzleTexture(device, pageW, pageH, offsets);
			wgpu::TextureViewDescriptor vd = {};
			vd.format    = wgpu::TextureFormat::R32Uint;
			vd.dimension = wgpu::TextureViewDimension::e2D;
			return {tex, tex.CreateView(&vd)};
		};

		auto [t0, v0] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMCT32::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMCT32::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMCT32>::GetPageOffsets());
		m_context->swizzleTables[0] = t0; m_context->swizzleTableViews[0] = v0;

		auto [t1, v1] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMCT16::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMCT16::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMCT16>::GetPageOffsets());
		m_context->swizzleTables[1] = t1; m_context->swizzleTableViews[1] = v1;

		auto [t2, v2] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMCT16S::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMCT16S::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMCT16S>::GetPageOffsets());
		m_context->swizzleTables[2] = t2; m_context->swizzleTableViews[2] = v2;

		auto [t3, v3] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMT8::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMT8::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMT8>::GetPageOffsets());
		m_context->swizzleTables[3] = t3; m_context->swizzleTableViews[3] = v3;

		auto [t4, v4] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMT4::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMT4::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMT4>::GetPageOffsets());
		m_context->swizzleTables[4] = t4; m_context->swizzleTableViews[4] = v4;

		auto [t5, v5] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMZ32::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMZ32::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMZ32>::GetPageOffsets());
		m_context->swizzleTables[5] = t5; m_context->swizzleTableViews[5] = v5;

		auto [t6, v6] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMZ16::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMZ16::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMZ16>::GetPageOffsets());
		m_context->swizzleTables[6] = t6; m_context->swizzleTableViews[6] = v6;

		auto [t7, v7] = buildAndCreate(
			CGsPixelFormats::STORAGEPSMZ16S::PAGEWIDTH,
			CGsPixelFormats::STORAGEPSMZ16S::PAGEHEIGHT,
			CGsPixelFormats::CPixelIndexor<CGsPixelFormats::STORAGEPSMZ16S>::GetPageOffsets());
		m_context->swizzleTables[7] = t7; m_context->swizzleTableViews[7] = v7;
	}

	// -----------------------------------------------------------------------
	// Shared memory bind group layout + bind group
	// (binding 0 = GS memory storage buffer,
	//  binding 1 = CLUT storage buffer,
	//  bindings 2-9 = swizzle table views)
	// -----------------------------------------------------------------------
	{
		std::vector<wgpu::BindGroupLayoutEntry> entries;

		// binding 0 — GS memory (storage, read-only in shader)
		{
			wgpu::BindGroupLayoutEntry e = {};
			e.binding    = 0;
			e.visibility = wgpu::ShaderStage::Fragment;
			e.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
			entries.push_back(e);
		}
		// binding 1 — CLUT (storage, read-only)
		{
			wgpu::BindGroupLayoutEntry e = {};
			e.binding    = 1;
			e.visibility = wgpu::ShaderStage::Fragment;
			e.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
			entries.push_back(e);
		}
		// bindings 2..9 — swizzle textures
		for(uint32_t i = 0; i < Context::SWIZZLE_TABLE_COUNT; i++)
		{
			wgpu::BindGroupLayoutEntry e = {};
			e.binding    = 2 + i;
			e.visibility = wgpu::ShaderStage::Fragment;
			e.texture.sampleType    = wgpu::TextureSampleType::Uint;
			e.texture.viewDimension = wgpu::TextureViewDimension::e2D;
			entries.push_back(e);
		}

		wgpu::BindGroupLayoutDescriptor bglDesc = {};
		bglDesc.label      = "Memory BGL";
		bglDesc.entryCount = entries.size();
		bglDesc.entries    = entries.data();
		m_context->memoryBindGroupLayout = device.CreateBindGroupLayout(&bglDesc);

		// Build the bind group
		std::vector<wgpu::BindGroupEntry> bgEntries;

		wgpu::BindGroupEntry e0 = {};
		e0.binding = 0;
		e0.buffer  = m_context->memoryBuffer;
		e0.offset  = 0;
		e0.size    = RAMSIZE;
		bgEntries.push_back(e0);

		wgpu::BindGroupEntry e1 = {};
		e1.binding = 1;
		e1.buffer  = m_context->clutBuffer;
		e1.offset  = 0;
		e1.size    = CLUTENTRYCOUNT * sizeof(uint32_t) * CLUT_CACHE_SIZE;
		bgEntries.push_back(e1);

		for(uint32_t i = 0; i < Context::SWIZZLE_TABLE_COUNT; i++)
		{
			wgpu::BindGroupEntry e = {};
			e.binding     = 2 + i;
			e.textureView = m_context->swizzleTableViews[i];
			bgEntries.push_back(e);
		}

		wgpu::BindGroupDescriptor bgDesc = {};
		bgDesc.label      = "Memory BG";
		bgDesc.layout     = m_context->memoryBindGroupLayout;
		bgDesc.entryCount = bgEntries.size();
		bgDesc.entries    = bgEntries.data();
		m_context->memoryBindGroup = device.CreateBindGroup(&bgDesc);
	}

	// -----------------------------------------------------------------------
	// Uniform buffers (vertex + fragment params)
	// -----------------------------------------------------------------------
	if(!m_context->vertexParamsBuffer)
	{
		wgpu::BufferDescriptor desc = {};
		desc.label = "VertexParams";
		desc.size  = 256; // aligned to 256 for uniform buffer offset alignment
		desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->vertexParamsBuffer = device.CreateBuffer(&desc);
	}
	if(!m_context->fragmentParamsBuffer)
	{
		wgpu::BufferDescriptor desc = {};
		desc.label = "FragmentParams";
		desc.size  = 256;
		desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->fragmentParamsBuffer = device.CreateBuffer(&desc);
	}

	// -----------------------------------------------------------------------
	// Create backend (Vulkan-style or OpenGL-style rendering path)
	// -----------------------------------------------------------------------
	if(m_backendName == "opengl")
	{
		m_backend = std::make_shared<COpenGLBackend>();
	}
	else
	{
		// Default: Vulkan-style WebGPU pipeline
		m_backend = std::make_shared<CVulkanBackend>();
	}

	m_backend->Initialize(m_context);

	printf("GSH_WebGPU: Initialized with backend '%s'\n", m_backendName.c_str());
}

void CGSH_WebGPU::ReleaseImpl()
{
	if(m_backend)
	{
		m_backend->Release();
		m_backend.reset();
	}

	// Release context resources
	m_context->memoryBindGroup       = nullptr;
	m_context->memoryBindGroupLayout = nullptr;
	for(auto& t : m_context->swizzleTables)     t = nullptr;
	for(auto& v : m_context->swizzleTableViews) v = nullptr;
	m_context->clutBuffer            = nullptr;
	m_context->memoryStagingBuffer   = nullptr;
	m_context->memoryBuffer          = nullptr;
	m_context->vertexParamsBuffer    = nullptr;
	m_context->fragmentParamsBuffer  = nullptr;
}

void CGSH_WebGPU::ResetImpl()
{
	// Zero out GS memory
	if(m_context->memoryBuffer)
	{
		std::vector<uint8_t> zeros(RAMSIZE, 0);
		m_context->queue.WriteBuffer(m_context->memoryBuffer, 0, zeros.data(), RAMSIZE);
	}
	if(m_backend) m_backend->Reset();
}

void CGSH_WebGPU::NotifyPreferencesChangedImpl()
{
	// Forward to backend if needed (e.g. resolution scale changed)
}

void CGSH_WebGPU::FlipImpl(const DISPLAY_INFO& dispInfo)
{
	// 1. Flush any pending draw calls from the backend
	if(m_backend)
	{
		m_backend->Flip();
	}

	// 2. Let base class handle stat tracking etc.
	CGSHandler::FlipImpl(dispInfo);

	// 3. Present (platform-specific: handled by CGSH_WebGPUJs::PresentBackbuffer)
	PresentBackbuffer();
}

// ---------------------------------------------------------------------------
// Transfer / Memory operations
// ---------------------------------------------------------------------------

void CGSH_WebGPU::ProcessHostToLocalTransfer()
{
	if(m_backend) m_backend->ProcessHostToLocalTransfer();
}

void CGSH_WebGPU::ProcessLocalToHostTransfer()
{
	if(m_backend) m_backend->ProcessLocalToHostTransfer();
}

void CGSH_WebGPU::ProcessLocalToLocalTransfer()
{
	if(m_backend) m_backend->ProcessLocalToLocalTransfer();
}

void CGSH_WebGPU::ProcessClutTransfer(uint32 cbp, uint32 csa)
{
	if(m_backend) m_backend->ProcessClutTransfer(cbp, csa);
}

// ---------------------------------------------------------------------------
// Register write + primitive assembly
// ---------------------------------------------------------------------------

void CGSH_WebGPU::WriteRegisterImpl(uint8 reg, uint64 value)
{
	CGSHandler::WriteRegisterImpl(reg, value);

	if(!m_backend) return;
	m_backend->WriteRegister(reg, value);

	switch(reg)
	{
	case GS_REG_PRIM:
		{
			auto prim     = make_convertible<PRIM>(value);
			m_primitiveType = prim.nType;
			m_vtxCount      = 0;
		}
		break;

	case GS_REG_XYZ2:
	case GS_REG_XYZ3:
	case GS_REG_XYZF2:
	case GS_REG_XYZF3:
		{
			bool isDrawKick = (reg == GS_REG_XYZ2) || (reg == GS_REG_XYZF2);
			bool isFog      = (reg == GS_REG_XYZF2) || (reg == GS_REG_XYZF3);

			if(m_vtxCount < 3)
			{
				auto& v     = m_vtxBuffer[m_vtxCount];
				auto  xyzf  = make_convertible<XYZF>(value);
				auto  xyz   = make_convertible<XYZ>(value);
				v.position  = value;
				v.rgbaq     = m_nReg[GS_REG_RGBAQ];
				v.uv        = m_nReg[GS_REG_UV];
				v.st        = m_nReg[GS_REG_ST];
				v.fog       = isFog ? static_cast<uint8>(xyzf.nF) : 0;
				m_vtxCount++;
			}

			bool endKick = false;
			switch(m_primitiveType)
			{
			case PRIM_POINT:
				if(m_vtxCount == 1) { if(isDrawKick) Prim_Point();    endKick = true; } break;
			case PRIM_LINE:
			case PRIM_LINESTRIP:
				if(m_vtxCount == 2) { if(isDrawKick) Prim_Line();     endKick = true; } break;
			case PRIM_TRIANGLE:
			case PRIM_TRIANGLESTRIP:
			case PRIM_TRIANGLEFAN:
				if(m_vtxCount == 3) { if(isDrawKick) Prim_Triangle(); endKick = true; } break;
			case PRIM_SPRITE:
				if(m_vtxCount == 2) { if(isDrawKick) Prim_Sprite();   endKick = true; } break;
			}

			if(endKick)
			{
				// For strip/fan, keep last vertex(es)
				switch(m_primitiveType)
				{
				case PRIM_LINESTRIP:
					m_vtxBuffer[1] = m_vtxBuffer[0];
					m_vtxCount = 1;
					break;
				case PRIM_TRIANGLESTRIP:
					m_vtxBuffer[2] = m_vtxBuffer[1];
					m_vtxBuffer[1] = m_vtxBuffer[0];
					m_vtxCount = 1;
					break;
				case PRIM_TRIANGLEFAN:
					m_vtxBuffer[1] = m_vtxBuffer[0];
					m_vtxCount = 1;
					break;
				default:
					m_vtxCount = 0;
					break;
				}
			}
		}
		break;
	}
}

// ---------------------------------------------------------------------------
// Primitive assembly helpers — build PRIM_VERTEX and push to backend
// ---------------------------------------------------------------------------

using PV = CDraw::PRIM_VERTEX;

static PV MakeVertex(const CGSHandler::VERTEX& v)
{
	auto xyz = make_convertible<CGSHandler::XYZ>(v.position);
	auto uv  = make_convertible<CGSHandler::UV>(v.uv);
	PV out;
	out.x     = xyz.GetX();
	out.y     = xyz.GetY();
	out.z     = xyz.nZ;
	out.color = static_cast<uint32>(v.rgbaq);
	out.s     = uv.GetU();
	out.t     = uv.GetV();
	out.q     = 1.0f;
	out.f     = static_cast<float>(v.fog);
	return out;
}

void CGSH_WebGPU::Prim_Point()
{
	PV v = MakeVertex(m_vtxBuffer[0]);
	m_backend->DrawVertices(&v, 1, m_primitiveType);
}

void CGSH_WebGPU::Prim_Line()
{
	PV v[2] = { MakeVertex(m_vtxBuffer[0]), MakeVertex(m_vtxBuffer[1]) };
	m_backend->DrawVertices(v, 2, m_primitiveType);
}

void CGSH_WebGPU::Prim_Triangle()
{
	PV v[3] = { MakeVertex(m_vtxBuffer[0]), MakeVertex(m_vtxBuffer[1]), MakeVertex(m_vtxBuffer[2]) };
	m_backend->DrawVertices(v, 3, m_primitiveType);
}

void CGSH_WebGPU::Prim_Sprite()
{
	// Sprites are quad → 2 triangles (6 vertices)
	auto xyz0 = make_convertible<XYZ>(m_vtxBuffer[0].position);
	auto xyz1 = make_convertible<XYZ>(m_vtxBuffer[1].position);
	auto uv0  = make_convertible<UV>(m_vtxBuffer[0].uv);
	auto uv1  = make_convertible<UV>(m_vtxBuffer[1].uv);
	uint32 color = static_cast<uint32>(m_vtxBuffer[1].rgbaq);
	uint32 z     = xyz1.nZ;
	float  fog   = static_cast<float>(m_vtxBuffer[1].fog);

	auto mkV = [&](float x, float y, float s, float t) -> PV {
		PV v; v.x = x; v.y = y; v.z = z; v.color = color;
		v.s = s; v.t = t; v.q = 1.0f; v.f = fog; return v;
	};

	float x0 = xyz0.GetX(), y0 = xyz0.GetY();
	float x1 = xyz1.GetX(), y1 = xyz1.GetY();
	float s0 = uv0.GetU(), t0 = uv0.GetV();
	float s1 = uv1.GetU(), t1 = uv1.GetV();

	PV verts[6] = {
		mkV(x0, y0, s0, t0),
		mkV(x1, y0, s1, t0),
		mkV(x0, y1, s0, t1),
		mkV(x1, y0, s1, t0),
		mkV(x1, y1, s1, t1),
		mkV(x0, y1, s0, t1),
	};
	m_backend->DrawVertices(verts, 6, m_primitiveType);
}

// ---------------------------------------------------------------------------
// Debugger interface stubs
// ---------------------------------------------------------------------------

Framework::CBitmap CGSH_WebGPU::GetScreenshot()              { return Framework::CBitmap(); }
bool CGSH_WebGPU::GetDepthTestingEnabled()  const            { return true; }
void CGSH_WebGPU::SetDepthTestingEnabled(bool)               {}
bool CGSH_WebGPU::GetAlphaBlendingEnabled() const            { return true; }
void CGSH_WebGPU::SetAlphaBlendingEnabled(bool)              {}
bool CGSH_WebGPU::GetAlphaTestingEnabled()  const            { return true; }
void CGSH_WebGPU::SetAlphaTestingEnabled(bool)               {}
Framework::CBitmap CGSH_WebGPU::GetFramebuffer(uint64)       { return Framework::CBitmap(); }
Framework::CBitmap CGSH_WebGPU::GetDepthbuffer(uint64, uint64) { return Framework::CBitmap(); }
Framework::CBitmap CGSH_WebGPU::GetTexture(uint64, uint32, uint64, uint64, uint32) { return Framework::CBitmap(); }
int CGSH_WebGPU::GetFramebufferScale()                       { return 1; }
const CGSHandler::VERTEX* CGSH_WebGPU::GetInputVertices() const { return m_vtxBuffer; }
