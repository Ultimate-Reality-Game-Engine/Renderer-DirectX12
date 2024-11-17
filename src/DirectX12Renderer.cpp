#include <directx/d3dx12.h>

#include <stdexcept>
#include <comdef.h>

#include <windowsx.h>

#include <DirectX12Renderer.h>

using namespace Microsoft::WRL;
using namespace UltReality::Utilities;

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

namespace UltReality::Rendering
{
	namespace
	{
		FORCE_INLINE void ThrowIfFailed(HRESULT hr)
		{
			if (FAILED(hr))
			{
				_com_error err(hr);
				throw std::runtime_error(err.ErrorMessage());
			}
		}
	}

	FORCE_INLINE void DirectX12Renderer::CreateDevice()
	{
#if defined(_DEBUG) or defined(DEBUG)
		// Enable the D3D12 debug layer
		{
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)));
			m_debugController->EnableDebugLayer();
		}
#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

		HRESULT hardwareResult = D3D12CreateDevice(
			nullptr,
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

	FORCE_INLINE void DirectX12Renderer::CreateFence()
	{
		ThrowIfFailed(m_d3dDevice->CreateFence(
			0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)
		));
	}

	FORCE_INLINE void DirectX12Renderer::GetDescriptorSizes()
	{
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		m_cbvSrvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	FORCE_INLINE void DirectX12Renderer::CheckMSAAQualitySupport(const uint32_t sampleCount)
	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
		qualityLevels.Format = m_backBufferFormat;
		qualityLevels.SampleCount = sampleCount;
		qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		qualityLevels.NumQualityLevels = 0;

		ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(
			D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&qualityLevels,
			sizeof(qualityLevels)
		));

		m_MSAAQuality = qualityLevels.NumQualityLevels;
#if defined(_DEBUG) or defined(DEBUG)
		assert(m_MSAAQuality > 0 && "Unexpected MSAA quality level");
