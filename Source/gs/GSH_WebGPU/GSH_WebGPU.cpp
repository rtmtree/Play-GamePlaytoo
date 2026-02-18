#include "GSH_WebGPU.h"
#include <assert.h>
#include <sstream>

std::string CGSH_WebGPU::GenerateShader(const SHADERCAPS& caps)
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
    
    // Unpack color (RGBA8)
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

	if(caps.texSourceMode != 0) // Not NONE
	{
		shader << "    var texCoord = in.texCoord.xy;" << std::endl;
		// Simple wrap/clamp for now
		shader << "    let sampledColor = textureSample(texture0, sampler0, texCoord);" << std::endl;

		// Texture Function (TCC)
		shader << "    if(fParams.texA0 == 0.0) {" << std::endl; // RGB
		shader << "        finalColor = vec4<f32>(sampledColor.rgb * finalColor.rgb, finalColor.a);" << std::endl;
		shader << "    } else {" << std::endl; // RGBA
		shader << "        finalColor = sampledColor * finalColor;" << std::endl;
		shader << "    }" << std::endl;
	}

	if(caps.hasFog)
	{
		shader << "    finalColor = vec4<f32>(mix(finalColor.rgb, fParams.fogColor, in.fog / 255.0), finalColor.a);" << std::endl;
	}

	shader << R"(
    return finalColor;
}
)";
	return shader.str();
}

const unsigned int CGSH_WebGPU::g_shaderClampModes[CGSHandler::CLAMP_MODE_MAX] =
{
	TEXTURE_CLAMP_MODE_STD,
	TEXTURE_CLAMP_MODE_CLAMP,
	TEXTURE_CLAMP_MODE_REGION_CLAMP,
	TEXTURE_CLAMP_MODE_REGION_REPEAT
};

const unsigned int CGSH_WebGPU::g_alphaTestInverse[CGSHandler::ALPHA_TEST_MAX] =
{
	ALPHA_TEST_ALWAYS,
	ALPHA_TEST_NEVER,
	ALPHA_TEST_GEQUAL,
	ALPHA_TEST_GREATER,
	ALPHA_TEST_NOTEQUAL,
	ALPHA_TEST_LESS,
	ALPHA_TEST_LEQUAL,
	ALPHA_TEST_EQUAL
};

CGSH_WebGPU::CGSH_WebGPU()
    : CGSHandler()
{
}

CGSH_WebGPU::~CGSH_WebGPU()
{
}

