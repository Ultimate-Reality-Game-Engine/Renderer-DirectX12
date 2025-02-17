#ifndef ULTREALITY_RENDERING_D3D12_UPLOAD_BUFFER_H
#define ULTREALITY_RENDERING_D3D12_UPLOAD_BUFFER_H

#include <stdint.h>

#include <wrl.h>
#include <d3d12.h>

#include <D3D12Utilities.h>

namespace UltReality::Rendering::D3D12
{
	template<typename T>
	class D3D12UploadBuffer
	{
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
		uint8_t* m_mappedData = nullptr;
		uint32_t m_elementByteSize = sizeof(T);
		bool m_isConstantBuffer = false;

	public:
		D3D12UploadBuffer(ID3D12Device* device, uint32_t elementCount, bool isConstantBuffer)
			: m_isConstantBuffer(isConstantBuffer)
		{
			// Constant buffer elements need to be multiples of 256 bytes.
			// This is because the hardware can only view constant data 
			// at m*256 byte offsets and of n*256 byte lengths. 
			// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
			// UINT64 OffsetInBytes; // multiple of 256
			// UINT   SizeInBytes;   // multiple of 256
			// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
			if (isConstantBuffer)
				m_elementByteSize = D3D12Utilities::CalcConstantBufferSize(sizeof(T));

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(m_lementByteSize * elementCount),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_uploadBuffer)));

			ThrowIfFailed(m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData)));
		}

		~D3D12UploadBuffer()
		{
			if (m_uploadBuffer != nullptr)
			{
				m_uploadBuffer->Unmap(0, nullptr);
			}

			m_mappedData = nullptr;
		}

		ID3D12Resource* Resource() const
		{
			return m_uploadBuffer.Get();
		}

		void CopyData(int elementIndex, const T& data)
		{
			memcpy(&m_mappedData[elementIndex * m_elementByteSize], &data, sizeof(T));
		}

		D3D12UploadBuffer(const UploadBuffer&) = delete;
		D3D12UploadBuffer& operator=(const UploadBuffer&) = delete;
	};
}

#endif // !ULTREALITY_RENDERING_D3D12_UPLOAD_BUFFER_H
