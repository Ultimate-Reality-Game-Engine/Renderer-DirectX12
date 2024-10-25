#ifndef ULTREALITY_RENDERING_DIRECTX12_RENDERER_H
#define ULTREALITY_RENDERING_DIRECTX12_RENDERER_H

#include <RendererInterface.h>

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <stdint.h>

#if defined(__GNUC__) or defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace UltReality::Rendering
{
	class DirectX12Renderer : public RendererInterface
	{
	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

#if defined(_DEBUG) or defined(DEBUG)
		Microsoft::WRL::ComPtr<ID3D12Debug> m_debugController;
#endif

		uint32_t m_rtvDescriptorSize;
		uint32_t m_dsvDescriptorSize;
		uint32_t m_cbvSrvDescriptorSize;

		uint32_t m_4xMsaaQuality;

		FORCE_INLINE void CreateDevice();
		FORCE_INLINE void CreateFence();
		FORCE_INLINE void GetDescriptorSizes();
		FORCE_INLINE void CheckMSAAQualitySupport(const uint32_t sampleCount);
		FORCE_INLINE void CreateCommandQueue();
		FORCE_INLINE void CreateCommandAllocator();
		FORCE_INLINE void CreateCommandList();

	public:
		bool RENDERER_INTERFACE_CALL Initialize() final;
		void RENDERER_INTERFACE_CALL CreateBuffer() final;
		void RENDERER_INTERFACE_CALL Render() final;
		void RENDERER_INTERFACE_CALL Present() final;
	};
}

#endif // !ULTREALITY_RENDERING_DIRECTX12_RENDERER_H