#endif
		m_MSAAState = true;
	}

	FORCE_INLINE void DirectX12Renderer::CreateCommandQueue()
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateCommandQueue(
			&queueDesc,
			IID_PPV_ARGS(&m_commandQueue)
		));
	}

	FORCE_INLINE void DirectX12Renderer::CreateCommandAllocator()
	{
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_directCmdListAlloc.GetAddressOf())
		));
	}

	FORCE_INLINE void DirectX12Renderer::CreateCommandList()
	{
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

	FORCE_INLINE void DirectX12Renderer::CreateSwapChain()
	{
		// Release the previous swapchain we will be recreating
		m_swapChain.Reset();

		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = m_clientWidth;
		sd.BufferDesc.Height = m_clientHeight;
		sd.BufferDesc.RefreshRate = m_clientRefreshRate;
		sd.BufferDesc.Format = m_backBufferFormat;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		sd.SampleDesc.Count = m_MSAAState ? 4 : 1;
		sd.SampleDesc.Quality = m_MSAAState ? (m_MSAAQuality - 1) : 0;

		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = m_swapChainBufferCount;
		sd.OutputWindow = m_mainWin;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// Note: Swap chain uses queue to perform flush
		ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
			m_commandQueue.Get(),
			&sd,
			m_swapChain.GetAddressOf()
		));
	}

	FORCE_INLINE void DirectX12Renderer::CreateDescriptorHeaps()
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

	FORCE_INLINE void DirectX12Renderer::CreateRenderTargetView()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (uint32_t i = 0; i < m_swapChainBufferCount; i++)
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
	}

	FORCE_INLINE void DirectX12Renderer::CreateDepthStencilBuffer()
	{
		// Create the depth/stencil buffer and view
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = m_clientWidth;
		depthStencilDesc.Height = m_clientHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = m_depthStencilFormat;
		depthStencilDesc.SampleDesc.Count = m_MSAAState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = m_MSAAState ? (m_MSAAQuality - 1) : 0;
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

	FORCE_INLINE void DirectX12Renderer::SetViewport()
	{
		D3D12_VIEWPORT vp;
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		vp.Width = static_cast<float>(m_clientWidth);
		vp.Height = static_cast<float>(m_clientHeight);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		m_commandList->RSSetViewports(
			1,
			&vp
		);
	}

	/*FORCE_INLINE void DirectX12Renderer::SetScissorRectangles(D3D12_RECT* rect)
	{
		m_commandList->RSSetScissorRects(1, rect);
	}*/

	FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE DirectX12Renderer::CurrentBackBufferView() const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), // handle start 
			m_currBackBuffer, // Index to offset 
			m_rtvDescriptorSize // Size in bytes of the descriptor
		);
	}

	FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE DirectX12Renderer::DepthStencilView() const
	{
		return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void DirectX12Renderer::Initialize(DisplayTarget targetWindow, const GameTimer* gameTimer)
	{
		// cache a reference to the target window
		m_mainWin = targetWindow.ToHWND();
		m_gameTimer = gameTimer;

		// Initialize DirectX components using m_mainWin
		CreateDevice();
		CreateFence();
		GetDescriptorSizes();
		CheckMSAAQualitySupport(4);
		CreateCommandQueue();
		CreateCommandAllocator();
		CreateCommandList();
		CreateSwapChain();
		CreateDescriptorHeaps();
		CreateRenderTargetView();
		CreateDepthStencilBuffer();
		SetViewport();
	}

	void DirectX12Renderer::OnResize(const EWindowResize& event)
	{
		using SizeDetails = EWindowResize::SizeDetails;

		// Save the new client area dimensions.
		m_clientWidth = event.width;
		m_clientHeight = event.height;

		// Flush before changing any resources.
		FlushCommandQueue();

		ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

		// Release the previous resources we will be recreating.
		for (int i = 0; i < m_swapChainBufferCount; ++i)
			m_swapChainBuffer[i].Reset();
		m_depthStencilBuffer.Reset();

		// Resize the swap chain.
		ThrowIfFailed(m_swapChain->ResizeBuffers(
			m_swapChainBufferCount,
			m_clientWidth, m_clientHeight,
			m_backBufferFormat,
			DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

		m_currBackBuffer = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < m_swapChainBufferCount; i++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffer[i])));
			m_d3dDevice->CreateRenderTargetView(m_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
		}

		// Create the depth/stencil buffer and view.
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = m_clientWidth;
		depthStencilDesc.Height = m_clientHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = m_depthStencilFormat;
		depthStencilDesc.SampleDesc.Count = m_MSAAState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = m_MSAAState ? (m_MSAAQuality - 1) : 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = m_depthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

		// Create descriptor to mip level 0 of entire resource using the format of the resource.
		m_d3dDevice->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, DepthStencilView());

		// Transition the resource from its initial state to be used as a depth buffer.
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		// Execute the resize commands.
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		// Wait until resize is complete.
		FlushCommandQueue();

		// Update the viewport transform to cover the client area.
		m_screenViewport.TopLeftX = 0;
		m_screenViewport.TopLeftY = 0;
		m_screenViewport.Width = static_cast<float>(m_clientWidth);
		m_screenViewport.Height = static_cast<float>(m_clientHeight);
		m_screenViewport.MinDepth = 0.0f;
		m_screenViewport.MaxDepth = 1.0f;

		m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };
	}

	void DirectX12Renderer::FlushCommandQueue()
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
			HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

			// Fire event when GPU hits current fence.  
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));

			// Wait until the GPU hits current fence event is fired.
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

	void DirectX12Renderer::CalculateFrameStats(FrameStats* fs)
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

	void DirectX12Renderer::LogAdapters()
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

	void DirectX12Renderer::LogAdapterOutputs(IDXGIAdapter* adapter)
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

	void DirectX12Renderer::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
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
}
