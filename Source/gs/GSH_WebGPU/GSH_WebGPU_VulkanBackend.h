#pragma once

#include "GSH_WebGPUBackend.h"
#include "GSH_WebGPUDraw.h"
#include "GSH_WebGPUTransfer.h"

namespace GSH_WebGPU
{
	class CVulkanBackend : public CBackend
	{
	public:
		CVulkanBackend() = default;
		virtual ~CVulkanBackend() = default;

		void Initialize(const ContextPtr& context) override 
		{ 
			m_context = context;
			m_draw = std::make_shared<CDraw>(context);
			m_transfer = std::make_shared<CTransfer>(context);
		}
		
		void Release() override 
        { 
            m_draw.reset(); 
            m_transfer.reset(); 
        }
        
		void Reset() override 
        { 
            if(m_draw) m_draw->FlushVertices(); 
        }
        
		void Flip() override 
        {
            if(m_draw) 
            {
                m_draw->FlushVertices();
                m_draw->FlushRenderPass();
                m_draw->MarkNewFrame();
            }
        }

		void WriteRegister(uint8 reg, uint64 value) override; // To be implemented in .cpp
		
		void ProcessHostToLocalTransfer() override 
		{
			if(m_transfer) 
			{
				// Forward to transfer logic (TODO: pass memory data)
			}
		}
		
		void ProcessLocalToHostTransfer() override {}
		void ProcessLocalToLocalTransfer() override {}
		void ProcessClutTransfer(uint32 cbp, uint32 csa) override {}

		void DrawVertices(const void* vertices, size_t count, uint32 primitiveType) override
		{
			if(m_draw)
			{
				// PRIM_VERTEX structure is specific to CDraw.
				// We assume the caller (CGSH_WebGPU) provides PRIM_VERTEX array.
				// This implies coupling between Handler and modular Draw implementation...
				// But Handler assembles PRIM_VERTEX based on registers.
				m_draw->AddVertices(static_cast<const CDraw::PRIM_VERTEX*>(vertices), count);
			}
		}

	private:
		ContextPtr m_context;
		DrawPtr m_draw;
		TransferPtr m_transfer;
	};
}
