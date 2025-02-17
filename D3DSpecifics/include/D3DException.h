#ifndef ULTREALITY_RENDERING_DXEXCEPTION_H
#define ULTREALITY_RENDERING_DXEXCEPTION_H

#include <string>
#include <stdexcept>

#include <Windows.h>

namespace UltReality::Rendering::D3D12
{
	struct D3DException : public std::runtime_error
	{
		D3DException(HRESULT hr, const std::string& fileName, uint32_t lineNumber);

		const char* what() const noexcept override;

		HRESULT _errorCode = S_OK;
		std::string _fileName;
		std::string _msg;
		int32_t _lineNumber = -1;
	};
}

#endif // !ULTREALITY_RENDERING_DXEXCEPTION_H
