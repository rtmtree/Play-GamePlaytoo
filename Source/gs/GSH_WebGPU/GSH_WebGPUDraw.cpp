#include "GSH_WebGPUDraw.h"
#include <sstream>
#include <cassert>
#include <cstring>

using namespace GSH_WebGPU;

// ---------------------------------------------------------------------------
// Uniform structs (must match WGSL layout)
// ---------------------------------------------------------------------------

struct alignas(16) VERTEXPARAMS
{
	float projMatrix[16]; // column-major mat4x4
};

struct alignas(16) FRAGMENTPARAMS
{
	float    textureSize[2]; // texWidth, texHeight
	float    texelSize[2];   // 1/texWidth, 1/texHeight
	float    clampMin[2];
	float    clampMax[2];
	float    texA0;
	float    texA1;
	uint32_t alphaRef;
	float    alphaFix;
	float    fogColor[3];
	float    padding;
	// Draw params (framebuffer / depth buffer addressing in GS memory)
	uint32_t fbBufAddr;
	uint32_t fbBufWidth;
	uint32_t depthBufAddr;
	uint32_t depthBufWidth;
	// Texture addressing
	uint32_t texBufAddr;
	uint32_t texBufWidth;
	uint32_t texWidth;
	uint32_t texHeight;
	uint32_t texCsa;    // CLUT slot
	uint32_t padding2[3];
};

// ---------------------------------------------------------------------------
// CDraw constructor / destructor
// ---------------------------------------------------------------------------

CDraw::CDraw(const ContextPtr& context)
    : m_context(context)
    , m_pipelineCache(context->device)
{
	// Uniform buffers are already created by GSH_WebGPU::InitializeImpl(),
	// but guard in case we're constructed before that.
	if(!m_context->vertexParamsBuffer)
	{
		wgpu::BufferDescriptor d = {};
		d.label = "VertexParams";
		d.size  = 256;
		d.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->vertexParamsBuffer = m_context->device.CreateBuffer(&d);
	}
	if(!m_context->fragmentParamsBuffer)
	{
		wgpu::BufferDescriptor d = {};
		d.label = "FragmentParams";
		d.size  = 256;
		d.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->fragmentParamsBuffer = m_context->device.CreateBuffer(&d);
	}
}

CDraw::~CDraw()
{
}

// ---------------------------------------------------------------------------
// Pipeline caps / params setters
// ---------------------------------------------------------------------------

void CDraw::SetPipelineCaps(const PIPELINE_CAPS& caps)
{
	if(static_cast<uint64_t>(m_currentCaps) == static_cast<uint64_t>(caps)) return;
	m_currentCaps    = caps;
	m_currentPipeline = m_pipelineCache.TryGetPipeline(caps);
	if(!m_currentPipeline)
	{
		CreatePipeline(caps);
		m_currentPipeline = m_pipelineCache.TryGetPipeline(caps);
	}
}

void CDraw::SetFramebufferParams(uint32 frameReg, uint32 zbufReg)
{
	// Extract FB address/width from FRAME register (bits [31:24] = FBP*64, [24:16] = FBW*64)
	m_fbBufAddr   = (frameReg & 0x000001FF) * 32 * 4; // FBP in 32-bit words, convert to bytes
	m_fbBufWidth  = ((frameReg >> 16) & 0x3F) * 64;
	m_depthBufAddr  = (zbufReg & 0x000001FF) * 32 * 4;
	m_depthBufWidth = m_fbBufWidth; // same buffer width as framebuffer by convention
}

void CDraw::SetTextureParams(uint64 tex0Raw, uint64 /*tex1*/, uint64 /*texA*/, uint64 /*clamp*/)
{
	// Unpack TEX0 (simplified — full format needs GsPixelFormats)
	m_texBufAddr  = (tex0Raw & 0x3FFF) * 32 * 4;
	m_texBufWidth = ((tex0Raw >> 14) & 0x3F) * 64;
	m_texWidth    = 1u << ((tex0Raw >> 26) & 0xF);
	m_texHeight   = 1u << ((tex0Raw >> 30) & 0xF);
	m_texCsa      = static_cast<uint32>((tex0Raw >> 24) & 0x1F); // CLUT slot
}

