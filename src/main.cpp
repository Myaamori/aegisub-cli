// Copyright (c) 2005, Rodrigo Braz Monteiro
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

/// @file main.cpp
/// @brief Main entry point, as well as crash handling
/// @ingroup main
///

#include "main.h"

#include "command/command.h"

#include "aegisublocale.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "auto4_base.h"
#include "auto4_lua_factory.h"
#include "include/aegisub/context.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "subs_controller.h"
#include "utils.h"
#include "version.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/format_path.h>
#include <libaegisub/fs.h>
#include <libaegisub/io.h>
#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/option.h>
#include <libaegisub/path.h>
#include <libaegisub/util.h>

#ifndef WIN32
#include <wx/app.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/locale.hpp>
#include <boost/program_options.hpp>
#include <locale>

#define StartupLog(a) LOG_I("main") << a
#define StartupError(a) LOG_E("main") << a

namespace config {
	agi::Options *opt = nullptr;
	agi::MRUManager *mru = nullptr;
	agi::Path *path = nullptr;
	Automation4::AutoloadScriptManager *global_scripts;
}

static const char *LastStartupState = nullptr;

int main(int argc, char **argv) {
#ifndef WIN32
	wxApp *app = new wxApp();
	wxApp::SetInstance(app);
	wxEntryStart(argc, argv);
#endif
	
	{
		// Try to get the UTF-8 version of the current locale
		auto locale = boost::locale::generator().generate("");

		// Check if we actually got a UTF-8 locale
		using codecvt = std::codecvt<wchar_t, char, std::mbstate_t>;
		int result = std::codecvt_base::error;
		if (std::has_facet<codecvt>(locale)) {
			wchar_t test[] = L"\xFFFE";
			char buff[8];
			auto mb = std::mbstate_t();
			const wchar_t* from_next;
			char* to_next;
			result = std::use_facet<codecvt>(locale).out(mb,
				test, std::end(test), from_next,
				buff, std::end(buff), to_next);
		}

		// If we didn't get a UTF-8 locale, force it to a known one
		if (result != std::codecvt_base::ok)
			locale = boost::locale::generator().generate("en_US.UTF-8");
		std::locale::global(locale);
	}

	boost::filesystem::path::imbue(std::locale());
	agi::dispatch::Init([](agi::dispatch::Thunk f) {
		f();
	});

	config::path = new agi::Path;

	agi::log::log = new agi::log::LogSink;
	agi::log::log->Subscribe(agi::make_unique<agi::log::EmitSTDOUT>());

	// Set config file
	StartupLog("Load local configuration");
#ifdef WIN32
	// Try loading configuration from the install dir if one exists there
	try {
		auto conf_local(config::path->Decode("?data/config.json"));
		std::unique_ptr<std::istream> localConfig(agi::io::Open(conf_local));
		config::opt = new agi::Options(conf_local, GET_DEFAULT_CONFIG(default_config));

		// Local config, make ?user mean ?data so all user settings are placed in install dir
		config::path->SetToken("?user", config::path->Decode("?data"));
		config::path->SetToken("?local", config::path->Decode("?data"));
		crash_writer::Initialize(config::path->Decode("?user"));
	} catch (agi::fs::FileSystemError const&) {
		// File doesn't exist or we can't read it
		// Might be worth displaying an error in the second case
	}
#endif

	StartupLog("Load user configuration");
	try {
		if (!config::opt)
			config::opt = new agi::Options(config::path->Decode("?user/config.json"), GET_DEFAULT_CONFIG(default_config));
		boost::interprocess::ibufferstream stream((const char *)default_config_platform, sizeof(default_config_platform));
		config::opt->ConfigNext(stream);
	} catch (agi::Exception& e) {
		LOG_E("config/init") << "Caught exception: " << e.GetMessage();
	}

	try {
		config::opt->ConfigUser();
	}
	catch (agi::Exception const& err) {
		StartupLog("Configuration file is invalid. Error reported:\n") << err.GetMessage();
	}

#ifdef _WIN32
	StartupLog("Load installer configuration");
	if (OPT_GET("App/First Start")->GetBool()) {
		try {
			auto installer_config = agi::io::Open(config::path->Decode("?data/installer_config.json"));
			config::opt->ConfigNext(*installer_config.get());
		} catch (agi::fs::FileSystemError const&) {
			// Not an error obviously as the user may not have used the installer
		}
	}
#endif

	// Init commands.
	cmd::init_builtin_commands();

	StartupLog("Load MRU");
	config::mru = new agi::MRUManager(config::path->Decode("?user/mru.json"), GET_DEFAULT_CONFIG(default_mru), config::opt);

	agi::util::SetThreadName("AegiMain");

	AegisubLocale locale;
	StartupLog("Inside OnInit");
	
	StartupLog("CWD: ") << boost::filesystem::current_path();
	try {
		// Initialize randomizer
		StartupLog("Initialize random generator");
		srand(time(nullptr));

		// locale for loading options
		StartupLog("Set initial locale");
		setlocale(LC_NUMERIC, "C");
		setlocale(LC_CTYPE, "C");

		StartupLog("Store options back");
		OPT_SET("Version/Last Version")->SetInt(GetSVNRevision());

		StartupLog("Initialize final locale");

		// Set locale
		locale.Init("en_US");

#ifdef __APPLE__
		// When run from an app bundle, LC_CTYPE defaults to "C", which breaks on
		// anything involving unicode and in some cases number formatting.
		// The right thing to do here would be to query CoreFoundation for the user's
		// locale and add .UTF-8 to that, but :effort:
		setlocale(LC_CTYPE, "en_US.UTF-8");
#endif
		
		// Load plugins
		Automation4::ScriptFactory::Register(agi::make_unique<Automation4::LuaScriptFactory>());

		// Load Automation scripts
		StartupLog("Load global Automation scripts");
		config::global_scripts = new Automation4::AutoloadScriptManager(OPT_GET("Path/Automation/Autoload")->GetString());
		
		auto context = agi::make_unique<agi::Context>();
		
		StartupLog("Parse command line");
		
		// Get parameter subs
		/*
		if (argc >= 3) {
			context->subsController->Load(argv[1], "UTF-8");
			auto& line = context->ass->Events.front();
			context->selectionController->SetActiveLine(&line);
			context->selectionController->SetSelectedSet(Selection { &line });
			
			for (int i = 3; i < argc; i++) {
				StartupLog("Calling: ") << argv[i];
				cmd::call(argv[i], context.get());
			}
			
			context->subsController->Save(argv[2]);
		}*/
	}
	catch (agi::Exception const& e) {
		StartupError("Fatal error while initializing: ") << e.GetMessage();
		return 1;
	}
	catch (std::exception const& e) {
		StartupError("Fatal error while initializing: ") << e.what();
		return 1;
	}
#ifndef _DEBUG
	catch (...) {
		StartupError("Unknown fatal error while initializing");
		return 1;
	}
#endif

	StartupLog("Initialization complete");
	
	delete config::opt;
	delete config::mru;
	cmd::clear();
	delete config::global_scripts;
	delete agi::log::log;
	
#ifndef WIN32
	wxEntryCleanup();
	delete app;
#endif
	return 0;
}
