#ifndef ULTREALITY_RENDERING_D3D12_DEVICE_RESOURCES_H
#define ULTREALITY_RENDERING_D3D12_DEVICE_RESOURCES_H

#include <stdint.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace UltReality::Rendering::D3D12
{
	struct DeviceResources
	{
		// ComPtr to the DirectX device
		Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
		// ComPtr to a DXGI Factory object. Used to create DirectX objects and structs
		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;

		// ComPtr to the renderer's command queue. Items submitted to command queue are submitted as soon as possible
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
		// ComPtr to renderer's command list. Items submitted to command list are pooled until explicitly executed. Can be used to create reusable sequence of commands for repeated submission
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> commandList;
		// ComPtr to an allocator used to create commands for the command list
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAlloc;

		// ComPtr to Pipeline fence object
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;

		uint64_t currentFence = 0;
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_DEVICE_RESOURCES_H