void CGSH_WebGPU::InitializeImpl()
{
	wgpu::BindGroupLayoutEntry bgEntries[4] = {};
	// Vertex Params
	bgEntries[0].binding = 0;
	bgEntries[0].visibility = wgpu::ShaderStage::Vertex;
	bgEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
	// Fragment Params
	bgEntries[1].binding = 1;
	bgEntries[1].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[1].buffer.type = wgpu::BufferBindingType::Uniform;
	// Sampler
	bgEntries[2].binding = 2;
	bgEntries[2].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[2].sampler.type = wgpu::SamplerBindingType::Filtering;
	// Texture
	bgEntries[3].binding = 3;
	bgEntries[3].visibility = wgpu::ShaderStage::Fragment;
	bgEntries[3].texture.sampleType = wgpu::TextureSampleType::Float;
	bgEntries[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;

	wgpu::BindGroupLayoutDescriptor bglDesc = {};
	bglDesc.entryCount = 4;
	bglDesc.entries = bgEntries;
	m_bindGroupLayout = m_device.CreateBindGroupLayout(&bglDesc);

	wgpu::PipelineLayoutDescriptor plDesc = {};
	plDesc.bindGroupLayoutCount = 1;
	plDesc.bindGroupLayouts = &m_bindGroupLayout;
	m_pipelineLayout = m_device.CreatePipelineLayout(&plDesc);

	wgpu::BufferDescriptor bufferDesc = {};
	bufferDesc.size = (sizeof(VERTEXPARAMS) + 15) & ~15;
	bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
	m_vertexParamsBuffer = m_device.CreateBuffer(&bufferDesc);

	bufferDesc.size = (sizeof(FRAGMENTPARAMS) + 15) & ~15;
	m_fragmentParamsBuffer = m_device.CreateBuffer(&bufferDesc);

	wgpu::SamplerDescriptor samplerDesc = {};
	samplerDesc.magFilter = wgpu::FilterMode::Linear;
	samplerDesc.minFilter = wgpu::FilterMode::Linear;
	m_sampler = m_device.CreateSampler(&samplerDesc);

	memset(&m_vertexParams, 0, sizeof(VERTEXPARAMS));
	m_vertexParams.projMatrix[0] = 1.0f;
	m_vertexParams.projMatrix[5] = 1.0f;
	m_vertexParams.projMatrix[10] = 1.0f;
	m_vertexParams.projMatrix[15] = 1.0f;

	m_vertexParams.texMatrix[0] = 1.0f;
	m_vertexParams.texMatrix[5] = 1.0f;
	m_vertexParams.texMatrix[10] = 1.0f;
	m_vertexParams.texMatrix[15] = 1.0f;

	// Create dummy texture
	wgpu::TextureDescriptor texDesc = {};
	texDesc.size = {1, 1, 1};
	texDesc.format = wgpu::TextureFormat::RGBA8Unorm;
	texDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
	m_dummyTexture.texture = m_device.CreateTexture(&texDesc);
	m_dummyTexture.view = m_dummyTexture.texture.CreateView();
	m_currentTextureView = m_dummyTexture.view;

	uint32 white = 0xFFFFFFFF;
	wgpu::TexelCopyTextureInfo dest = {};
	dest.texture = m_dummyTexture.texture;
	wgpu::TexelCopyBufferLayout layout = {};
	layout.bytesPerRow = 4;
	layout.rowsPerImage = 1;
	m_queue.WriteTexture(&dest, &white, 4, &layout, &texDesc.size);
}

void CGSH_WebGPU::ReleaseImpl()
{
}

void CGSH_WebGPU::ResetImpl()
{
}

void CGSH_WebGPU::NotifyPreferencesChangedImpl()
{
}

wgpu::RenderPipeline CGSH_WebGPU::GetPipelineFromCaps(const SHADERCAPS& caps)
{
	auto it = m_pipelines.find(caps);
	if(it != m_pipelines.end()) return it->second;

	std::string shaderSource = GenerateShader(caps);
	wgpu::ShaderSourceWGSL wgslDesc = {};
	wgslDesc.code = shaderSource.c_str();

	wgpu::ShaderModuleDescriptor smDesc = {};
	smDesc.nextInChain = &wgslDesc;
	wgpu::ShaderModule module = m_device.CreateShaderModule(&smDesc);

	wgpu::ColorTargetState colorTarget = {};
	colorTarget.format = m_swapChainFormat;
	colorTarget.blend = nullptr; // TODO: Implement blending based on caps
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	wgpu::VertexAttribute attributes[5] = {};
	// Position
	attributes[0].shaderLocation = 0;
	attributes[0].format = wgpu::VertexFormat::Float32x2;
	attributes[0].offset = offsetof(PRIM_VERTEX, x);
	// Depth
	attributes[1].shaderLocation = 1;
	attributes[1].format = wgpu::VertexFormat::Uint32;
	attributes[1].offset = offsetof(PRIM_VERTEX, z);
	// Color
	attributes[2].shaderLocation = 2;
	attributes[2].format = wgpu::VertexFormat::Uint32;
	attributes[2].offset = offsetof(PRIM_VERTEX, color);
	// TexCoord
	attributes[3].shaderLocation = 3;
	attributes[3].format = wgpu::VertexFormat::Float32x3;
	attributes[3].offset = offsetof(PRIM_VERTEX, s);
	// Fog
	attributes[4].shaderLocation = 4;
	attributes[4].format = wgpu::VertexFormat::Float32;
	attributes[4].offset = offsetof(PRIM_VERTEX, f);

	wgpu::VertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = sizeof(PRIM_VERTEX);
	vertexBufferLayout.attributeCount = 5;
	vertexBufferLayout.attributes = attributes;

	wgpu::RenderPipelineDescriptor desc = {};
	desc.layout = m_pipelineLayout;

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

	auto pipeline = m_device.CreateRenderPipeline(&desc);
	m_pipelines[caps] = pipeline;
	return pipeline;
}

void CGSH_WebGPU::WriteRegisterImpl(uint8 reg, uint64 value)
{
	CGSHandler::WriteRegisterImpl(reg, value);

	switch(reg)
	{
	case GS_REG_PRIM:
		SetRenderingContext(value);
		ProcessPrim(value);
		break;
	case GS_REG_XYZ2:
	case GS_REG_XYZ3:
		VertexKick(reg, value);
		break;
	case GS_REG_XYZF2:
	case GS_REG_XYZF3:
		VertexKick(reg, value);
		break;
	}
}

void CGSH_WebGPU::SetRenderingContext(uint64 primReg)
{
	auto prim = make_convertible<PRMODE>(primReg);
	unsigned int context = prim.nContext;

	uint64 testReg = m_nReg[GS_REG_TEST_1 + context];
	uint64 frameReg = m_nReg[GS_REG_FRAME_1 + context];
	uint64 alphaReg = m_nReg[GS_REG_ALPHA_1 + context];
	uint64 zbufReg = m_nReg[GS_REG_ZBUF_1 + context];
	uint64 tex0Reg = m_nReg[GS_REG_TEX0_1 + context];
	uint64 tex1Reg = m_nReg[GS_REG_TEX1_1 + context];
	uint64 texAReg = m_nReg[GS_REG_TEXA];
	uint64 clampReg = m_nReg[GS_REG_CLAMP_1 + context];
	uint64 fogColReg = m_nReg[GS_REG_FOGCOL];
	uint64 scissorReg = m_nReg[GS_REG_SCISSOR_1 + context];

	auto shaderCaps = make_convertible<SHADERCAPS>(0);
	FillShaderCapsFromTexture(shaderCaps, tex0Reg, tex1Reg, texAReg, clampReg);
	FillShaderCapsFromTest(shaderCaps, testReg);
	FillShaderCapsFromAlpha(shaderCaps, prim.nAlpha != 0, alphaReg);

	if(prim.nFog)
	{
		shaderCaps.hasFog = 1;
	}

	if(!prim.nTexture)
	{
		shaderCaps.texSourceMode = 0; // TEXTURE_SOURCE_MODE_NONE
	}

	if(m_renderState.isValid)
	{
		if((static_cast<uint32>(m_renderState.shaderCaps) == static_cast<uint32>(shaderCaps)) &&
		   (m_renderState.primReg == primReg) &&
		   (m_renderState.testReg == testReg) &&
		   (m_renderState.zbufReg == zbufReg) &&
		   (m_renderState.frameReg == frameReg) &&
		   (m_renderState.scissorReg == scissorReg) &&
		   (m_renderState.tex0Reg == tex0Reg) &&
		   (m_renderState.tex1Reg == tex1Reg) &&
		   (m_renderState.texAReg == texAReg) &&
		   (m_renderState.clampReg == clampReg) &&
		   (m_renderState.fogColReg == fogColReg))
		{
			auto offset = make_convertible<XYOFFSET>(m_nReg[GS_REG_XYOFFSET_1 + context]);
			m_nPrimOfsX = offset.GetX();
			m_nPrimOfsY = offset.GetY();
			return;
		}
	}

	FlushVertexBuffer();

	m_renderState.isValid = true;
	m_renderState.shaderCaps = shaderCaps;
	m_renderState.primReg = primReg;
	m_renderState.testReg = testReg;
	m_renderState.zbufReg = zbufReg;
	m_renderState.frameReg = frameReg;
	m_renderState.scissorReg = scissorReg;
	m_renderState.tex0Reg = tex0Reg;
	m_renderState.tex1Reg = tex1Reg;
	m_renderState.texAReg = texAReg;
	m_renderState.clampReg = clampReg;
	m_renderState.fogColReg = fogColReg;

	auto offset = make_convertible<XYOFFSET>(m_nReg[GS_REG_XYOFFSET_1 + context]);
	m_nPrimOfsX = offset.GetX();
	m_nPrimOfsY = offset.GetY();

	SetupFramebuffer(frameReg, zbufReg, scissorReg, testReg);
	SetupTexture(primReg, tex0Reg, tex1Reg, texAReg, clampReg);
	SetupTestFunctions(testReg);
	SetupBlendingFunction(alphaReg);
	SetupFogColor(fogColReg);
}

void CGSH_WebGPU::SetupFramebuffer(uint64 frameReg, uint64 zbufReg, uint64 scissorReg, uint64 testReg)
{
	auto frame = make_convertible<FRAME>(frameReg);
	float projWidth = static_cast<float>(frame.GetWidth());
	float projHeight = static_cast<float>(FRAMEBUFFER_HEIGHT);

	m_renderState.viewportWidth = frame.GetWidth();
	m_renderState.viewportHeight = FRAMEBUFFER_HEIGHT;

	MakeLinearZOrtho(m_vertexParams.projMatrix, 0, projWidth, 0, projHeight);
}

void CGSH_WebGPU::SetupTexture(uint64 primReg, uint64 tex0Reg, uint64 tex1Reg, uint64 texAReg, uint64 clampReg)
{
	auto tex0 = make_convertible<TEX0>(tex0Reg);
	auto clamp = make_convertible<CLAMP>(clampReg);

	m_fragmentParams.textureSize[0] = static_cast<float>(tex0.GetWidth());
	m_fragmentParams.textureSize[1] = static_cast<float>(tex0.GetHeight());
	m_fragmentParams.texelSize[0] = 1.0f / m_fragmentParams.textureSize[0];
	m_fragmentParams.texelSize[1] = 1.0f / m_fragmentParams.textureSize[1];

	m_fragmentParams.clampMin[0] = static_cast<float>(clamp.nMINU);
	m_fragmentParams.clampMin[1] = static_cast<float>(clamp.GetMinV());
	m_fragmentParams.clampMax[0] = static_cast<float>(clamp.nMAXU);
	m_fragmentParams.clampMax[1] = static_cast<float>(clamp.nMAXV);

	m_fragmentParams.texA0 = static_cast<float>(tex0.nColorComp);

	auto it = m_textureCache.find(tex0.GetBufPtr());
	if(it != m_textureCache.end())
	{
		m_currentTextureView = it->second.view;
	}
	else
	{
		// TODO: Create/Update texture
		m_currentTextureView = m_dummyTexture.view;
	}
}

void CGSH_WebGPU::SetupTestFunctions(uint64 testReg)
{
	auto test = make_convertible<TEST>(testReg);
	m_fragmentParams.alphaRef = test.nAlphaRef;
}

void CGSH_WebGPU::SetupBlendingFunction(uint64 alphaReg)
{
	auto alpha = make_convertible<ALPHA>(alphaReg);
	m_fragmentParams.alphaFix = static_cast<float>(alpha.nFix) / 255.0f;
}

void CGSH_WebGPU::SetupFogColor(uint64 fogColReg)
{
	auto fogCol = make_convertible<FOGCOL>(fogColReg);
	m_fragmentParams.fogColor[0] = static_cast<float>(fogCol.nFCR) / 255.0f;
	m_fragmentParams.fogColor[1] = static_cast<float>(fogCol.nFCG) / 255.0f;
	m_fragmentParams.fogColor[2] = static_cast<float>(fogCol.nFCB) / 255.0f;
}

void CGSH_WebGPU::MakeLinearZOrtho(float* matrix, float left, float right, float bottom, float top)
{
	memset(matrix, 0, sizeof(float) * 16);
	matrix[0] = 2.0f / (right - left);
	matrix[5] = 2.0f / (top - bottom);
	matrix[10] = 1.0f;
	matrix[12] = -(right + left) / (right - left);
	matrix[13] = -(top + bottom) / (top - bottom);
	matrix[15] = 1.0f;
}

void CGSH_WebGPU::FillShaderCapsFromTexture(SHADERCAPS& shaderCaps, uint64 tex0Reg, uint64 tex1Reg, uint64 texAReg, uint64 clampReg)
{
	auto tex0 = make_convertible<TEX0>(tex0Reg);
	auto tex1 = make_convertible<TEX1>(tex1Reg);
	auto texA = make_convertible<TEXA>(texAReg);
	auto clamp = make_convertible<CLAMP>(clampReg);

	shaderCaps.texSourceMode = 1; // TEXTURE_SOURCE_MODE_STD

	unsigned int clampMode[2];
	clampMode[0] = g_shaderClampModes[clamp.nWMS];
	clampMode[1] = g_shaderClampModes[clamp.nWMT];

	if(clampMode[0] == TEXTURE_CLAMP_MODE_REGION_REPEAT && CanRegionRepeatClampModeSimplified(clamp.nMINU, clamp.nMAXU)) clampMode[0] = TEXTURE_CLAMP_MODE_REGION_REPEAT_SIMPLE;
	if(clampMode[1] == TEXTURE_CLAMP_MODE_REGION_REPEAT && CanRegionRepeatClampModeSimplified(clamp.GetMinV(), clamp.nMAXV)) clampMode[1] = TEXTURE_CLAMP_MODE_REGION_REPEAT_SIMPLE;

	shaderCaps.texClampS = clampMode[0];
	shaderCaps.texClampT = clampMode[1];

	if(CGsPixelFormats::IsPsmIDTEX(tex0.nPsm))
	{
		if((tex1.nMinFilter != MIN_FILTER_NEAREST) || (tex1.nMagFilter != MIN_FILTER_NEAREST))
		{
			shaderCaps.texBilinearFilter = 1;
		}
	}

	if(tex0.nColorComp == 1)
	{
		shaderCaps.texHasAlpha = 1;
	}

	if((tex0.nPsm == PSMCT16) || (tex0.nPsm == PSMCT16S) || (tex0.nPsm == PSMCT24))
	{
		shaderCaps.texUseAlphaExpansion = 1;
	}

	if(CGsPixelFormats::IsPsmIDTEX(tex0.nPsm))
	{
		if((tex0.nCPSM == PSMCT16) || (tex0.nCPSM == PSMCT16S))
		{
			shaderCaps.texUseAlphaExpansion = 1;
		}

		shaderCaps.texSourceMode = CGsPixelFormats::IsPsmIDTEX4(tex0.nPsm) ? 2 : 3; // 2: IDX4, 3: IDX8
	}

	if(texA.nAEM)
	{
		shaderCaps.texBlackIsTransparent = 1;
	}

	shaderCaps.texFunction = tex0.nFunction;
}

void CGSH_WebGPU::FillShaderCapsFromTest(SHADERCAPS& shaderCaps, uint64 testReg)
{
	auto test = make_convertible<TEST>(testReg);
	if(test.nAlphaEnabled)
	{
		shaderCaps.hasAlphaTest = 1;
		shaderCaps.alphaTestMethod = test.nAlphaMethod;
		shaderCaps.alphaFailMethod = test.nAlphaFail;
	}

	if(test.nDestAlphaEnabled)
	{
		shaderCaps.hasDestAlphaTest = 1;
		shaderCaps.destAlphaTestRef = test.nDestAlphaMode;
	}
}

void CGSH_WebGPU::FillShaderCapsFromAlpha(SHADERCAPS& shaderCaps, bool alphaEnabled, uint64 alphaReg)
{
	if(alphaEnabled)
	{
		auto alpha = make_convertible<ALPHA>(alphaReg);
		shaderCaps.hasAlphaBlend = 1;
		shaderCaps.alphaBlendA = alpha.nA;
		shaderCaps.alphaBlendB = alpha.nB;
		shaderCaps.alphaBlendC = alpha.nC;
		shaderCaps.alphaBlendD = alpha.nD;
	}
}

bool CGSH_WebGPU::CanRegionRepeatClampModeSimplified(uint32 clampMin, uint32 clampMax)
{
	uint32 mask = clampMin;
	uint32 val = clampMax;
	return (mask == (val - 1)) && ((val & (val - 1)) == 0);
}

void CGSH_WebGPU::ProcessPrim(uint64 value)
{
	m_primitiveMode = make_convertible<PRIM>(value);
	m_primitiveType = m_primitiveMode.nType;
	m_vtxCount = 0;
}

void CGSH_WebGPU::VertexKick(uint8 reg, uint64 value)
{
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
		if(m_vtxCount == 1)
		{
			Prim_Point();
			endKick = true;
		}
		break;
	case PRIM_LINE:
		if(m_vtxCount == 2)
		{
			Prim_Line();
			endKick = true;
		}
		break;
	case PRIM_TRIANGLE:
		if(m_vtxCount == 3)
		{
			Prim_Triangle();
			endKick = true;
		}
		break;
	case PRIM_SPRITE:
		if(m_vtxCount == 2)
		{
			Prim_Sprite();
			endKick = true;
		}
		break;
	}

	if(endKick) m_vtxCount = 0;
}

