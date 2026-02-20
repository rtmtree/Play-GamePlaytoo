#pragma once

#include "GSH_WebGPUBackend.h"

namespace GSH_WebGPU
{
	class COpenGLBackend : public CBackend
	{
	public:
		COpenGLBackend() = default;
		virtual ~COpenGLBackend() = default;

		void Initialize(const ContextPtr& context) override 
        { 
            m_context = context;
            // No custom modules yet, monolithic style logic would go here
        }
		
        void Release() override {}
		void Reset() override {}
		void Flip() override {}

		void WriteRegister(uint8 reg, uint64 value) override 
        {
            // OpenGL-style implementation (monolithic state machine)
            // Stubs for now
        }
        
        void DrawVertices(const void* vertices, size_t count, uint32 primitiveType) override {}
		
		void ProcessHostToLocalTransfer() override {}
		void ProcessLocalToHostTransfer() override {}
		void ProcessLocalToLocalTransfer() override {}
		void ProcessClutTransfer(uint32 cbp, uint32 csa) override {}

	private:
		ContextPtr m_context;
	};
}
