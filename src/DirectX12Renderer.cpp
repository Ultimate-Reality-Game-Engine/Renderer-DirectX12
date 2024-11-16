#include <directx/d3dx12.h>

#include <stdexcept>
#include <comdef.h>

#include <DirectX12Renderer.h>

using namespace Microsoft::WRL;

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

		m_MsaaQuality = qualityLevels.NumQualityLevels;
#if defined(_DEBUG) or defined(DEBUG)
		assert(m_MsaaQuality > 0 && "Unexpected MSAA quality level");
#endif
		m_MsaaState = true;
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

		sd.SampleDesc.Count = m_MsaaState ? 4 : 1;
		sd.SampleDesc.Quality = m_MsaaState ? (m_MsaaQuality - 1) : 0;

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
		depthStencilDesc.SampleDesc.Count = m_MsaaState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = m_MsaaState ? (m_MsaaQuality - 1) : 0;
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

	void DirectX12Renderer::Initialize(DisplayTarget targetWindow)
	{
		// cache a reference to the target window
		m_mainWin = targetWindow.ToHWND();

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
}