void CGSH_WebGPU::Prim_Point()
{
	PRIM_VERTEX v;
	auto xyz = make_convertible<XYZ>(m_vtxBuffer[0].position);
	v.x = xyz.GetX();
	v.y = xyz.GetY();
	v.z = xyz.nZ;
	v.color = static_cast<uint32>(m_vtxBuffer[0].rgbaq);
	v.s = make_convertible<UV>(m_vtxBuffer[0].uv).GetU();
	v.t = make_convertible<UV>(m_vtxBuffer[0].uv).GetV();
	v.q = 1.0f;
	v.f = m_vtxBuffer[0].fog;
	m_vertexBuffer.push_back(v);
}

void CGSH_WebGPU::Prim_Line()
{
	for(int i = 0; i < 2; i++)
	{
		PRIM_VERTEX v;
		auto xyz = make_convertible<XYZ>(m_vtxBuffer[i].position);
		v.x = xyz.GetX();
		v.y = xyz.GetY();
		v.z = xyz.nZ;
		v.color = static_cast<uint32>(m_vtxBuffer[i].rgbaq);
		v.s = make_convertible<UV>(m_vtxBuffer[i].uv).GetU();
		v.t = make_convertible<UV>(m_vtxBuffer[i].uv).GetV();
		v.q = 1.0f;
		v.f = m_vtxBuffer[i].fog;
		m_vertexBuffer.push_back(v);
	}
}