void CDraw::SetScissor(uint32 rect)
{
	m_scissorX = (rect >>  0) & 0x7FF;
	m_scissorY = (rect >> 16) & 0x7FF;
}

// ---------------------------------------------------------------------------
// Vertex buffering
// ---------------------------------------------------------------------------

void CDraw::AddVertices(const PRIM_VERTEX* vertices, size_t count)
{
	m_vertexBuffer.insert(m_vertexBuffer.end(), vertices, vertices + count);
}

// ---------------------------------------------------------------------------
// FlushVertices — uploads vertices + uniforms and records a draw call
// ---------------------------------------------------------------------------

void CDraw::FlushVertices()
{
	if(m_vertexBuffer.empty())  return;
	if(!m_currentPipeline)      return;
	if(!m_context->currentEncoder) BeginRenderPass();
	if(!m_context->currentEncoder) return; // still not ready

	auto& queue   = m_context->queue;
	auto& encoder = m_context->currentEncoder;

	// ---- Upload vertex data into a transient GPU buffer ----
	size_t vbSize = m_vertexBuffer.size() * sizeof(PRIM_VERTEX);
	// Align to 4 bytes
	size_t vbAligned = (vbSize + 3) & ~3;
	wgpu::BufferDescriptor vbDesc = {};
	vbDesc.label = "VB transient";
	vbDesc.size  = vbAligned;
	vbDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
	auto vbo = m_context->device.CreateBuffer(&vbDesc);
	queue.WriteBuffer(vbo, 0, m_vertexBuffer.data(), vbSize);

	// ---- Upload vertex uniform params ----
	{
		VERTEXPARAMS vp = {};
		// Build orthographic projection for GS coordinate system
		// GS screen space: [0, 4096] × [0, 4096], map to NDC [-1,1]
		// proj[0][0] = 2/w, proj[1][1] = -2/h, proj[3][0] = -1, proj[3][1] = 1
		float w = (m_context->currentSurfaceWidth  > 0) ? (float)m_context->currentSurfaceWidth  : 480.f;
		float h = (m_context->currentSurfaceHeight > 0) ? (float)m_context->currentSurfaceHeight : 360.f;
		// column-major
		float gsW = 4096.f, gsH = 4096.f;
		vp.projMatrix[0]  =  2.f / gsW;
		vp.projMatrix[5]  = -2.f / gsH;
		vp.projMatrix[10] =  1.f;
		vp.projMatrix[12] = -1.f;
		vp.projMatrix[13] =  1.f;
		vp.projMatrix[15] =  1.f;
		(void)w; (void)h;
		queue.WriteBuffer(m_context->vertexParamsBuffer, 0, &vp, sizeof(vp));
	}

	// ---- Upload fragment uniform params ----
	{
		FRAGMENTPARAMS fp = {};
		fp.textureSize[0]  = (float)m_texWidth;
		fp.textureSize[1]  = (float)m_texHeight;
		fp.texelSize[0]    = m_texWidth  > 0 ? 1.f / m_texWidth  : 0;
		fp.texelSize[1]    = m_texHeight > 0 ? 1.f / m_texHeight : 0;
		fp.fogColor[0]     = fp.fogColor[1] = fp.fogColor[2] = 0.f;
		fp.fbBufAddr       = m_fbBufAddr;
		fp.fbBufWidth      = m_fbBufWidth;
		fp.depthBufAddr    = m_depthBufAddr;
		fp.depthBufWidth   = m_depthBufWidth;
		fp.texBufAddr      = m_texBufAddr;
		fp.texBufWidth     = m_texBufWidth;
		fp.texWidth        = m_texWidth;
		fp.texHeight       = m_texHeight;
		fp.texCsa          = m_texCsa;
		queue.WriteBuffer(m_context->fragmentParamsBuffer, 0, &fp, sizeof(fp));
	}

	// ---- Begin render pass if not already active ----
	BeginRenderPass();

	auto& rp = m_context->currentRenderPass;
	if(!rp) return;

	// ---- Build per-draw bind group (group 0 = uniforms) ----
	wgpu::BindGroupEntry bgE[2] = {};
	bgE[0].binding = 0;
	bgE[0].buffer  = m_context->vertexParamsBuffer;
	bgE[0].offset  = 0;
	bgE[0].size    = sizeof(VERTEXPARAMS);
	bgE[1].binding = 1;
	bgE[1].buffer  = m_context->fragmentParamsBuffer;
	bgE[1].offset  = 0;
	bgE[1].size    = sizeof(FRAGMENTPARAMS);

	wgpu::BindGroupDescriptor bgDesc = {};
	bgDesc.layout     = m_currentPipeline->uniformBindGroupLayout;
	bgDesc.entryCount = 2;
	bgDesc.entries    = bgE;
	auto uniformBG = m_context->device.CreateBindGroup(&bgDesc);

	rp.SetPipeline(m_currentPipeline->pipeline);
	rp.SetBindGroup(0, uniformBG);

	// ---- Group 1 = GS memory + CLUT + swizzle tables ----
	if(m_context->memoryBindGroup)
	{
		rp.SetBindGroup(1, m_context->memoryBindGroup);
	}

	rp.SetVertexBuffer(0, vbo);
	rp.Draw(static_cast<uint32_t>(m_vertexBuffer.size()));

	m_vertexBuffer.clear();
}

