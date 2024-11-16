#ifndef ULTREALITY_RENDERING_DIRECTX12_RENDERER_H
#define ULTREALITY_RENDERING_DIRECTX12_RENDERER_H

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <stdint.h>

#include <RendererInterface.h>
#include <DisplayTarget.h>

#if defined(__GNUC__) or defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace UltReality::Rendering
{
	/// <summary>
	/// Class implements the <seealso cref="UltReality.Rendering.RendererInterface"/> interface using DirectX12
	/// </summary>
	class DirectX12Renderer : public RendererInterface
	{
	private:
		// Windows specific handle to a Windows window instance for this application. Render target for DirectX
		HWND m_mainWin = nullptr;

		// Render Target View descriptor size
		uint32_t m_rtvDescriptorSize;
		// Depth Stencil View descriptor size
		uint32_t m_dsvDescriptorSize;
		// 
		uint32_t m_cbvSrvDescriptorSize;

		// If MSAA is found to be supported this is set to true. Used to configure DirectX device to use MSAA
		bool m_MsaaState = false;
		// Hardware is queried and quality level set according to capability
		uint32_t m_MsaaQuality;

		// Variable keeps track of the target window's width in pixels
		uint32_t m_clientWidth = 1920;
		// Variable keeps track of the target window's height in pixels
		uint32_t m_clientHeight = 1080;
		// Tracks the devices refresh rate
		DXGI_RATIONAL m_clientRefreshRate = { 60, 1 };
		
		// Format of the texels in the swap chain (back buffer)
		DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		// Static value for the number of render targets in our swap chain (2 for double buffering)
		static constexpr uint8_t m_swapChainBufferCount = 2;
		// Variable tracks which index in the swap chain we are currently rendering to (back buffer)
		uint8_t m_currBackBuffer = 0;

		// Format of the texels in the depth stencil buffers
		DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// ComPtr to the DirectX device
		Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
		// ComPtr to a DXGI Factory object. Used to create DirectX objects and structs
		Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
		// ComPtr to Pipeline fence object
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		// ComPtr to the renderer's command queue. Items submitted to command queue are submitted as soon as possible
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		// ComPtr to an allocator used to create commands for the command list
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCmdListAlloc;
		// ComPtr to renderer's command list. Items submitted to command list are pooled until explicitly executed. Can be used to create reusable sequence of commands for repeated submission
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
		// ComPtr to the renderer's swap chain. Corresponds to buffer on hardware (device)
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		// ComPtr to descriptors for render target views in the swap chain
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		// ComPtr to descriptor for the depth stencil view
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		// Array of ComPtr to the render target buffers in the swap chain
		Microsoft::WRL::ComPtr<ID3D12Resource> m_swapChainBuffer[m_swapChainBufferCount];
		// ComPtr to the depth stencil view buffer
		Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;

#if defined(_DEBUG) or defined(DEBUG)
		Microsoft::WRL::ComPtr<ID3D12Debug> m_debugController;
#endif
		/// <summary>
		/// Method creates the DirectX device <seealso cref="m_d3dDevice"/>
		/// </summary>
		FORCE_INLINE void CreateDevice();

		/// <summary>
		/// Method creates the fence object <seealso cref="m_fence"/>
		/// </summary>
		FORCE_INLINE void CreateFence();

		/// <summary>
		/// Queries the descriptor sizes and sets the descriptor variables. <seealso cref="m_rtvDescriptorSize"/> <seealso cref="m_dsvDescriptorSize"/> <seealso cref="m_cbvSrvDescriptorSize"/>
		/// </summary>
		FORCE_INLINE void GetDescriptorSizes();

		/// <summary>
		/// Queries the devices MSAA quality level support and sets variable with value <seealso cref="m_4xMsaaQuality"/>
		/// </summary>
		/// <param name="sampleCount">Quality level to query for</param>
		FORCE_INLINE void CheckMSAAQualitySupport(const uint32_t sampleCount);

		/// <summary>
		/// Creates the renderer's command queue and sets <seealso cref="m_commandQueue"/>
		/// </summary>
		FORCE_INLINE void CreateCommandQueue();

		/// <summary>
		/// Creates the command allocators and sets <seealso cref="m_directCmdListAlloc"/>
		/// </summary>
		FORCE_INLINE void CreateCommandAllocator();

		/// <summary>
		/// Creates the command list and sets <seealso cref="m_commandList"/>
		/// </summary>
		FORCE_INLINE void CreateCommandList();

		/// <summary>
		/// Creates the swap chain and sets <seealso cref="m_swapChain"/>
		/// </summary>
		FORCE_INLINE void CreateSwapChain();

		/// <summary>
		/// Creates the descriptor heaps, setting <seealso cref="m_rtvHeap"/>, <seealso cref="m_dsvHeap"/>
		/// </summary>
		FORCE_INLINE void CreateDescriptorHeaps();

		/// <summary>
		/// Initializes the items in <seealso cref="m_swapChainBuffer"/>
		/// </summary>
		FORCE_INLINE void CreateRenderTargetView();

		/// <summary>
		/// Initializes the <seealso cref="m_depthStencilBuffer"/>
		/// </summary>
		FORCE_INLINE void CreateDepthStencilBuffer();

		/// <summary>
		/// Issues command to initialize the viewport and its configuration
		/// </summary>
		FORCE_INLINE void SetViewport();
		//FORCE_INLINE void SetScissorRectangles(D3D12_RECT* rect);

		/// <summary>
		/// Gets the current back buffer view
		/// </summary>
		/// <returns>Handle to the current buffer in the swap chain (back buffer)</returns>
		FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;

		/// <summary>
		/// Gets the depth stencil view buffer
		/// </summary>
		/// <returns>Handle to the depth stencil view buffer</returns>
		FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	public:
		/// <summary>
		/// Called to initialize the renderer and prepare it for rendering tasks
		/// </summary>
		/// <param name="targetWindow">The window for the renderer to target. For DirectX12Renderer this should have been initialized with an HWND instance</param>
		RENDERER_INTERFACE_ABI void RENDERER_INTERFACE_CALL Initialize(DisplayTarget targetWindow) final;

		/// <summary>
		/// 
		/// </summary>
		RENDERER_INTERFACE_ABI void RENDERER_INTERFACE_CALL CreateBuffer() final;
		RENDERER_INTERFACE_ABI void RENDERER_INTERFACE_CALL Render() final;
		RENDERER_INTERFACE_ABI void RENDERER_INTERFACE_CALL Present() final;
	};
}

#endif // !ULTREALITY_RENDERING_DIRECTX12_RENDERER_H