#include <D3DException.h>

#include <comdef.h>
#include <windowsx.h>

namespace UltReality::Rendering::D3D12
{
	D3DException::D3DException(HRESULT hr, const std::string& fileName, uint32_t lineNumber) 
		: std::runtime_error("DirectX Exception"), _errorCode(hr), _fileName(fileName), _lineNumber(lineNumber)
	{
		_com_error err(_errorCode);
		_msg = std::string("DirectX exception in ") + _fileName + "; line " + std::to_string(_lineNumber) + "; error: " + err.ErrorMessage();
	}

	const char* D3DException::what() const noexcept
	{
		return _msg.c_str();
	}
}