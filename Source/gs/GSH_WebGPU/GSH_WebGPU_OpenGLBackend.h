#pragma once

#include "GSH_WebGPUBackend.h"

namespace GSH_WebGPU
{
	class COpenGLBackend : public CBackend
	{
	public:
		COpenGLBackend();
		virtual ~COpenGLBackend();

		void Initialize(const ContextPtr& context) override;
        void Release() override;
		void Reset() override;
		void Flip() override;

		void WriteRegister(uint8 reg, uint64 value) override;
        void DrawVertices(const void* vertices, size_t count, uint32 primitiveType) override;
		
		void ProcessHostToLocalTransfer() override;
		void ProcessLocalToHostTransfer() override;
		void ProcessLocalToLocalTransfer() override;
		void ProcessClutTransfer(uint32 cbp, uint32 csa) override;

	private:
		ContextPtr m_context;
	};
}