void CGSH_WebGPU::Prim_Triangle()
{
	for(int i = 0; i < 3; i++)
	{
		PRIM_VERTEX v;
		auto xyz = make_convertible<XYZ>(m_vtxBuffer[i].position);
		v.x = xyz.GetX();
		v.y = xyz.GetY();
		v.z = xyz.nZ;
		v.color = static_cast<uint32>(m_vtxBuffer[i].rgbaq);
		v.s = make_convertible<UV>(m_vtxBuffer[i].uv).GetU();
		v.t = make_convertible<UV>(m_vtxBuffer[i].uv).GetV();
		v.q = 1.0f;
		v.f = m_vtxBuffer[i].fog;
		m_vertexBuffer.push_back(v);
	}
}

void CGSH_WebGPU::Prim_Sprite()
{
	auto xyz0 = make_convertible<XYZ>(m_vtxBuffer[0].position);
	auto xyz1 = make_convertible<XYZ>(m_vtxBuffer[1].position);
	auto uv0 = make_convertible<UV>(m_vtxBuffer[0].uv);
	auto uv1 = make_convertible<UV>(m_vtxBuffer[1].uv);

	PRIM_VERTEX v[4];
	for(int i = 0; i < 4; i++)
	{
		v[i].z = xyz1.nZ;
		v[i].color = static_cast<uint32>(m_vtxBuffer[1].rgbaq);
		v[i].q = 1.0f;
		v[i].f = m_vtxBuffer[1].fog;
	}

	v[0].x = xyz0.GetX(); v[0].y = xyz0.GetY(); v[0].s = uv0.GetU(); v[0].t = uv0.GetV();
	v[1].x = xyz1.GetX(); v[1].y = xyz0.GetY(); v[1].s = uv1.GetU(); v[1].t = uv0.GetV();
	v[2].x = xyz0.GetX(); v[2].y = xyz1.GetY(); v[2].s = uv0.GetU(); v[2].t = uv1.GetV();
	v[3].x = xyz1.GetX(); v[3].y = xyz1.GetY(); v[3].s = uv1.GetU(); v[3].t = uv1.GetV();

	m_vertexBuffer.push_back(v[0]);
	m_vertexBuffer.push_back(v[1]);
	m_vertexBuffer.push_back(v[2]);
	m_vertexBuffer.push_back(v[1]);
	m_vertexBuffer.push_back(v[3]);
	m_vertexBuffer.push_back(v[2]);
}

