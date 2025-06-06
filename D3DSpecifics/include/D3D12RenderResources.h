#ifndef ULTREALITY_RENDERING_D3D12_RENDER_RESOURCES_H
#define ULTREALITY_RENDERING_D3D12_RENDER_RESOURCES_H

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace UltReality::Rendering::D3D12
{
	struct RenderResources
	{
		// ComPtr to descriptors for render target views in the swap chain
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTV_DescriptorHeap;

		// ComPtr to descriptor for the depth stencil view
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DSV_DescriptorHeap;
		// ComPtr to the depth stencil view buffer
		Microsoft::WRL::ComPtr<ID3D12Resource> DSV_Buffer;

		// ComPtr to descriptors for render msaa render target view
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> msaaRTV_DescriptorHeap;
		// ComPtr to the msaa render target view
		Microsoft::WRL::ComPtr<ID3D12Resource> msaaRTV_Buffer;

		// ComPtr to descriptors for texture samplers
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_DescriptorHeap;

		// ComPtr to descriptor heap for shadow map
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowMap_DescriptorHeap;
		// ComPtr to shadow map
		Microsoft::WRL::ComPtr<ID3D12Resource> shadowMap_Buffer;
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_RENDER_RESOURCES_H
