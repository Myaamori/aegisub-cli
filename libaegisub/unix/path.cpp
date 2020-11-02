// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include <libaegisub/path.h>

#include <libaegisub/exception.h>
#include <libaegisub/util_osx.h>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem/operations.hpp>
#include <pwd.h>

namespace {
std::string home_dir() {
	const char *env = getenv("HOME");
	if (env) return env;

#ifndef __APPLE__
	if ((env = getenv("USER")) || (env = getenv("LOGNAME"))) {
		if (passwd *user_info = getpwnam(env))
			return user_info->pw_dir;
	}
#endif

	throw agi::EnvironmentError("Could not get home directory. Make sure HOME is set.");
}
}

namespace agi {
void Path::FillPlatformSpecificPaths() {
	agi::fs::path home = home_dir();
	agi::fs::path app_loc = boost::dll::program_location();
#ifdef P_PORTABLE
	SetToken("?data", app_loc.parent_path());
#else
	SetToken("?data", P_DATA);
#endif

#ifndef __APPLE__
	SetToken("?user", home/".aegisub");
	SetToken("?local", home/".aegisub");
#else
	SetToken("?user", home/"Library/Application Support/Aegisub");
	SetToken("?local", home/"Library/Application Support/Aegisub");
#endif
	SetToken("?temp", boost::filesystem::temp_directory_path());
}

}
