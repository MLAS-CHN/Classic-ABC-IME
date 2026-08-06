// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <SDKDDKVer.h>

#define NOIME

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#ifndef NOMINMAX
#define NOMINMAX                        // avoid min/max macros clashing with std::min/std::max
#endif
#include <windows.h>
#include <shellapi.h>

#include "immdev.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
