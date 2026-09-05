// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstdlib>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace evidence {
inline void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
inline void configureErrors() {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}
}