// ---------------------------------------------------------------------------
// Render pass lifetime
// ---------------------------------------------------------------------------

void CDraw::BeginRenderPass()
{
	if(m_context->currentRenderPass) return;
	if(!m_context->currentEncoder)
	{
		// Start a new command encoder for this frame
		wgpu::CommandEncoderDescriptor encDesc = {};
		encDesc.label = "Frame encoder";
		m_context->currentEncoder = m_context->device.CreateCommandEncoder(&encDesc);
	}

	// Acquire current surface texture
	wgpu::SurfaceTexture st = {};
	m_context->surface.GetCurrentTexture(&st);
	if(!st.texture || st.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal)
	{
		// Surface not ready (e.g. resizing) — skip
		m_context->currentEncoder = nullptr;
		return;
	}

	wgpu::TextureViewDescriptor tvd = {};
	tvd.format          = m_context->swapChainFormat;
	tvd.dimension       = wgpu::TextureViewDimension::e2D;
	tvd.mipLevelCount   = 1;
	tvd.arrayLayerCount = 1;
	auto backbuffer = st.texture.CreateView(&tvd);
	m_context->currentBackbufferView = backbuffer;

	wgpu::RenderPassColorAttachment colorAttach = {};
	colorAttach.view       = backbuffer;
	colorAttach.loadOp     = wgpu::LoadOp::Clear;
	colorAttach.storeOp    = wgpu::StoreOp::Store;
	colorAttach.clearValue = {0.0, 0.0, 0.0, 1.0};

	wgpu::RenderPassDescriptor rpDesc = {};
	rpDesc.label                  = "GS Draw Pass";
	rpDesc.colorAttachmentCount   = 1;
	rpDesc.colorAttachments       = &colorAttach;

	m_context->currentRenderPass = m_context->currentEncoder.BeginRenderPass(&rpDesc);
}

void CDraw::FlushRenderPass()
{
	if(m_context->currentRenderPass)
	{
		m_context->currentRenderPass.End();
		m_context->currentRenderPass = nullptr;
	}
}

void CDraw::MarkNewFrame()
{
	// Called at the end of each frame (after Present)
	// The encoder was already submitted in PresentBackbuffer (GSH_WebGPUJs)
	m_context->currentEncoder = nullptr;
}

// ---------------------------------------------------------------------------
// Pipeline creation
// ---------------------------------------------------------------------------

