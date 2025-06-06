#ifndef ULTREALITY_RENDERING_D3D12_RENDERER_H
#define ULTREALITY_RENDERING_D3D12_RENDERER_H

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <stdint.h>

#include <IRenderer.h>
#include <DisplayTarget.h>
#include <PlatformMessageHandler.h>

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
	/// Class implements the <see cref="IRenderer"/> interface using DirectX12
	/// </summary>
	class RENDERER_INTERFACE_ABI D3D12Renderer : public IRenderer
	{
	private:
		// Render Target View descriptor size
		uint32_t m_rtvDescriptorSize;
		// Depth Stencil View descriptor size
		uint32_t m_dsvDescriptorSize;
		// 
		uint32_t m_cbvSrvDescriptorSize;

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
		FORCE_INLINE bool CheckMSAAQualitySupport(const uint32_t sampleCount);

		FORCE_INLINE void ConfigureMSAA();

		FORCE_INLINE void DisableMSAA();

		FORCE_INLINE void RecreateMSAAResources();

		/// <summary>
		/// Creates the renderer's command queue, allocator, and command list and sets <seealso cref="m_commandQueue"/>
		/// </summary>
		FORCE_INLINE void CreateCommandObjects();

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

		FORCE_INLINE void UpdateSamplerDescriptor();

		FORCE_INLINE void UpdateTextureQuality();

		FORCE_INLINE void UpdateMipmapping();

		FORCE_INLINE void UpdateShadowQuality();

		FORCE_INLINE void RecreateShadowMap();

		FORCE_INLINE void UpdateSoftShadowsState();

		FORCE_INLINE ID3D12Resource* CurrentBackBuffer() const;

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

		void LogAdapterOutputs(IDXGIAdapter* adapter);

		void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	public:
		D3D12Renderer() = default;
		~D3D12Renderer();

		/// <summary>
		/// Called to initialize the renderer and prepare it for rendering tasks
		/// </summary>
		/// <param name="targetWindow">The window for the renderer to target. For DirectX12Renderer this should have been initialized with an HWND instance</param>
		void RENDERER_INTERFACE_CALL Initialize(DisplayTarget targetWindow, const UltReality::Utilities::GameTimer* gameTimer) final;

		/// <summary>
		/// 
		/// </summary>
		void RENDERER_INTERFACE_CALL CreateBuffer() final {};

		/// <summary>
		/// Method that issues a render call. Purge the render queue
		/// </summary>
		void RENDERER_INTERFACE_CALL Render() final;

		/// <summary>
		/// Method that draws the rendered buffer onto the target window
		/// </summary>
		void RENDERER_INTERFACE_CALL Present() final;

		/// <summary>
		/// Method that processes the commands queued up the point this method is called
		/// </summary>
		void RENDERER_INTERFACE_CALL FlushCommandQueue() final;

		/// <summary>
		/// Method that calculates frame stats
		/// </summary>
		void RENDERER_INTERFACE_CALL CalculateFrameStats(FrameStats* fs) final;

		/// <summary>
		/// Method that gets info on available adapters and reports details
		/// </summary>
		void RENDERER_INTERFACE_CALL LogAdapters() final;

		/// <summary>
		/// Method to set the display settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.DisplaySettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetDisplaySettings(const DisplaySettings& settings) final;

		/// <summary>
		/// Method to set the anti aliasing settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.AntiAliasingSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetAntiAliasingSettings(const AntiAliasingSettings& settings) final;

		/// <summary>
		/// Method to set the texture settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.TextureSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetTextureSettings(const TextureSettings& settings) final;

		/// <summary>
		/// Method to set the shadow settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.ShadowSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetShadowSettings(const ShadowSettings& settings) final;

		/// <summary>
		/// Method to set the lighting settings for the renderers
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.LightingSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetLightingSettings(const LightingSettings& settings) final;

		/// <summary>
		/// Method to set the post-processing settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.PostProcessingSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetPostProcessingSettings(const PostProcessingSettings& settings) final;

		/// <summary>
		/// Method to set the performance related settings for the renderer
		/// </summary>
		/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.PerformanceSettings"/> struct to get settings from</param>
		void RENDERER_INTERFACE_CALL SetPerformanceSettings(const PerformanceSettings& settings) final;
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_RENDERER_H
