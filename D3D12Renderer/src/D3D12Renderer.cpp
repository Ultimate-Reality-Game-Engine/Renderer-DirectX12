#include <directx/d3dx12.h>

#include <DirectXColors.h>
#if defined(DEBUG) or defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include <D3D12Renderer.h>
#include <D3D12Utilities.h>
#include <D3DException.h>

using namespace Microsoft::WRL;
using namespace UltReality::Utilities;
using namespace DirectX;
using namespace UltReality::Rendering::D3D12;

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

namespace UltReality::Rendering
{
	D3D12Renderer::~D3D12Renderer()
	{
		if(m_d3dDevice)
			FlushCommandQueue();

#if defined(DEBUG) or defined(_DEBUG)
		ComPtr<IDXGIDebug> dxgiDebug;
		ThrowIfFailed(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));
#if defined(_D3DDebug_ListLiveOBJ)
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
#endif
#endif
	}

	FORCE_INLINE void D3D12Renderer::CreateDevice()
	{
		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

		HRESULT hardwareResult = D3D12CreateDevice(
			nullptr, // Default adapter
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_d3dDevice)
		);

		// Fallback to WARP device
		if (FAILED(hardwareResult))
		{
			ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

			ThrowIfFailed(D3D12CreateDevice(
				pWarpAdapter.Get(),
				D3D_FEATURE_LEVEL_12_0,
				IID_PPV_ARGS(&m_d3dDevice)
			));
		}
	}

	FORCE_INLINE void D3D12Renderer::CreateFence()
	{
		ThrowIfFailed(m_d3dDevice->CreateFence(
			0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)
		));
	}

	FORCE_INLINE void D3D12Renderer::GetDescriptorSizes()
	{
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		m_cbvSrvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	FORCE_INLINE bool D3D12Renderer::CheckMSAAQualitySupport(const uint32_t sampleCount)
	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
		qualityLevels.Format = m_backBufferFormat;
		qualityLevels.SampleCount = sampleCount;
		qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		qualityLevels.NumQualityLevels = 0;

		ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(
			D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&qualityLevels,
			sizeof(qualityLevels)));

		m_antiAliasingSettings.qualityLevel = qualityLevels.NumQualityLevels;
		if (m_antiAliasingSettings.qualityLevel < 1)
			return false;

		return true;
	}

	FORCE_INLINE void D3D12Renderer::ConfigureMSAA()
	{
		if (CheckMSAAQualitySupport(m_antiAliasingSettings.sampleCount))
		{
			m_msaaEnabled = true;
			m_msaaSampleCount = m_antiAliasingSettings.sampleCount;
			m_msaaQualityLevel = m_antiAliasingSettings.qualityLevel;

			RecreateMSAAResources();
		}
	}

	FORCE_INLINE void D3D12Renderer::DisableMSAA()
	{
		m_msaaEnabled = false;
		m_msaaSampleCount = 1;
		m_msaaQualityLevel = 0;

		RecreateMSAAResources();
	}

	FORCE_INLINE void D3D12Renderer::RecreateMSAAResources()
	{
		FlushCommandQueue();

		CreateRenderTargetView();
		CreateDepthStencilBuffer();
	}

	FORCE_INLINE void D3D12Renderer::CreateCommandObjects()
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateCommandQueue(
			&queueDesc,
			IID_PPV_ARGS(&m_commandQueue)
		));

		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_directCmdListAlloc.GetAddressOf())
		));

		ThrowIfFailed(m_d3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_directCmdListAlloc.Get(), // Associated command allocator
			nullptr, // Initial PipelineStateObject
			IID_PPV_ARGS(m_commandList.GetAddressOf())
		));

		// Start off in a closed state. This is because the first time we
		// refer to the command list we will reset it, and it needs to be
		// closed before calling reset
		m_commandList->Close();
	}

	FORCE_INLINE void D3D12Renderer::CreateSwapChain()
	{
		// Release the previous swapchain we will be recreating
		m_swapChain.Reset();

		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = m_displaySettings.width;
		sd.BufferDesc.Height = m_displaySettings.height;
		sd.BufferDesc.RefreshRate = DXGI_RATIONAL{ m_displaySettings.refreshRate, 1 };
		sd.BufferDesc.Format = m_backBufferFormat;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		sd.SampleDesc.Count = m_msaaEnabled ? m_antiAliasingSettings.sampleCount : 1;
		sd.SampleDesc.Quality = m_msaaEnabled ? (m_antiAliasingSettings.qualityLevel - 1) : 0;

		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = m_swapChainBufferCount;
		sd.OutputWindow = m_mainWin;
		sd.Windowed = m_displaySettings.mode == DisplaySettings::ScreenMode::Windowed;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// Note: Swap chain uses queue to perform flush
		ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
			m_commandQueue.Get(),
			&sd,
			m_swapChain.GetAddressOf()
		));
	}

	FORCE_INLINE void D3D12Renderer::CreateDescriptorHeaps()
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.NumDescriptors = m_swapChainBufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS(m_rtvHeap.GetAddressOf())
		));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
			&dsvHeapDesc,
			IID_PPV_ARGS(m_dsvHeap.GetAddressOf())
		));
	}

	FORCE_INLINE void D3D12Renderer::CreateRenderTargetView()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (uint8_t i = 0; i < m_swapChainBufferCount; i++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(
				i, 
				IID_PPV_ARGS(&m_swapChainBuffer[i])
			));

			m_d3dDevice->CreateRenderTargetView(
				m_swapChainBuffer[i].Get(),
				nullptr,
				rtvHeapHandle
			);

			rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
		}

		/*if (m_msaaEnabled)
		{
			D3D12_RESOURCE_DESC msaaRenderTargetDesc = {};
			msaaRenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			msaaRenderTargetDesc.Width = m_displaySettings.width;
			msaaRenderTargetDesc.Height = m_displaySettings.height;
			msaaRenderTargetDesc.MipLevels = 1;
			msaaRenderTargetDesc.Format = m_backBufferFormat;
			msaaRenderTargetDesc.SampleDesc.Count = m_msaaSampleCount;
			msaaRenderTargetDesc.SampleDesc.Quality = m_msaaQualityLevel;
			msaaRenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE clearValue = {};
			clearValue.Format = m_backBufferFormat;
			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;

			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
			ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&msaaRenderTargetDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearValue,
				IID_PPV_ARGS(m_msaaRenderTarget.GetAddressOf())
			));

			m_d3dDevice->CreateRenderTargetView(m_msaaRenderTarget.Get(), nullptr, m_msaaRtvHeap->GetCPUDescriptorHandleForHeapStart());
		}*/
	}

	FORCE_INLINE void D3D12Renderer::CreateDepthStencilBuffer()
	{
		// Create the depth/stencil buffer and view
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = m_displaySettings.width;
		depthStencilDesc.Height = m_displaySettings.height;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = m_depthStencilFormat;
		depthStencilDesc.SampleDesc.Count = m_msaaEnabled ? m_msaaSampleCount : 1;
		depthStencilDesc.SampleDesc.Quality = m_msaaEnabled ? (m_msaaQualityLevel - 1) : 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = m_depthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())
		));

		// Create descriptor to mip level 0 of entire resource using the format of the resource
		m_d3dDevice->CreateDepthStencilView(
			m_depthStencilBuffer.Get(),
			nullptr,
			DepthStencilView()
		);

		// Transition the resource from its initial state to state so it can be used as a depth buffer
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_depthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		m_commandList->ResourceBarrier(1, &barrier);
	}

	FORCE_INLINE void D3D12Renderer::SetViewport()
	{
		m_screenViewport.TopLeftX = 0;
		m_screenViewport.TopLeftY = 0;
		m_screenViewport.Width = static_cast<float>(m_displaySettings.width);
		m_screenViewport.Height = static_cast<float>(m_displaySettings.height);
		m_screenViewport.MinDepth = 0.0f;
		m_screenViewport.MaxDepth = 1.0f;

		m_scissorRect = { 0,0,m_displaySettings.width, m_displaySettings.height };
	}

	/*FORCE_INLINE void D3D12Renderer::SetScissorRectangles(D3D12_RECT* rect)
	{
		m_commandList->RSSetScissorRects(1, rect);
	}*/

	FORCE_INLINE void D3D12Renderer::UpdateSamplerDescriptor()
	{
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = (m_textureSettings.filteringLevel > 4) ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = m_textureSettings.filteringLevel;

		CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
		m_d3dDevice->CreateSampler(&samplerDesc, samplerHandle);
	}

	FORCE_INLINE void D3D12Renderer::UpdateTextureQuality()
	{
		// Adjust texture resolution scale based on quality setting
		float resolutionScale;
		switch (m_textureSettings.quality)
		{
		case TextureSettings::TextureQuality::low:
			resolutionScale = 0.5f; // Half resolution
			break;

		case TextureSettings::TextureQuality::medium:
			resolutionScale = 0.75f; // Three-quarters resolution
			break;

		case TextureSettings::TextureQuality::high:
			resolutionScale = 1.0f; // Full resolution
			break;

		case TextureSettings::TextureQuality::ultra:
			resolutionScale = 1.5f; // Enhanced resolution (of supported)
			break;
		}

		// Apply resolution scaling logic (e.g., recreate texture resources)
		//RecreateTextures(resolutionScale);
	}

	FORCE_INLINE void D3D12Renderer::UpdateMipmapping()
	{
		// Recreate textures with or without mipmaps
		/*for (auto& texture : m_loadedTextures)
		{
			texture->EnableMipmaps(m_textureSettings.mipmapping);
		}*/
	}

	FORCE_INLINE void D3D12Renderer::UpdateShadowQuality()
	{
		switch (m_shadowSettings.quality)
		{
		case ShadowSettings::ShadowQuality::low:
			m_shadowBias = 0.005f;
			m_shadowSampleCount = 4;
			break;

		case ShadowSettings::ShadowQuality::medium:
			m_shadowBias = 0.003f;
			m_shadowSampleCount = 8;
			break;

		case ShadowSettings::ShadowQuality::high:
			m_shadowBias = 0.001f;
			m_shadowSampleCount = 16;
			break;

		case ShadowSettings::ShadowQuality::ultra:
			m_shadowBias = 0.0005f;
			m_shadowSampleCount = 32;
			break;
		}
	}

	FORCE_INLINE void D3D12Renderer::RecreateShadowMap()
	{
		// Release the old shadow map
		m_shadowMap.Reset();

		D3D12_RESOURCE_DESC shadowMapDesc = {};
		shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		shadowMapDesc.Width = m_shadowSettings.mapResolution;
		shadowMapDesc.Height = m_shadowSettings.mapResolution;
		shadowMapDesc.MipLevels = 1;
		shadowMapDesc.Format = DXGI_FORMAT_D32_FLOAT;
		shadowMapDesc.SampleDesc.Count = 1;
		shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&shadowMapDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(m_shadowMap.GetAddressOf())
		));

		// Create a depth-stencil view for the shadow map
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_d3dDevice->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, m_shadowMapHeap->GetCPUDescriptorHandleForHeapStart());
	}

	FORCE_INLINE void D3D12Renderer::UpdateSoftShadowsState()
	{

	}

	FORCE_INLINE ID3D12Resource* D3D12Renderer::CurrentBackBuffer() const
	{
		return m_swapChainBuffer[m_currBackBuffer].Get();
	}

	FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE D3D12Renderer::CurrentBackBufferView() const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), // handle start 
			m_currBackBuffer, // Index to offset 
			m_rtvDescriptorSize // Size in bytes of the descriptor
		);
	}

	FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE D3D12Renderer::DepthStencilView() const
	{
		return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void D3D12Renderer::Initialize(DisplayTarget targetWindow, const GameTimer* gameTimer)
	{
		// cache a reference to the target window
		m_mainWin = targetWindow.ToHWND();
		m_gameTimer = gameTimer;

#if defined(DEBUG) || defined(_DEBUG) 
		// Enable the D3D12 debug layer.
		{
			ComPtr<ID3D12Debug> debugController;
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
			debugController->EnableDebugLayer();

#if defined(_D3DDebug_Verbose)
			ComPtr<ID3D12Debug1> debugController1;
			ThrowIfFailed(debugController.As(&debugController1));
			debugController1->SetEnableGPUBasedValidation(true);
			debugController1->SetEnableSynchronizedCommandQueueValidation(true);
#endif
		}
#endif

		// Initialize DirectX components using m_mainWin
		CreateDevice();
		CreateFence();
		GetDescriptorSizes();

		// Check MSAA
		if (m_antiAliasingSettings.type == AntiAliasingSettings::AntiAliasingType::MSAA)
		{
			if (CheckMSAAQualitySupport(m_antiAliasingSettings.sampleCount))
				m_msaaEnabled = false;
			else
				m_msaaEnabled = false;
		}

#if defined(DEBUG) or defined(_DEBUG)
		LogAdapters();
#endif

		CreateCommandObjects();
		CreateSwapChain();
		CreateDescriptorHeaps();
		//CreateRenderTargetView();
		//CreateDepthStencilBuffer();
		//SetViewport();
	}

	void D3D12Renderer::Render()
	{
		// Reuse the memory associated with the command recording
		// We can only reset when the associated command list have finished
		// execution on the gpu
		ThrowIfFailed(m_directCmdListAlloc->Reset());

		// A command list can be reset after it has been added to the
		// command queue via ExecuteCommandList. Reusing the command list
		// reuses memory
		ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

		// Indicate a state transition on the resource usage
		auto presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(
			1,
			&presentBarrier);

		// Set the viewport and scissor rect. This needs to be reset
		// whenever the command list is reset
		m_commandList->RSSetViewports(1, &m_screenViewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);

		// Clear the back buffer and depth buffer
		m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// Specify the buffers we are going to render to
		auto backBufferView = CurrentBackBufferView();
		auto depthStencilView = DepthStencilView();
		m_commandList->OMSetRenderTargets(1, &backBufferView, true, &depthStencilView);

		// Indicate a state transition on the resource usage
		auto swapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &swapBarrier);

		// Done recording commands
		ThrowIfFailed(m_commandList->Close());

		// Add command list to the queue for execution
		ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	}

	void D3D12Renderer::Present()
	{
		// Swap the back and front buffers
		ThrowIfFailed(m_swapChain->Present(0, 0));
		m_currBackBuffer = (m_currBackBuffer + 1) % m_swapChainBufferCount;

		// Wait until frame commands are complete. This waiting is
		// inefficient and is done for simplicity. Later we wil show how to
		// organize our rendering code so we don't have to wait per frame
		FlushCommandQueue();
	}

	void D3D12Renderer::FlushCommandQueue()
	{
		// Advance the fence value to mark commands up to this fence point.
		m_currentFence++;

		// Add an instruction to the command queue to set a new fence point.  Because we 
		// are on the GPU timeline, the new fence point won't be set until the GPU finishes
		// processing all the commands prior to this Signal().
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

		// Wait until the GPU has completed commands up to this fence point.
		if (m_fence->GetCompletedValue() < m_currentFence)
		{
			HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);

			// Fire event when GPU hits current fence.  
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));

			// Wait until the GPU hits current fence event is fired.
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

	void D3D12Renderer::CalculateFrameStats(FrameStats* fs)
	{
		// Code computes the average frames per second, and also the 
		// average time it takes to render one frame.  These stats 
		// are appended to the window caption bar.

		static int frameCnt = 0;
		static float timeElapsed = 0.0f;

		frameCnt++;

		// Compute averages over one second period.
		if ((m_gameTimer->GetTotalTime() - timeElapsed) >= 1.0f)
		{
			fs->fps = (float)frameCnt; // fps = frameCnt / 1
			fs->mspf = 1000.0f / fs->fps;

			// Reset for next average.
			frameCnt = 0;
			timeElapsed += 1.0f;
		}
	}

	void D3D12Renderer::LogAdapters()
	{
		UINT i = 0;
		IDXGIAdapter* adapter = nullptr;
		std::vector<IDXGIAdapter*> adapterList;
		while (m_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);

			std::wstring text = L"***Adapter: ";
			text += desc.Description;
			text += L"\n";

			OutputDebugStringW(text.c_str());

			adapterList.push_back(adapter);

			++i;
		}

		for (size_t i = 0; i < adapterList.size(); ++i)
		{
			LogAdapterOutputs(adapterList[i]);
			ReleaseCom(adapterList[i]);
		}
	}

	void D3D12Renderer::LogAdapterOutputs(IDXGIAdapter* adapter)
	{
		UINT i = 0;
		IDXGIOutput* output = nullptr;
		while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);

			std::wstring text = L"***Output: ";
			text += desc.DeviceName;
			text += L"\n";
			OutputDebugStringW(text.c_str());

			LogOutputDisplayModes(output, m_backBufferFormat);

			ReleaseCom(output);

			++i;
		}
	}

	void D3D12Renderer::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
	{
		UINT count = 0;
		UINT flags = 0;

		// Call with nullptr to get list count.
		output->GetDisplayModeList(format, flags, &count, nullptr);

		std::vector<DXGI_MODE_DESC> modeList(count);
		output->GetDisplayModeList(format, flags, &count, &modeList[0]);

		for (auto& x : modeList)
		{
			UINT n = x.RefreshRate.Numerator;
			UINT d = x.RefreshRate.Denominator;
			std::wstring text =
				L"Width = " + std::to_wstring(x.Width) + L" " +
				L"Height = " + std::to_wstring(x.Height) + L" " +
				L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
				L"\n";
			::OutputDebugStringW(text.c_str());
		}
	}

	void RENDERER_INTERFACE_CALL D3D12Renderer::SetDisplaySettings(const DisplaySettings& settings)
	{
		if (settings.width != m_displaySettings.width || settings.height != m_displaySettings.height)
		{
			// Update cached dimensions
			m_displaySettings.width = settings.width;
			m_displaySettings.height = settings.height;
		}

		if (settings.mode != m_displaySettings.mode)
		{
			m_displaySettings.mode = settings.mode;
		}

		if (settings.refreshRate != m_displaySettings.refreshRate)
		{
			m_displaySettings.refreshRate = settings.refreshRate;
		}

		if (settings.vSync != m_displaySettings.vSync)
		{
			m_displaySettings.vSync = settings.vSync;
		}

		FlushCommandQueue();

		ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

		for (uint8_t i = 0; i < m_swapChainBufferCount; i++)
		{
			m_swapChainBuffer[i].Reset();
		}
		m_depthStencilBuffer.Reset();

		m_swapChain->ResizeBuffers(
			m_swapChainBufferCount,
			m_displaySettings.width,
			m_displaySettings.height,
			m_backBufferFormat,
			DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
		);

		m_currBackBuffer = 0;

		//// Handle fullscreen, borderless, or windowed mode
		//if (m_swapChain)
		//{
		//	BOOL isCurrentlyFullscreen = FALSE;
		//	m_swapChain->GetFullscreenState(&isCurrentlyFullscreen, nullptr);

		//	if (m_displaySettings.mode == DisplaySettings::ScreenMode::Fullscreen && !isCurrentlyFullscreen)
		//	{
		//		m_swapChain->SetFullscreenState(TRUE, nullptr);
		//	}
		//	else if (m_displaySettings.mode != DisplaySettings::ScreenMode::Fullscreen && isCurrentlyFullscreen)
		//	{
		//		m_swapChain->SetFullscreenState(FALSE, nullptr);
		//	}

		//	// Borderless mode
		//	if (m_displaySettings.mode == DisplaySettings::ScreenMode::Borderless)
		//	{
		//		SetWindowLongPtr(m_mainWin, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		//		SetWindowPos(m_mainWin, HWND_TOP, 0, 0, m_displaySettings.width, m_displaySettings.height, SWP_FRAMECHANGED);
		//	}
		//}

		// Recreate render target views for the new swap chain buffers
		CreateRenderTargetView();

		// Recreate the depth-stencil buffer
		CreateDepthStencilBuffer();

		// Execute the resize commands
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

		// Wait until Resize is complete
		FlushCommandQueue();

		// Update the viewport and scissor rect
		SetViewport();
	}

	void RENDERER_INTERFACE_CALL D3D12Renderer::SetAntiAliasingSettings(const AntiAliasingSettings& settings)
	{
		m_antiAliasingSettings.sampleCount = settings.sampleCount;
		m_antiAliasingSettings.qualityLevel = settings.qualityLevel;

		if (settings.type != m_antiAliasingSettings.type)
		{
			m_antiAliasingSettings.type = settings.type;

			if (settings.type == AntiAliasingSettings::AntiAliasingType::MSAA)
			{
				ConfigureMSAA();
			}
			else
			{
				DisableMSAA();
			}

			// Additional logic for FXAA/TAA added here is needed
		}
	}

	void RENDERER_INTERFACE_CALL D3D12Renderer::SetTextureSettings(const TextureSettings& settings)
	{
		// Check and apply filtering level changes
		if (settings.filteringLevel != m_textureSettings.filteringLevel)
		{
			m_textureSettings.filteringLevel = settings.filteringLevel;

			// Update sampler descriptors to reflect the new filtering level
			UpdateSamplerDescriptor();
		}

		// Check and apply texture quality changes
		if (settings.quality != m_textureSettings.quality)
		{
			m_textureSettings.quality = settings.quality;

			// Adjust texture resource resolution/scaling to match the quality setting
			UpdateTextureQuality();
		}

		// Check and apply mipmapping changes
		if (settings.mipmapping != m_textureSettings.mipmapping)
		{
			m_textureSettings.mipmapping = settings.mipmapping;

			// recreate textures with or without mipmaps as needed
			UpdateMipmapping();
		}
	}

	void RENDERER_INTERFACE_CALL D3D12Renderer::SetShadowSettings(const ShadowSettings& settings)
	{
		// Check and apply shadow quality changes
		if (settings.quality != m_shadowSettings.quality)
		{
			m_shadowSettings.quality = settings.quality;

			// Adjust shadow rendering parameters based on quality
			UpdateShadowQuality();
		}

		// Check and apply shadow map resolution changes
		if (settings.mapResolution != m_shadowSettings.mapResolution)
		{
			m_shadowSettings.mapResolution = settings.mapResolution;

			// Recreate shadow maps with the new resolution
			RecreateShadowMap();
		}

		// Check and apply soft shadow settings
		if (settings.softShadows != m_shadowSettings.softShadows)
		{
			m_shadowSettings.softShadows = settings.softShadows;

			// Update pipeline state or shaders to toggle soft shadows
			UpdateSoftShadowsState();
		}
	}

	/// <summary>
	/// Method to set the lighting settings for the renderers
	/// </summary>
	/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.LightingSettings"/> struct to get settings from</param>
	void RENDERER_INTERFACE_CALL D3D12Renderer::SetLightingSettings(const LightingSettings& settings)
	{

	}

	/// <summary>
	/// Method to set the post-processing settings for the renderer
	/// </summary>
	/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.PostProcessingSettings"/> struct to get settings from</param>
	void RENDERER_INTERFACE_CALL D3D12Renderer::SetPostProcessingSettings(const PostProcessingSettings& settings)
	{

	}

	/// <summary>
	/// Method to set the performance related settings for the renderer
	/// </summary>
	/// <param name="settings">Instance of <seealso cref="UltReality.Rendering.PerformanceSettings"/> struct to get settings from</param>
	void RENDERER_INTERFACE_CALL D3D12Renderer::SetPerformanceSettings(const PerformanceSettings& settings)
	{

	}
}
