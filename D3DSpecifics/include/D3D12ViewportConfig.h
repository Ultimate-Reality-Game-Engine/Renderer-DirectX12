#ifndef ULTREALITY_RENDERING_D3D12_VIEWPORT_CONFIG_H
#define ULTREALITY_RENDERING_D3D12_VIEWPORT_CONFIG_H

#include <d3d12.h>

namespace UltReality::Rendering::D3D12
{
	struct ViewportConfig
	{
		D3D12_VIEWPORT screenViewport;
		D3D12_RECT scissorRect;
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_VIEWPORT_CONFIG_H
