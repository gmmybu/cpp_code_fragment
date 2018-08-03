#pragma once
#include "logger.h"

/// @robust_rank: SSS
/// @review_date: 2017-10-12

#ifndef _NOT_USE_LOGGER_
#define dd_trace(file_name, func_name, line_num) logger_error_va("%s - %s(%d), %d", file_name, func_name, GetLastError(), line_num)
#else
#define dd_trace(file_name, func_name, line_num)
#endif

#define dd_checkbool_z(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return 0; }
#define dd_checkbool_r(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return false; }
#define dd_checkbool_p(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return nullptr; }
#define dd_checkbool_n(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return; }
#define dd_checkbool_s(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); }

#define dd_checkd3d9_z(x) if (FAILED(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return 0; }
#define dd_checkd3d9_r(x) if (FAILED(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return false; }
#define dd_checkd3d9_p(x) if (FAILED(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return nullptr; }
#define dd_checkd3d9_n(x) if (FAILED(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); return; }
#define dd_checkd3d9_s(x) if (FAILED(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); }

#define dd_assert(x) if (!(x)) { dd_trace(__FILE__, __FUNCTION__, __LINE__); abort(); }

#define dd_checkpoint() dd_trace(__FILE__, __FUNCTION__, __LINE__)
