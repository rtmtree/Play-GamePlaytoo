#pragma once

#include "GSH_WebGPUContext.h"
#include "Types.h"
#include <vector>

namespace GSH_WebGPU
{
	class CTransfer
	{
	public:
		struct Params
		{
			uint32 bufAddress = 0;
			uint32 bufWidth = 0;
			uint32 rrw = 0;
			uint32 dsax = 0;
			uint32 dsay = 0;
			uint32 xferBufferOffset = 0;
			uint32 pixelCount = 0;
		};

		CTransfer(const ContextPtr& context);
		virtual ~CTransfer();

		void DoTransfer(const std::vector<uint8>& data, const Params& params);
		
		// For Local-to-Local and Local-to-Host transfers
		// Stubs for now
		void DoTransferLocal(const Params& params);
		void DoTransferRead(const Params& params);

	private:
		ContextPtr m_context;
		
		// Resources for transfer
		wgpu::BindGroupLayout m_bindGroupLayout;
		wgpu::PipelineLayout m_pipelineLayout;
		wgpu::RenderPipeline m_pipeline;
		
		void CreatePipeline();
	};

	typedef std::shared_ptr<CTransfer> TransferPtr;
}