void CGSH_WebGPU::FlushVertexBuffer()
{
	if(m_vertexBuffer.empty()) return;

	if(!m_currentRenderPass)
	{
		m_surface.GetCurrentTexture(&m_currentSurfaceTexture);
		wgpu::RenderPassColorAttachment colorAttachment = {};
		colorAttachment.view = m_currentSurfaceTexture.texture.CreateView();
		colorAttachment.loadOp = wgpu::LoadOp::Clear;
		colorAttachment.storeOp = wgpu::StoreOp::Store;
		colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

		wgpu::RenderPassDescriptor renderPassDesc = {};
		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &colorAttachment;

		m_currentEncoder = m_device.CreateCommandEncoder();
		m_currentRenderPass = m_currentEncoder.BeginRenderPass(&renderPassDesc);
	}

	SHADERCAPS caps = m_renderState.shaderCaps;
	auto pipeline = GetPipelineFromCaps(caps);

	// Upload parameters
	m_queue.WriteBuffer(m_vertexParamsBuffer, 0, &m_vertexParams, sizeof(VERTEXPARAMS));
	m_queue.WriteBuffer(m_fragmentParamsBuffer, 0, &m_fragmentParams, sizeof(FRAGMENTPARAMS));

	// Upload vertex data
	size_t bufferSize = m_vertexBuffer.size() * sizeof(PRIM_VERTEX);
	wgpu::BufferDescriptor bufferDesc = {};
	bufferDesc.size = (bufferSize + 3) & ~3;
	bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
	auto vbo = m_device.CreateBuffer(&bufferDesc);
	m_queue.WriteBuffer(vbo, 0, m_vertexBuffer.data(), bufferSize);

	// Create BindGroup for this draw
	wgpu::BindGroupEntry bgEntries[4] = {};
	bgEntries[0].binding = 0;
	bgEntries[0].buffer = m_vertexParamsBuffer;
	bgEntries[0].size = sizeof(VERTEXPARAMS);

	bgEntries[1].binding = 1;
	bgEntries[1].buffer = m_fragmentParamsBuffer;
	bgEntries[1].size = sizeof(FRAGMENTPARAMS);

	bgEntries[2].binding = 2;
	bgEntries[2].sampler = m_sampler;

	bgEntries[3].binding = 3;
	bgEntries[3].textureView = m_currentTextureView;

	wgpu::BindGroupDescriptor bgDesc = {};
	bgDesc.layout = m_bindGroupLayout;
	bgDesc.entryCount = 4;
	bgDesc.entries = bgEntries;
	auto bindGroup = m_device.CreateBindGroup(&bgDesc);

	m_currentRenderPass.SetPipeline(pipeline);
	m_currentRenderPass.SetBindGroup(0, bindGroup);
	m_currentRenderPass.SetVertexBuffer(0, vbo);
	m_currentRenderPass.Draw(static_cast<uint32>(m_vertexBuffer.size()));

	m_vertexBuffer.clear();
}