void CDraw::CreatePipeline(const PIPELINE_CAPS& caps)
{
	std::string shaderSrc = GenerateShader(caps);

	wgpu::ShaderSourceWGSL wgslSrc = {};
	wgslSrc.code = shaderSrc.c_str();

	wgpu::ShaderModuleDescriptor smDesc = {};
	smDesc.nextInChain = &wgslSrc;
	wgpu::ShaderModule module = m_context->device.CreateShaderModule(&smDesc);

	// ---- Group 0: Uniform buffers (vertex params + fragment params) ----
	wgpu::BindGroupLayoutEntry uniEntries[2] = {};
	uniEntries[0].binding    = 0;
	uniEntries[0].visibility = wgpu::ShaderStage::Vertex;
	uniEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
	uniEntries[1].binding    = 1;
	uniEntries[1].visibility = wgpu::ShaderStage::Fragment;
	uniEntries[1].buffer.type = wgpu::BufferBindingType::Uniform;

	wgpu::BindGroupLayoutDescriptor bgl0Desc = {};
	bgl0Desc.label      = "Uniforms BGL";
	bgl0Desc.entryCount = 2;
	bgl0Desc.entries    = uniEntries;
	wgpu::BindGroupLayout bgl0 = m_context->device.CreateBindGroupLayout(&bgl0Desc);

	// ---- Group 1: GS memory resources (from context shared layout) ----
	wgpu::BindGroupLayout bgl1 = m_context->memoryBindGroupLayout;

	wgpu::BindGroupLayout layouts[2] = {bgl0, bgl1};
	wgpu::PipelineLayoutDescriptor plDesc = {};
	plDesc.label                = "Draw Pipeline Layout";
	plDesc.bindGroupLayoutCount = (bgl1 ? 2 : 1);
	plDesc.bindGroupLayouts     = layouts;
	wgpu::PipelineLayout pipelineLayout = m_context->device.CreatePipelineLayout(&plDesc);

	// ---- Vertex buffer layout ----
	wgpu::VertexAttribute attribs[5] = {};
	attribs[0].shaderLocation = 0; attribs[0].format = wgpu::VertexFormat::Float32x2; attribs[0].offset = offsetof(PRIM_VERTEX, x);
	attribs[1].shaderLocation = 1; attribs[1].format = wgpu::VertexFormat::Uint32;     attribs[1].offset = offsetof(PRIM_VERTEX, z);
	attribs[2].shaderLocation = 2; attribs[2].format = wgpu::VertexFormat::Uint32;     attribs[2].offset = offsetof(PRIM_VERTEX, color);
	attribs[3].shaderLocation = 3; attribs[3].format = wgpu::VertexFormat::Float32x3;  attribs[3].offset = offsetof(PRIM_VERTEX, s);
	attribs[4].shaderLocation = 4; attribs[4].format = wgpu::VertexFormat::Float32;    attribs[4].offset = offsetof(PRIM_VERTEX, f);

	wgpu::VertexBufferLayout vbLayout = {};
	vbLayout.arrayStride    = sizeof(PRIM_VERTEX);
	vbLayout.stepMode       = wgpu::VertexStepMode::Vertex;
	vbLayout.attributeCount = 5;
	vbLayout.attributes     = attribs;

	// ---- Blend state ----
	wgpu::BlendState blend = {};
	if(caps.hasAlphaBlending)
	{
		blend.color.operation = wgpu::BlendOperation::Add;
		blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
		blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
		blend.alpha.operation = wgpu::BlendOperation::Add;
		blend.alpha.srcFactor = wgpu::BlendFactor::One;
		blend.alpha.dstFactor = wgpu::BlendFactor::Zero;
	}

	wgpu::ColorTargetState colorTarget = {};
	colorTarget.format    = m_context->swapChainFormat;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;
	if(caps.hasAlphaBlending)
		colorTarget.blend = &blend;

	wgpu::FragmentState fragState = {};
	fragState.module      = module;
	fragState.entryPoint  = "fs_main";
	fragState.targetCount = 1;
	fragState.targets     = &colorTarget;

	// ---- Primitive topology ----
	wgpu::PrimitiveTopology topology = wgpu::PrimitiveTopology::TriangleList;
	if(caps.primitiveType == 1) topology = wgpu::PrimitiveTopology::LineList;
	if(caps.primitiveType == 2) topology = wgpu::PrimitiveTopology::PointList;

	wgpu::RenderPipelineDescriptor rpDesc = {};
	rpDesc.layout                = pipelineLayout;
	rpDesc.vertex.module         = module;
	rpDesc.vertex.entryPoint     = "vs_main";
	rpDesc.vertex.bufferCount    = 1;
	rpDesc.vertex.buffers        = &vbLayout;
	rpDesc.primitive.topology    = topology;
	rpDesc.primitive.cullMode    = wgpu::CullMode::None;
	rpDesc.fragment              = &fragState;

	PIPELINE newPipeline;
	newPipeline.pipeline             = m_context->device.CreateRenderPipeline(&rpDesc);
	newPipeline.uniformBindGroupLayout = bgl0;
	newPipeline.pipelineLayout       = pipelineLayout;
	newPipeline.bindGroupLayout      = bgl0; // keep for compat

	m_pipelineCache.RegisterPipeline(caps, newPipeline);
}

