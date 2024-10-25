#include <DirectX12Renderer.h>

#include <stdexcept>
#include <comdef.h>

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

		m_4xMsaaQuality = qualityLevels.NumQualityLevels;
#if defined(_DEBUG) or defined(DEBUG)
		assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level");
#endif
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
}