void CGSH_WebGPU::FlipImpl(const DISPLAY_INFO& dispInfo)
{
	FlushVertexBuffer();

	if(m_currentRenderPass)
	{
		m_currentRenderPass.End();
		wgpu::CommandBuffer commandBuffer = m_currentEncoder.Finish();
		m_queue.Submit(1, &commandBuffer);
		m_currentRenderPass = nullptr;
		m_currentEncoder = nullptr;
	}

	CGSHandler::FlipImpl(dispInfo);
	PresentBackbuffer();
}

void CGSH_WebGPU::ProcessHostToLocalTransfer()
{
}

void CGSH_WebGPU::ProcessLocalToHostTransfer()
{
}

void CGSH_WebGPU::ProcessLocalToLocalTransfer()
{
}

void CGSH_WebGPU::ProcessClutTransfer(uint32, uint32)
{
}

Framework::CBitmap CGSH_WebGPU::GetScreenshot()
{
	return Framework::CBitmap();
}

bool CGSH_WebGPU::GetDepthTestingEnabled() const
{
	return m_depthTestingEnabled;
}

void CGSH_WebGPU::SetDepthTestingEnabled(bool enabled)
{
	m_depthTestingEnabled = enabled;
}

bool CGSH_WebGPU::GetAlphaBlendingEnabled() const
{
	return m_alphaBlendingEnabled;
}

void CGSH_WebGPU::SetAlphaBlendingEnabled(bool enabled)
{
	m_alphaBlendingEnabled = enabled;
}

bool CGSH_WebGPU::GetAlphaTestingEnabled() const
{
	return m_alphaTestingEnabled;
}

void CGSH_WebGPU::SetAlphaTestingEnabled(bool enabled)
{
	m_alphaTestingEnabled = enabled;
}

Framework::CBitmap CGSH_WebGPU::GetFramebuffer(uint64)
{
	return Framework::CBitmap();
}

Framework::CBitmap CGSH_WebGPU::GetDepthbuffer(uint64, uint64)
{
	return Framework::CBitmap();
}

Framework::CBitmap CGSH_WebGPU::GetTexture(uint64, uint32, uint64, uint64, uint32)
{
	return Framework::CBitmap();
}

int CGSH_WebGPU::GetFramebufferScale()
{
	return 1;
}

const CGSHandler::VERTEX* CGSH_WebGPU::GetInputVertices() const
{
	return nullptr;
}
