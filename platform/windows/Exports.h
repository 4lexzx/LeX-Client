#pragma once

#if defined(_WIN32)
#  if defined(LEX_BUILD_DLL)
#    define LEX_API __declspec(dllexport)
#  else
#    define LEX_API __declspec(dllimport)
#  endif
#else
#  define LEX_API
#endif
