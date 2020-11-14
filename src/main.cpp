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
#include <libaegisub/json.h>
#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/option.h>
#include <libaegisub/path.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>

#ifndef WIN32
#include <wx/app.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/locale.hpp>
#include <boost/program_options.hpp>
#include <locale>
#include "string_codec.h"

#define StartupLog(a) LOG_D("main") << a
#define StartupError(a) LOG_E("main") << a

namespace config {
	agi::Options *opt = nullptr;
	agi::MRUManager *mru = nullptr;
	agi::Path *path = nullptr;
	std::list<std::pair<int, std::string>> *dialog_responses;
	std::list<std::vector<agi::fs::path>> *file_responses;
}

std::set<int> parse_range(const std::string& s) {
	std::set<int> lines;
	for (auto tok : agi::Split(s, ',')) {
		auto item = agi::str(tok);
		int i;

		if (boost::conversion::try_lexical_convert(item, i)) {
			lines.insert(i);
		} else {
			auto sep = item.find('-');
			if (sep == std::string::npos) {
				throw agi::InvalidInputException("Invalid number or range: " + item);
			}

			if (!boost::conversion::try_lexical_convert(item.substr(0, sep), i)) {
				throw agi::InvalidInputException("Invalid start number in range: " + item);
			}

			int j;
			if (!boost::conversion::try_lexical_convert(item.substr(sep + 1), j)) {
				throw agi::InvalidInputException("Invalid end number in range: " + item);
			}

			for (int k = i; k <= j; k++) {
				lines.insert(k);
			}
		}
	}
	return lines;
}

std::unique_ptr<Automation4::Script> find_script(const std::string& file)
{
	auto absolute = agi::fs::path(file);
	auto relative = boost::filesystem::current_path() / file;

	agi::fs::path script;

	if (agi::fs::FileExists(absolute)) {
		script = absolute;
	} else if (agi::fs::FileExists(relative)) {
		script = relative;
	} else {
		auto autodirs = OPT_GET("Path/Automation/Autoload")->GetString();

		for (auto tok : agi::Split(autodirs, '|')) {
			auto dirname = config::path->Decode(agi::str(tok));
			if (!agi::fs::DirectoryExists(dirname)) continue;

			auto scriptname = dirname / file;
			if (agi::fs::FileExists(scriptname)) {
				script = scriptname;
			}
		}
	}

	if (script.empty()) {
		throw agi::InvalidInputException("Could not find script file: " + file);
	}

	return Automation4::ScriptFactory::CreateFromFile(script, true, false);
}

std::string serialize_element(const json::UnknownElement& elem) {
	try {
		auto& val_str = static_cast<json::String const&>(elem);
		return inline_string_encode(val_str);
	}
	catch (json::Exception const& e) {}

	try {
		auto& val_int = static_cast<json::Integer const&>(elem);
		return std::to_string(val_int);
	}
	catch (json::Exception const& e) {}

	try {
		auto& val_double = static_cast<json::Double const&>(elem);
		return std::to_string(val_double);
	}
	catch (json::Exception const& e) {}

	try {
		auto& val_bool = static_cast<json::Boolean const&>(elem);
		return val_bool ? "1" : "0";
	} catch (json::Exception const& e) {}

	auto& val_array = static_cast<json::Array const&>(elem);
	if (val_array.size() != 3 && val_array.size() != 4) {
		StartupError("Wrong array length for color");
		throw json::Exception("Wrong array length for color");
	}

	agi::Color color;
	for (int i = 0; i < val_array.size(); i++) {
		int arrelem_int = static_cast<json::Integer const&>(val_array[i]);
		if (arrelem_int < 0 || arrelem_int >= 256) {
			StartupError("Color part not within [0,255] bounds");
			throw json::Exception("Color part not within [0,255] bounds");
		}

		switch (i) {
			case 0: color.r = arrelem_int; break;
			case 1: color.g = arrelem_int; break;
			case 2: color.b = arrelem_int; break;
			case 3: color.a = arrelem_int; break;
		}
	}
	return color.GetHexFormatted(val_array.size() == 4);
}