// ---------------------------------------------------------------------------
// Shader generation (WGSL)
// ---------------------------------------------------------------------------
// Generates a complete WGSL shader for the given PIPELINE_CAPS.
// Group 0: uniforms (vertex + fragment params)
// Group 1: GS memory storage buffer + CLUT + swizzle tables

std::string CDraw::GenerateShader(const PIPELINE_CAPS& caps)
{
	std::stringstream s;

	// ---------- Uniform structs ----------
	s << R"(
struct VertexParams {
    projMatrix : mat4x4<f32>,
};

struct FragmentParams {
    textureSize  : vec2<f32>,
    texelSize    : vec2<f32>,
    clampMin     : vec2<f32>,
    clampMax     : vec2<f32>,
    texA0        : f32,
    texA1        : f32,
    alphaRef     : u32,
    alphaFix     : f32,
    fogColor     : vec3<f32>,
    _pad         : f32,
    fbBufAddr    : u32,
    fbBufWidth   : u32,
    depthBufAddr : u32,
    depthBufWidth: u32,
    texBufAddr   : u32,
    texBufWidth  : u32,
    texWidth     : u32,
    texHeight    : u32,
    texCsa       : u32,
    _pad2        : vec3<u32>,
};

@group(0) @binding(0) var<uniform> vParams   : VertexParams;
@group(0) @binding(1) var<uniform> fParams   : FragmentParams;

// GS memory storage buffer (4MB as array of u32)
@group(1) @binding(0) var<storage, read> gsMemory   : array<u32>;
// CLUT buffer
@group(1) @binding(1) var<storage, read> clutBuffer : array<u32>;
// Swizzle tables (indexed by pixel format)
@group(1) @binding(2) var swizzlePSMCT32  : texture_2d<u32>;
@group(1) @binding(3) var swizzlePSMCT16  : texture_2d<u32>;
@group(1) @binding(4) var swizzlePSMCT16S : texture_2d<u32>;
@group(1) @binding(5) var swizzlePSMT8    : texture_2d<u32>;
@group(1) @binding(6) var swizzlePSMT4    : texture_2d<u32>;
@group(1) @binding(7) var swizzlePSMZ32   : texture_2d<u32>;
@group(1) @binding(8) var swizzlePSMZ16   : texture_2d<u32>;
@group(1) @binding(9) var swizzlePSMZ16S  : texture_2d<u32>;

)";

	// ---------- Vertex IO ----------
	s << R"(
struct VertexInput {
    @location(0) position : vec2<f32>,
    @location(1) depth    : u32,
    @location(2) color    : u32,
    @location(3) texCoord : vec3<f32>,
    @location(4) fog      : f32,
};

struct VertexOutput {
    @builtin(position) clipPos  : vec4<f32>,
    @location(0)       color    : vec4<f32>,
    @location(1)       texCoord : vec3<f32>,
    @location(2)       fog      : f32,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos4 = vec4<f32>(in.position, 0.0, 1.0);
    out.clipPos = vParams.projMatrix * pos4;

