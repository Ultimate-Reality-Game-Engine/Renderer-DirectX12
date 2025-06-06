#ifndef ULTREALITY_RENDERING_D3D12_WINDOW_SWAP_CHAIN_H
#define ULTREALITY_RENDERING_D3D12_WINDOW_SWAP_CHAIN_H

#include <stdint.h>

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace UltReality::Rendering::D3D12
{
	struct WindowSwapChain
	{
		// Windows specific handle to a Windows window instance for this application. Render target for DirectX
		HWND mainWin = nullptr;

		// ComPtr to the renderer's swap chain. Corresponds to buffer on hardware (device)
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

		// Static value for the number of render targets in our swap chain (2 for double buffering)
		static constexpr uint8_t swapChainBufferCount = 2;

		// Format of the texels in the swap chain (back buffer)
		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		// Variable tracks which index in the swap chain we are currently rendering to (back buffer)
		uint8_t currBackBuffer = 0;

		// Format of the texels in the depth stencil buffers
		DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// Array of ComPtr to the render target buffers in the swap chain
		Microsoft::WRL::ComPtr<ID3D12Resource> swapChainBuffer[swapChainBufferCount];
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_WINDOW_SWAP_CHAIN_H
