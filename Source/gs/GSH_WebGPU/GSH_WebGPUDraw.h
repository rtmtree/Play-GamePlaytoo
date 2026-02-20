#pragma once

#include "GSH_WebGPUContext.h"
#include "GSH_WebGPUPipelineCache.h"
#include "Convertible.h"
#include "Types.h"
#include <vector>

namespace GSH_WebGPU
{
	class CDraw
	{
	public:
		typedef uint64 PipelineCapsInt;

		struct PIPELINE_CAPS : public convertible<PipelineCapsInt>
		{
			uint32 primitiveType : 2;

			uint32 hasTexture : 1;
			uint32 textureHasAlpha : 1;
			uint32 textureBlackIsTransparent : 1;
			uint32 textureFunction : 2;
			uint32 textureUseLinearFiltering : 1;
			uint32 texClampS : 3;
			uint32 texClampT : 3;
			
			uint32 hasFog : 1;
			uint32 hasAlphaBlending : 1;
			uint32 alphaBlendA : 2;
			uint32 alphaBlendB : 2;
			uint32 alphaBlendC : 2;
			uint32 alphaBlendD : 2;
			
			uint32 hasAlphaTest : 1;
			uint32 alphaTestMethod : 3;
			uint32 alphaTestFail : 2;
			
			uint32 hasDestAlphaTest : 1;
			uint32 destAlphaTestRef : 1;
			
			// WebGPU specific (render pass compatibility)
			uint32 colorFormat : 6;
			uint32 depthFormat : 6;
		};
		static_assert(sizeof(PIPELINE_CAPS) <= sizeof(PipelineCapsInt), "PIPELINE_CAPS too big for PipelineCapsInt");

		struct PRIM_VERTEX
		{
			float x, y;
			uint32 z;
			uint32 color;
			float s, t, q;
			float f;
		};

		CDraw(const ContextPtr& context);
		virtual ~CDraw();

		void SetPipelineCaps(const PIPELINE_CAPS& caps);
		
		// Parameter setters (mirrors Vulkan/OpenGL)
		void SetFramebufferParams(uint32 frameReg, uint32 zbufReg);
		void SetTextureParams(uint64 tex0, uint64 tex1, uint64 texA, uint64 clamp);
		void SetScissor(uint32 rect);
		
		void AddVertices(const PRIM_VERTEX* vertices, size_t count);
		
		void FlushVertices();
		void FlushRenderPass();
		
		void MarkNewFrame();

	private:
		// Internal helpers
		void CreatePipeline(const PIPELINE_CAPS& caps);
		std::string GenerateShader(const PIPELINE_CAPS& caps);
		
		ContextPtr m_context;
		typedef CPipelineCache<PipelineCapsInt> PipelineCache;
		PipelineCache m_pipelineCache;
		
		PIPELINE_CAPS m_currentCaps;
		const PIPELINE* m_currentPipeline = nullptr;
		
		std::vector<PRIM_VERTEX> m_vertexBuffer;
	};

	typedef std::shared_ptr<CDraw> DrawPtr;
}