    // Unpack RGBAQ — stored as ABGR u32
    let c = in.color;
    out.color = vec4<f32>(
        f32( c        & 0xffu) / 128.0,   // R  (GS uses 0-128 = 0-1)
        f32((c >>  8u) & 0xffu) / 128.0,   // G
        f32((c >> 16u) & 0xffu) / 128.0,   // B
        f32((c >> 24u) & 0xffu) / 128.0    // A
    );
    out.texCoord = in.texCoord;
    out.fog      = in.fog;
    return out;
}

)";

	// ---------- GS memory read helpers ----------
	s << R"(
// Read a 32-bit word from GS memory at byte address
fn readGsMem32(addr: u32) -> u32 {
    return gsMemory[addr >> 2u];
}

// Read a 16-bit value (lo or hi halfword)
fn readGsMem16(addr: u32) -> u32 {
    let word = gsMemory[addr >> 2u];
    let shift = (addr & 2u) * 8u;
    return (word >> shift) & 0xffffu;
}

// Read an 8-bit value
fn readGsMem8(addr: u32) -> u32 {
    let word = gsMemory[addr >> 2u];
    let shift = (addr & 3u) * 8u;
    return (word >> shift) & 0xffu;
}

)";

	// ---------- Fragment shader ----------
	s << R"(
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var finalColor = in.color;
)";

	// Texture sampling via GS memory (PSMCT32 path — expands to other formats later)
	if(caps.hasTexture)
	{
		s << R"(
    // --- Texture fetch from GS memory (PSMCT32) ---
    let texW = f32(fParams.texWidth);
    let texH = f32(fParams.texHeight);
    var st = in.texCoord.xy;
    if(in.texCoord.z > 0.0) { st = st / in.texCoord.z; } // perspective divide for STQ

    // Clamp / wrap (simple clamp for now)
    st = clamp(st, vec2<f32>(0.0), vec2<f32>(1.0));

    let px = u32(st.x * texW);
    let py = u32(st.y * texH);

    // Page-based address (PSMCT32: 64x32 pixels per page)
    let pageW : u32 = 64u;
    let pageH : u32 = 32u;
    let pageX = px / pageW;
    let pageY = py / pageH;
    let localX = px % pageW;
    let localY = py % pageH;
    let bufWidth = fParams.texBufWidth / pageW;     // buffer width in pages
    let pageIndex = pageY * bufWidth + pageX;
    let swizzleOffset = textureLoad(swizzlePSMCT32, vec2<i32>(i32(localX), i32(localY)), 0).r;
    let blockAddr = fParams.texBufAddr + pageIndex * (pageW * pageH * 4u) + swizzleOffset * 4u;

    let texelU32 = readGsMem32(blockAddr);
    // PSMCT32 → RGBA8 (stored as ABGR)
    let texColor = vec4<f32>(
        f32( texelU32        & 0xffu) / 255.0,
        f32((texelU32 >>  8u) & 0xffu) / 255.0,
        f32((texelU32 >> 16u) & 0xffu) / 255.0,
        f32((texelU32 >> 24u) & 0xffu) / 255.0
    );
)";
		// Texture function (MODULATE / DECAL / HIGHLIGHT / HIGHLIGHT2)
		if(caps.textureFunction == 0) // MODULATE
			s << "    finalColor = finalColor * texColor;\n";
		else if(caps.textureFunction == 1) // DECAL
			s << "    finalColor = texColor;\n";
		else // HIGHLIGHT / HIGHLIGHT2
			s << "    finalColor = vec4<f32>(in.color.rgb + texColor.rgb, in.color.a * texColor.a);\n";
	}

	// Alpha test
	if(caps.hasAlphaTest)
	{
		s << R"(
    // Alpha test (GREATER method as default)
    if(finalColor.a * 128.0 <= f32(fParams.alphaRef)) { discard; }
)";
	}

	// Fog
	if(caps.hasFog)
	{
		s << R"(
    let fogFactor = clamp(in.fog / 255.0, 0.0, 1.0);
    finalColor = vec4<f32>(mix(fParams.fogColor, finalColor.rgb, fogFactor), finalColor.a);
)";
	}

	s << "    return finalColor;\n}\n";
	return s.str();
}
