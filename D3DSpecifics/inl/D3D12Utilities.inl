#ifndef ULTREALITY_RENDERING_D3D12_UTILITIES_INL
#define ULTREALITY_RENDERING_D3D12_UTILITIES_INL

#if defined(__GNUC__) or defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace UltReality::Rendering::D3D12
{
    FORCE_INLINE void ThrowIfFailed(HRESULT hr, const char* file, uint32_t line)
    {
        if (FAILED(hr))
        {
            throw D3DException(hr, file, line);
        }
    }

	FORCE_INLINE uint32_t D3D12Utilities::CalcConstantBufferSize(uint32_t bytes)
	{
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (bytes + 255) & ~255;
	}
}

#endif // !ULTREALITY_RENDERING_D3D12_UTILITIES_INL
