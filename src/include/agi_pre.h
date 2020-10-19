// Copyright (c) 2006, Rodrigo Braz Monteiro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file agi_pre.h
/// @brief Precompiled headers include file, including all headers that should be precompiled
/// @ingroup main
///
/// In order to use it, set the project to use this header as precompiled and
/// insert it in every source file (under C/C++ -> Advanced -> Force Includes),
/// then set stdwx.cpp to generate the precompiled header
///
/// @note Make sure that you disable use of precompiled headers on md5.c and
///       MatroskaParser.c, as well as any possible future .c files.

#ifdef __cplusplus

// Block msvc from complaining about not using msvc-specific versions for
// insecure C functions.
#ifdef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS_DEFINED
#else
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../acconf.h"

#define WIN32_LEAN_AND_MEAN

// Common C
#include <cassert>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

// Common C++
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef _WIN32
#include <objbase.h>
#include <mmsystem.h>
#else
#include <sys/param.h>
#endif

// Boost
#include <boost/container/list.hpp>
#include <boost/flyweight.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#include <boost/regex.hpp>
#define BOOST_NO_SCOPED_ENUMS
#include <boost/filesystem/path.hpp>
#undef BOOST_NO_SCOPED_ENUMS
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/interprocess/streams/vectorstream.hpp>

#ifndef _CRT_SECURE_NO_WARNINGS_DEFINED
#undef _CRT_SECURE_NO_WARNINGS
#endif

#endif
