#pragma once

#if defined(_WIN32)
#include "comdef.h"
#endif
#include "debug_output.h"

namespace DBG
{
	#if defined(_WIN32)
	inline void throw_hr(HRESULT hr)
	{
		if (FAILED(hr))
		{
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			OutputString("COM ERROR:: %s", errMsg);
			throw;
		}
	}

	inline void test_hr(HRESULT hr)
	{
		if (FAILED(hr))
		{
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			OutputString("COM ERROR:: %s", errMsg);
		}
	}
	#else
	inline void throw_hr(int result)
	{
		if (result != 0)
		{
			throw std::runtime_error("Operation failed");
		}
	}

	inline void test_hr(int result)
	{
		if (result != 0)
		{
			OutputString("Operation failed");
		}
	}
	#endif

}