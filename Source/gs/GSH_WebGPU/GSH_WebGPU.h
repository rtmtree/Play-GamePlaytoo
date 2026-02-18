#pragma once

#include "../GSHandler.h"
#include "../GsDebuggerInterface.h"
#include "../GsPixelFormats.h"
#include <webgpu/webgpu_cpp.h>

class CGSH_WebGPU : public CGSHandler, public CGsDebuggerInterface
{
public:
	// ... (rest of public methods)
	static const unsigned int g_shaderClampModes[CLAMP_MODE_MAX];
	static const unsigned int g_alphaTestInverse[ALPHA_TEST_MAX];

	enum
	{
		TEXTURE_CLAMP_MODE_STD = 0,
		TEXTURE_CLAMP_MODE_CLAMP = 1,
		TEXTURE_CLAMP_MODE_REGION_CLAMP = 2,
		TEXTURE_CLAMP_MODE_REGION_REPEAT = 3,
		TEXTURE_CLAMP_MODE_REGION_REPEAT_SIMPLE = 4,
	};
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

protected:
	wgpu::RenderPipeline GetPipelineFromCaps(const SHADERCAPS&);
	std::string GenerateShader(const SHADERCAPS&);

	struct SHADERCAPS
	{
		unsigned int texSourceMode : 2; // 0 - None, 1 - Std, 2 - Idx4, 3 - Idx8
		unsigned int hasFog : 1;
		unsigned int hasAlphaTest : 1;
		unsigned int alphaTestMethod : 3;
		unsigned int alphaFailMethod : 2;
		unsigned int hasDestAlphaTest : 1;
		unsigned int destAlphaTestRef : 1;
		unsigned int hasAlphaBlend : 1;
		unsigned int alphaBlendA : 2;
		unsigned int alphaBlendB : 2;
		unsigned int alphaBlendC : 2;
		unsigned int alphaBlendD : 2;
		unsigned int texClampS : 3;
		unsigned int texClampT : 3;
		unsigned int texHasAlpha : 1;
		unsigned int texBlackIsTransparent : 1;
		unsigned int texFunction : 2;
		unsigned int texUseAlphaExpansion : 1;
		unsigned int texBilinearFilter : 1;

		bool operator<(const SHADERCAPS& other) const
		{
			return memcmp(this, &other, sizeof(SHADERCAPS)) < 0;
		}
	};

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

	struct PRIM_VERTEX
	{
		float x, y;
		uint32 z;
		uint32 color;
		float s, t, q;
		float f;
	};

	wgpu::Instance m_instance;
	wgpu::Surface m_surface;
	wgpu::Device m_device;
	wgpu::Queue m_queue;
	wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;

	wgpu::Buffer m_vertexParamsBuffer;
	wgpu::Buffer m_fragmentParamsBuffer;
	wgpu::BindGroupLayout m_bindGroupLayout;
	wgpu::BindGroup m_bindGroup;
	wgpu::PipelineLayout m_pipelineLayout;
	wgpu::Sampler m_sampler;

	wgpu::CommandEncoder m_currentEncoder;
	wgpu::RenderPassEncoder m_currentRenderPass;
	wgpu::SurfaceTexture m_currentSurfaceTexture;

	struct TEXTURE_INFO
	{
		wgpu::Texture texture;
		wgpu::TextureView view;
		uint32 width = 0;
		uint32 height = 0;
	};

	TEXTURE_INFO m_dummyTexture;
	wgpu::TextureView m_currentTextureView;
	std::map<uint32, TEXTURE_INFO> m_textureCache;

	std::map<SHADERCAPS, wgpu::RenderPipeline> m_pipelines;

	struct RENDERSTATE
	{
		bool isValid = false;
		SHADERCAPS shaderCaps;
		uint64 primReg = 0;
		uint64 testReg = 0;
		uint64 frameReg = 0;
		uint64 alphaReg = 0;
		uint64 zbufReg = 0;
		uint64 tex0Reg = 0;
		uint64 tex1Reg = 0;
		uint64 texAReg = 0;
		uint64 clampReg = 0;
		uint64 fogColReg = 0;
		uint64 scissorReg = 0;

		uint32 viewportWidth = 0;
		uint32 viewportHeight = 0;
	};

	RENDERSTATE m_renderState;

	VERTEXPARAMS m_vertexParams;
	FRAGMENTPARAMS m_fragmentParams;

	bool m_depthTestingEnabled = true;
	bool m_alphaBlendingEnabled = true;
	bool m_alphaTestingEnabled = true;

	void SetRenderingContext(uint64);
	void SetupFramebuffer(uint64, uint64, uint64, uint64);
	void SetupTexture(uint64, uint64, uint64, uint64, uint64);
	void SetupTestFunctions(uint64);
	void SetupBlendingFunction(uint64);
	void SetupFogColor(uint64);

	void FillShaderCapsFromTexture(SHADERCAPS&, uint64, uint64, uint64, uint64);
	void FillShaderCapsFromTest(SHADERCAPS&, uint64);
	void FillShaderCapsFromAlpha(SHADERCAPS&, bool, uint64);

	bool CanRegionRepeatClampModeSimplified(uint32, uint32);

	void MakeLinearZOrtho(float*, float, float, float, float);

	int m_fbScale = 1;

	float m_nPrimOfsX = 0;
	float m_nPrimOfsY = 0;

private:
	void ProcessPrim(uint64);
	void VertexKick(uint8, uint64);

	void Prim_Point();
	void Prim_Line();
	void Prim_Triangle();
	void Prim_Sprite();

	void FlushVertexBuffer();

	std::vector<PRIM_VERTEX> m_vertexBuffer;
	VERTEX m_vtxBuffer[3];
	uint32 m_vtxCount = 0;
	bool m_pendingPrim = false;
	uint64 m_pendingPrimValue = 0;
	PRMODE m_primitiveMode;
	unsigned int m_primitiveType = PRIM_INVALID;
};
