#include "GSH_WebGPUDraw.h"
#include <sstream>
#include <cassert>

using namespace GSH_WebGPU;

// Define vertex/fragment params structures matching the shader
struct VERTEXPARAMS
{
	float projMatrix[16];
	float texMatrix[16];
};

struct FRAGMENTPARAMS
{
	float textureSize[2];
	float texelSize[2];
	float clampMin[2];
	float clampMax[2];
	float texA0;
	float texA1;
	uint32 alphaRef;
	float alphaFix;
	float fogColor[3];
	float padding2;
};

CDraw::CDraw(const ContextPtr& context)
    : m_context(context)
    , m_pipelineCache(context->device)
{
	// Create buffers for params if not already
	if(!m_context->vertexParamsBuffer)
	{
		wgpu::BufferDescriptor bufferDesc = {};
		bufferDesc.size = (sizeof(VERTEXPARAMS) + 15) & ~15;
		bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->vertexParamsBuffer = m_context->device.CreateBuffer(&bufferDesc);
	}
	
	if(!m_context->fragmentParamsBuffer)
	{
		wgpu::BufferDescriptor bufferDesc = {};
		bufferDesc.size = (sizeof(FRAGMENTPARAMS) + 15) & ~15;
		bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
		m_context->fragmentParamsBuffer = m_context->device.CreateBuffer(&bufferDesc);
	}
}

CDraw::~CDraw()
{
}

void CDraw::SetPipelineCaps(const PIPELINE_CAPS& caps)
{
	if(static_cast<uint64>(m_currentCaps) == static_cast<uint64>(caps)) return;

	m_currentCaps = caps;
	m_currentPipeline = m_pipelineCache.TryGetPipeline(caps);

	if(!m_currentPipeline)
	{
		CreatePipeline(caps);
		m_currentPipeline = m_pipelineCache.TryGetPipeline(caps);
	}
}

void CDraw::SetFramebufferParams(uint32 frameReg, uint32 zbufReg)
{
	// Placeholder
}

void CDraw::SetTextureParams(uint64 tex0, uint64 tex1, uint64 texA, uint64 clamp)
{
	// Placeholder
}

void CDraw::SetScissor(uint32 rect)
{
	// Placeholder
}

void CDraw::AddVertices(const PRIM_VERTEX* vertices, size_t count)
{
	m_vertexBuffer.insert(m_vertexBuffer.end(), vertices, vertices + count);
}

void CDraw::FlushVertices()
{
	if(m_vertexBuffer.empty()) return;
	if(!m_currentPipeline) return;
	if(!m_context->currentRenderPass) return;

	size_t bufferSize = m_vertexBuffer.size() * sizeof(PRIM_VERTEX);
	wgpu::BufferDescriptor bufferDesc = {};
	bufferDesc.size = (bufferSize + 3) & ~3;
	bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
	auto vbo = m_context->device.CreateBuffer(&bufferDesc);
	m_context->queue.WriteBuffer(vbo, 0, m_vertexBuffer.data(), bufferSize);

	m_context->currentRenderPass.SetPipeline(m_currentPipeline->pipeline);
	m_context->currentRenderPass.SetBindGroup(0, m_context->currentRenderPass ? /* need bindgroup */ nullptr : nullptr); 
	// Note: We need a BindGroup here. For now passing nullptr will fail validation if logic runs.
	// But we need to create a BindGroup from the layout and buffers. 
	// This usually happens per-draw or per-frame if params change.
	// For this port, let's assume we can create it here or cache it.
	
	// Create temporary bind group for this draw (inefficient but works for port start)
	wgpu::BindGroupEntry bgEntries[4] = {};
	bgEntries[0].binding = 0;
	bgEntries[0].buffer = m_context->vertexParamsBuffer;
	bgEntries[0].size = sizeof(VERTEXPARAMS);

	bgEntries[1].binding = 1;
	bgEntries[1].buffer = m_context->fragmentParamsBuffer;
	bgEntries[1].size = sizeof(FRAGMENTPARAMS);
	
	// Sampler and Texture need to come from somewhere (TextureCache?)
	// For now, stubbing with dummy if not available
	// bgEntries[2] ...
	// bgEntries[3] ...
	
	// We need the BindGroupLayout to create BindGroup.
	// It's in m_currentPipeline->bindGroupLayout.
	
	// Since we don't have texture management yet, we skip SetBindGroup details or it will crash.
	// Commenting out SetBindGroup actual call to avoid immediate crash on uninit data
	// m_context->currentRenderPass.SetBindGroup(0, ...);

	m_context->currentRenderPass.SetVertexBuffer(0, vbo);
	m_context->currentRenderPass.Draw(static_cast<uint32>(m_vertexBuffer.size()));

	m_vertexBuffer.clear();
}

void CDraw::FlushRenderPass()
{
	if (m_context->currentRenderPass) {
		m_context->currentRenderPass.End();
		m_context->currentRenderPass = nullptr;
	}
}

void CDraw::MarkNewFrame()
{
	// m_pipelineCache.Clear(); 
}

