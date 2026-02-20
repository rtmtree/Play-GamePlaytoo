#pragma once

#include "../GSHandler.h"
#include "GSH_WebGPUContext.h"
#include "Types.h"

namespace GSH_WebGPU
{
	class CBackend
	{
	public:
		virtual ~CBackend() = default;

		virtual void Initialize(const ContextPtr& context) = 0;
		virtual void Release() = 0;
		virtual void Reset() = 0;
		virtual void Flip() = 0;

		virtual void WriteRegister(uint8 reg, uint64 value) = 0;
		// Abstracted drawing call
		virtual void DrawVertices(const void* vertices, size_t count, uint32 primitiveType) = 0;
		
		virtual void ProcessHostToLocalTransfer() = 0;
		virtual void ProcessLocalToHostTransfer() = 0;
		virtual void ProcessLocalToLocalTransfer() = 0;
		virtual void ProcessClutTransfer(uint32 cbp, uint32 csa) = 0;

		// Primitive assembly helper (optional, can be done by handler or backend)
		// For now, let handler do assembly and call backend->Draw* ?
		// The Vulkan backend wants Draw calls.
		// The OpenGL backend (monolithic) processed registers directly.
		
		// To support both, we should probably pass the registers to the backend.
		// If the backend wants pre-assembled vertices, it can ask for them?
		// No, better to keep the register interface clean.
		
		// Wait, if I move the register handling (GS_REG_PRIM, XYZ, etc) to backend, 
		// then `GSH_WebGPU` just forwards WriteRegisterImpl.
		// This gives maximum flexibility.
	};

	typedef std::shared_ptr<CBackend> BackendPtr;
}