std::pair<int, std::string> json_to_lua(const std::string& s) {
	std::istringstream stream(s);
	auto elem = agi::json_util::parse(stream);
	auto& root = static_cast<json::Object const&>(elem);
	auto button = root.find("button");
	if (button == root.end()) {
		throw agi::InvalidInputException("No button specified in JSON: " + s);
	}

	const json::Integer* button_num = nullptr;
	try {
		button_num = &static_cast<json::Integer const&>(button->second);
	}
	catch (json::Exception const& e) {
		throw agi::InvalidInputException("'button' not an integer in JSON: " + s);
	}

	auto values = root.find("values");
	if (values == root.end()) {
		return std::make_pair(*button_num, "");
	}

	const json::Object* values_obj = nullptr;
	try {
		values_obj = &static_cast<json::Object const&>(values->second);
	}
	catch (json::Exception const& e) {
		throw agi::InvalidInputException("'values' not an object in JSON: " + s);
	}

	std::string vals = "";
	for (auto& p : *values_obj) {
		if (!vals.empty()) {
			vals += "|";
		}

		auto& key = p.first;
		try {
			vals += inline_string_encode(key) + ":" + serialize_element(p.second);
		}
		catch (json::Exception const& e) {
			throw agi::InvalidInputException(agi::format("Could not serialize 'values.%s' in JSON: %s", key, s));
		}
	}

	return std::make_pair(*button_num, vals);
}