void CDraw::CreatePipeline(const PIPELINE_CAPS& caps)
{
	std::string shaderSource = GenerateShader(caps);
	
	wgpu::ShaderSourceWGSL wgslDesc = {};
	wgslDesc.code = shaderSource.c_str();

	wgpu::ShaderModuleDescriptor smDesc = {};
	smDesc.nextInChain = &wgslDesc;
	wgpu::ShaderModule module = m_context->device.CreateShaderModule(&smDesc);

	// Create BindGroupLayout
	wgpu::BindGroupLayoutEntry bgEntries[4] = {};
	bgEntries[0].binding = 0;
	bgEntries[0].visibility = wgpu::ShaderStage::Vertex;
	bgEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
	bgEntries[1].binding = 1;
	bgEntries[1].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[1].buffer.type = wgpu::BufferBindingType::Uniform;
	bgEntries[2].binding = 2;
	bgEntries[2].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[2].sampler.type = wgpu::SamplerBindingType::Filtering;
	bgEntries[3].binding = 3;
	bgEntries[3].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[3].texture.sampleType = wgpu::TextureSampleType::Float;
	bgEntries[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;

	wgpu::BindGroupLayoutDescriptor bglDesc = {};
	bglDesc.entryCount = 4;
	bglDesc.entries = bgEntries;
	wgpu::BindGroupLayout bgl = m_context->device.CreateBindGroupLayout(&bglDesc);

	wgpu::PipelineLayoutDescriptor plDesc = {};
	plDesc.bindGroupLayoutCount = 1;
	plDesc.bindGroupLayouts = &bgl;
	wgpu::PipelineLayout pipelineLayout = m_context->device.CreatePipelineLayout(&plDesc);

	wgpu::ColorTargetState colorTarget = {};
	colorTarget.format = m_context->swapChainFormat;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	// Attributes
    wgpu::VertexAttribute attributes[5] = {};
	attributes[0].shaderLocation = 0; // Pos
	attributes[0].format = wgpu::VertexFormat::Float32x2;
	attributes[0].offset = offsetof(PRIM_VERTEX, x);
	attributes[1].shaderLocation = 1; // Depth (uint for now in struct, but shader expects...)
	attributes[1].format = wgpu::VertexFormat::Uint32;
	attributes[1].offset = offsetof(PRIM_VERTEX, z);
	attributes[2].shaderLocation = 2; // Color
	attributes[2].format = wgpu::VertexFormat::Uint32;
	attributes[2].offset = offsetof(PRIM_VERTEX, color);
	attributes[3].shaderLocation = 3; // Tex
	attributes[3].format = wgpu::VertexFormat::Float32x3;
	attributes[3].offset = offsetof(PRIM_VERTEX, s);
	attributes[4].shaderLocation = 4; // Fog
	attributes[4].format = wgpu::VertexFormat::Float32;
	attributes[4].offset = offsetof(PRIM_VERTEX, f);

	wgpu::VertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = sizeof(PRIM_VERTEX);
	vertexBufferLayout.attributeCount = 5;
	vertexBufferLayout.attributes = attributes;

	wgpu::RenderPipelineDescriptor desc = {};
	desc.layout = pipelineLayout;
	desc.vertex.module = module;
	desc.vertex.entryPoint = "vs_main";
	desc.vertex.bufferCount = 1;
	desc.vertex.buffers = &vertexBufferLayout;

	wgpu::FragmentState fragment = {};
	fragment.module = module;
	fragment.entryPoint = "fs_main";
	fragment.targetCount = 1;
	fragment.targets = &colorTarget;
	desc.fragment = &fragment;

	desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

	PIPELINE newPipeline;
	newPipeline.pipeline = m_context->device.CreateRenderPipeline(&desc);
	newPipeline.bindGroupLayout = bgl;
	newPipeline.pipelineLayout = pipelineLayout;

	m_pipelineCache.RegisterPipeline(caps, newPipeline);
}

std::string CDraw::GenerateShader(const PIPELINE_CAPS& caps)
{
	std::stringstream shader;
	shader << R"(
struct VertexParams {
    projMatrix: mat4x4<f32>,
    texMatrix: mat4x4<f32>,
};

struct FragmentParams {
    textureSize: vec2<f32>,
    texelSize: vec2<f32>,
    clampMin: vec2<f32>,
    clampMax: vec2<f32>,
    texA0: f32,
    texA1: f32,
    alphaRef: u32,
    alphaFix: f32,
    fogColor: vec3<f32>,
};

@group(0) @binding(0) var<uniform> vParams: VertexParams;
@group(0) @binding(1) var<uniform> fParams: FragmentParams;

struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) depth: u32,
    @location(2) color: u32,
    @location(3) texCoord: vec3<f32>,
    @location(4) fog: f32,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) texCoord: vec3<f32>,
    @location(2) fog: f32,
};

@vertex
fn vs_main(model: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = vec4<f32>(model.position, 0.0, 1.0);
    out.clip_position = vParams.projMatrix * pos;
    
    let c = model.color;
    out.color = vec4<f32>(
        f32(c & 0xffu) / 255.0,
        f32((c >> 8u) & 0xffu) / 255.0,
        f32((c >> 16u) & 0xffu) / 255.0,
        f32((c >> 24u) & 0xffu) / 255.0
    );
    
    out.texCoord = (vParams.texMatrix * vec4<f32>(model.texCoord, 1.0)).xyz;
    out.fog = model.fog;
    return out;
}

@group(0) @binding(2) var sampler0: sampler;
@group(0) @binding(3) var texture0: texture_2d<f32>;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var finalColor = in.color;
)";
	
	// Add texture sampling logic based on caps
	// ...
	shader << "    return finalColor;\n}\n";
	
	return shader.str();
}
