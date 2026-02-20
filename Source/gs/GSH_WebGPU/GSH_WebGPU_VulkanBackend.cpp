#include "GSH_WebGPU_VulkanBackend.h"

using namespace GSH_WebGPU;

void CVulkanBackend::WriteRegister(uint8 reg, uint64 value)
{
    if(!m_draw) return;

    switch(reg)
    {
    case GS_REG_SCISSOR_1:
    case GS_REG_SCISSOR_2:
        m_draw->SetScissor(static_cast<uint32>(value));
        break;
    
    // Note: Primitive Assembly (GS_REG_XYZ etc.) is handled by the main handler (CGSH_WebGPU)
    // which then calls m_draw->AddVertices via the backend interface?
    // Wait, the main handler assembles vertices into m_vtxBuffer.
    // It needs to push them to m_draw.
    
    // The main handler writes to registers. If it's a vertex kick, it calls Prim_Triangle etc.
    // Those methods call m_draw->AddVertices.
    // So the BACKEND needs to expose AddVertices or HandleDraw?
    
    // My CBackend interface just has WriteRegister.
    // This implies primitive assembly should happen IN THE BACKEND or the Handler needs access to CDraw.
    
    // If I want to support OpenGL style (which might not use CDraw), I should let the backend handle everything.
    // But the handler (CGSH_WebGPU) contains the vertex assembly state logic (m_vtxBuffer).
    // This logic is common? Or specific to the Vulkan path?
    // OpenGL implementation also needs vertex assembly.
    
    // Strategy:
    // Move Primitive Assembly implementation (Prim_*, m_vtxBuffer) into the Backend?
    // Or keep it in Handler and have a virtual `DrawVertices(vtx, count)` in Backend.
    
    // For now, let's just forward SCISSOR.
    // The Handler handles XYZ/PRIM and calls internal Prim methods. 
    // Those Prim methods need to call `m_backend->DrawVertices(...)`.
    }
}