int main(int argc, char **argv) {
	boost::program_options::options_description cmdline("Options");
	boost::program_options::options_description flags("Options");
	boost::program_options::positional_options_description posdesc;

	cmdline.add_options()
		("in-file", boost::program_options::value<std::string>(), "input file")
		("out-file", boost::program_options::value<std::string>(), "output file")
		("macro", boost::program_options::value<std::string>(), "macro to run")
	;

	flags.add_options()
		("help", "produce help message")
		("video", boost::program_options::value<std::string>(), "video to load")
		("timecodes", boost::program_options::value<std::string>(), "timecodes to load")
		("keyframes", boost::program_options::value<std::string>(), "keyframes to load")
		("automation", boost::program_options::value<std::vector<std::string>>(), "an automation script to run")
		("active-line", boost::program_options::value<int>()->default_value(-1), "the active line")
		("selected-lines", boost::program_options::value<std::string>()->default_value(""), "the selected lines")
		("dialog", boost::program_options::value<std::vector<std::string>>(), "response to a dialog, in JSON")
		("file", boost::program_options::value<std::vector<std::string>>(), "filename to supply to an open/save call")
		("loglevel", boost::program_options::value<int>()->default_value(4), "0 = exception; 1 = assert; 2 = warning; 3 = info; 4 = debug")
	;

	cmdline.add(flags);
	posdesc.add("in-file", 1);
	posdesc.add("out-file", 1);
	posdesc.add("macro", 1);
	boost::program_options::variables_map vm;
	boost::program_options::store(
		boost::program_options::command_line_parser(argc, argv).
		options(cmdline).positional(posdesc).run(), vm);
	boost::program_options::notify(vm);

	if (vm.count("help") || !vm.count("macro")) {
		if (!vm.count("macro")) {
			std::cout << "Too few arguments." << std::endl;
		}
		std::cout << argv[0] << " [options] <input file> <output file> <macro>" << std::endl;
		std::cout << flags << std::endl;
		return 1;
	}


#ifndef WIN32
	wxApp::SetInstance(new wxApp());
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
	config::dialog_responses = new std::list<std::pair<int, std::string>>;
	config::file_responses = new std::list<std::vector<agi::fs::path>>;

	agi::log::log = new agi::log::LogSink;
	agi::log::log->Subscribe(agi::make_unique<agi::log::EmitSTDOUT>(vm["loglevel"].as<int>()));

	// Set config file
	StartupLog("Load local configuration");
#ifdef WIN32
	// Try loading configuration from the install dir if one exists there
	try {
		auto conf_local(config::path->Decode("?data/config.json"));
		std::unique_ptr<std::istream> localConfig(agi::io::Open(conf_local));
		config::opt = new agi::Options(conf_local, GET_DEFAULT_CONFIG(default_config), agi::Options::FLUSH_SKIP);

		// Local config, make ?user mean ?data so all user settings are placed in install dir
		config::path->SetToken("?user", config::path->Decode("?data"));
		config::path->SetToken("?local", config::path->Decode("?data"));
	} catch (agi::fs::FileSystemError const&) {
		// File doesn't exist or we can't read it
		// Might be worth displaying an error in the second case
	}
#endif

	StartupLog("Load user configuration");
	try {
		if (!config::opt)
			config::opt = new agi::Options(config::path->Decode("?user/config.json"), GET_DEFAULT_CONFIG(default_config), agi::Options::FLUSH_SKIP);
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

		auto context = agi::make_unique<agi::Context>();

		// make sure to load files before loading automation,
		// since automations can change the cwd
		StartupLog("Loading subtitles...");
		if (!context->project->LoadSubtitles(
				boost::filesystem::absolute(vm["in-file"].as<std::string>()))) {
			return 2;
		}

		if (vm.count("video")) {
			StartupLog("Loading video...");
			if (!context->project->LoadVideo(
					boost::filesystem::absolute(vm["video"].as<std::string>()))) {
				return 2;
			}
		}

		if (vm.count("timecodes")) {
			StartupLog("Loading timecodes...");
			if (!context->project->LoadTimecodes(
					boost::filesystem::absolute(vm["timecodes"].as<std::string>()))) {
				return 2;
			}
		}

		if (vm.count("keyframes")) {
			StartupLog("Loading keyframes...");
			if (!context->project->LoadKeyframes(
					boost::filesystem::absolute(vm["keyframes"].as<std::string>()))) {
				return 2;
			}
		}

		auto active_index = vm["active-line"].as<int>();
		AssDialogue* active_line = nullptr;

		auto selected_indices = parse_range(vm["selected-lines"].as<std::string>());
		Selection selected_lines;

		int i = 0;
		for (auto& line : context->ass->Events) {
			if (i == active_index) {
				active_line = &line;
			}

			if (selected_indices.empty() || selected_indices.count(i)) {
				selected_lines.insert(&line);
				if (active_line == nullptr) {
					// assign first line in selection as a fallback
					active_line = &line;
				}
			}
			i++;
		}

		if (active_line == nullptr) {
			// selection was empty
			active_line = &context->ass->Events.front();
			selected_lines.insert(active_line);
		}

		context->selectionController->SetSelectionAndActive(
			std::move(selected_lines), active_line);

		// Load plugins
		Automation4::ScriptFactory::Register(agi::make_unique<Automation4::LuaScriptFactory>());

		if (vm.count("dialog")) {
			for (auto& s : vm["dialog"].as<std::vector<std::string>>()) {
				auto pair = json_to_lua(s);
				StartupLog(agi::format("Dialog response: button %d -> %s", pair.first, pair.second));
				config::dialog_responses->push_back(std::move(pair));
			}
		}

		if (vm.count("file")) {
			for (auto& s : vm["file"].as<std::vector<std::string>>()) {
				std::vector<agi::fs::path> paths;
				std::stringstream ss;
				for (const auto& tok : agi::Split(s, '|')) {
					auto p = boost::filesystem::absolute(agi::str(tok));
					ss << p << " ";
					paths.push_back(std::move(p));
				}
				StartupLog("File response: ") << ss.str();
				config::file_responses->push_back(std::move(paths));
			}
		}

		// Load Automation scripts
		StartupLog("Load automation script");
		// cache cwd in case automation changes it
		auto cwd = boost::filesystem::current_path();

		std::vector<std::unique_ptr<Automation4::Script>> scripts;
		if (vm.count("automation")) {
			for (auto& s : vm["automation"].as<std::vector<std::string>>()) {
				StartupLog("Loading ") << s;
				auto script = find_script(s);
				if (!script) {
					return 1;
				}
				scripts.emplace_back(std::move(script));
			}
		}

		auto macro = vm["macro"].as<std::string>();
		StartupLog("Calling: ") << macro;
		if (!cmd::call(macro, context.get())) {
			StartupError("Skipping automation because validation function returned false");
			return 1;
		}

		// restore cwd for saving
		boost::filesystem::current_path(cwd);
		context->subsController->Save(vm["out-file"].as<std::string>());
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
	delete config::dialog_responses;
	delete config::file_responses;
	cmd::clear();
	delete agi::log::log;
	
#ifndef WIN32
	wxEntryCleanup();
#endif
	return 0;
}
