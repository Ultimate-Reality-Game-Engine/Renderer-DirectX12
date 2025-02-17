#ifndef ULTREALITY_RENDERING_D3D12_UTILITIES_H
#define ULTREALITY_RENDERING_D3D12_UTILITIES_H

#include <Windows.h>

#include <stdint.h>

#include <D3DException.h>

namespace UltReality::Rendering::D3D12
{
	void ThrowIfFailed(HRESULT hr, const char* file = __FILE__, uint32_t line = __LINE__);

	struct D3D12Utilities
	{
		static uint32_t CalcConstantBufferSize(uint32_t bytes);
	};
}

#include <D3D12Utilities.inl>

#endif // !ULTREALITY_RENDERING_D3D12_UTILITIES_H
