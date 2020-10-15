// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "project.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "async_video_provider.h"
#include "charset_detect.h"
#include "dialog_progress.h"
#include "include/aegisub/context.h"
#include "include/aegisub/video_provider.h"
#include "options.h"
#include "selection_controller.h"
#include "subs_controller.h"
#include "utils.h"
#include "video_controller.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/format_path.h>
#include <libaegisub/fs.h>
#include <libaegisub/keyframe.h>
#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/path.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem/operations.hpp>

Project::Project(agi::Context *c) : context(c) {
	OPT_SUB("Provider/Avisynth/Allow Ancient", &Project::ReloadVideo, this);
	OPT_SUB("Provider/Avisynth/Memory Max", &Project::ReloadVideo, this);
	OPT_SUB("Provider/Video/FFmpegSource/Decoding Threads", &Project::ReloadVideo, this);
	OPT_SUB("Provider/Video/FFmpegSource/Unsafe Seeking", &Project::ReloadVideo, this);
	OPT_SUB("Subtitle/Provider", &Project::ReloadVideo, this);
	OPT_SUB("Video/Provider", &Project::ReloadVideo, this);
}

Project::~Project() { }

void Project::UpdateRelativePaths() {
	context->ass->Properties.audio_file     = context->path->MakeRelative(audio_file, "?script").generic_string();
	context->ass->Properties.video_file     = context->path->MakeRelative(video_file, "?script").generic_string();
	context->ass->Properties.timecodes_file = context->path->MakeRelative(timecodes_file, "?script").generic_string();
	context->ass->Properties.keyframes_file = context->path->MakeRelative(keyframes_file, "?script").generic_string();
}

void Project::ReloadVideo() {
	if (video_provider) {
		DoLoadVideo(video_file);
		context->videoController->JumpToFrame(context->videoController->GetFrameN());
	}
}

void Project::ShowError(std::string const& message) {
	LOG_E("agi/project") << message;
}

void Project::SetPath(agi::fs::path& var, const char *token, const char *mru, agi::fs::path const& value) {
	var = value;
	if (*token)
		context->path->SetToken(token, value);
	if (*mru)
		config::mru->Add(mru, value);
	UpdateRelativePaths();
}

bool Project::DoLoadSubtitles(agi::fs::path const& path, std::string encoding, ProjectProperties &properties) {
	try {
		if (encoding.empty())
			encoding = CharSetDetect::GetEncoding(path);
	}
	catch (agi::UserCancelException const&) {
		return false;
	}
	catch (agi::fs::FileNotFound const&) {
		config::mru->Remove("Subtitle", path);
		ShowError(path.string() + " not found.");
		return false;
	}

	if (encoding != "binary") {
		// Try loading as timecodes and keyframes first since we can't
		// distinguish them based on filename alone, and just ignore failures
		// rather than trying to differentiate between malformed timecodes
		// files and things that aren't timecodes files at all
		try { DoLoadTimecodes(path); return false; } catch (...) { }
		try { DoLoadKeyframes(path); return false; } catch (...) { }
	}

	try {
		properties = context->subsController->Load(path, encoding);
	}
	catch (agi::UserCancelException const&) { return false; }
	catch (agi::fs::FileNotFound const&) {
		config::mru->Remove("Subtitle", path);
		ShowError(path.string() + " not found.");
		return false;
	}
	catch (agi::Exception const& e) {
		ShowError(e.GetMessage());
		return false;
	}
	catch (std::exception const& e) {
		ShowError(std::string(e.what()));
		return false;
	}
	catch (...) {
		ShowError("Unknown error");
		return false;
	}

	Selection sel;
	AssDialogue *active_line = nullptr;
	if (!context->ass->Events.empty()) {
		int row = mid<int>(0, properties.active_row, context->ass->Events.size() - 1);
		active_line = &*std::next(context->ass->Events.begin(), row);
		sel.insert(active_line);
	}
	context->selectionController->SetSelectionAndActive(std::move(sel), active_line);

	return true;
}

bool Project::LoadSubtitles(agi::fs::path path, std::string encoding) {
	ProjectProperties properties;
	return DoLoadSubtitles(path, encoding, properties);
}

void Project::CloseSubtitles() {
	context->subsController->Close();
	context->path->SetToken("?script", "");
	auto line = &*context->ass->Events.begin();
	context->selectionController->SetSelectionAndActive({line}, line);
}

bool Project::DoLoadVideo(agi::fs::path const& path) {
	if (!progress)
		progress = new DialogProgress();

	try {
		auto old_matrix = context->ass->GetScriptInfo("YCbCr Matrix");
		video_provider = agi::make_unique<AsyncVideoProvider>(path, old_matrix, progress);
	}
	catch (agi::UserCancelException const&) { return false; }
	catch (agi::fs::FileSystemError const& err) {
		config::mru->Remove("Video", path);
		ShowError(err.GetMessage());
		return false;
	}
	catch (VideoProviderError const& err) {
		ShowError(err.GetMessage());
		return false;
	}

	AnnounceVideoProviderModified(video_provider.get());

	//UpdateVideoProperties(context->ass.get(), video_provider.get());

	timecodes = video_provider->GetFPS();
	keyframes = video_provider->GetKeyFrames();

	timecodes_file.clear();
	keyframes_file.clear();
	SetPath(video_file, "?video", "Video", path);

	std::string warning = video_provider->GetWarning();
	if (!warning.empty())
		LOG_W("agi/project") << warning;

	video_has_subtitles = false;

	AnnounceKeyframesModified(keyframes);
	AnnounceTimecodesModified(timecodes);
	return true;
}

bool Project::LoadVideo(agi::fs::path path) {
	if (path.empty()) return false;
	if (!DoLoadVideo(path)) return false;

	double dar = video_provider->GetDAR();
	if (dar > 0)
		context->videoController->SetAspectRatio(dar);
	else
		context->videoController->SetAspectRatio(AspectRatio::Default);
	context->videoController->JumpToFrame(0);
	return true;
}

void Project::CloseVideo() {
	AnnounceVideoProviderModified(nullptr);
	video_provider.reset();
	SetPath(video_file, "?video", "", "");
	video_has_subtitles = false;
	context->ass->Properties.ar_mode = 0;
	context->ass->Properties.ar_value = 0.0;
	context->ass->Properties.video_position = 0;
}

void Project::DoLoadTimecodes(agi::fs::path const& path) {
	timecodes = agi::vfr::Framerate(path);
	SetPath(timecodes_file, "", "Timecodes", path);
	AnnounceTimecodesModified(timecodes);
}

bool Project::LoadTimecodes(agi::fs::path path) {
	try {
		DoLoadTimecodes(path);
		return true;
	}
	catch (agi::fs::FileSystemError const& e) {
		ShowError(e.GetMessage());
		config::mru->Remove("Timecodes", path);
	}
	catch (agi::vfr::Error const& e) {
		ShowError("Failed to parse timecodes file: " + e.GetMessage());
		config::mru->Remove("Timecodes", path);
	}
	return false;
}

void Project::CloseTimecodes() {
	timecodes = video_provider ? video_provider->GetFPS() : agi::vfr::Framerate{};
	SetPath(timecodes_file, "", "", "");
	AnnounceTimecodesModified(timecodes);
}

void Project::DoLoadKeyframes(agi::fs::path const& path) {
	keyframes = agi::keyframe::Load(path);
	SetPath(keyframes_file, "", "Keyframes", path);
	AnnounceKeyframesModified(keyframes);
}

bool Project::LoadKeyframes(agi::fs::path path) {
	try {
		DoLoadKeyframes(path);
		return true;
	}
	catch (agi::fs::FileSystemError const& e) {
		ShowError(e.GetMessage());
		config::mru->Remove("Keyframes", path);
	}
	catch (agi::keyframe::KeyframeFormatParseError const& e) {
		ShowError("Failed to parse keyframes file: " + e.GetMessage());
		config::mru->Remove("Keyframes", path);
	}
	catch (agi::keyframe::UnknownKeyframeFormatError const& e) {
		ShowError("Keyframes file in unknown format: " + e.GetMessage());
		config::mru->Remove("Keyframes", path);
	}
	return false;
}

void Project::CloseKeyframes() {
	keyframes = video_provider ? video_provider->GetKeyFrames() : std::vector<int>{};
	SetPath(keyframes_file, "", "", "");
	AnnounceKeyframesModified(keyframes);
}

void Project::LoadList(std::vector<agi::fs::path> const& files) {
	// Keep these lists sorted

	// Video formats
	const char *videoList[] = {
		".asf",
		".avi",
		".avs",
		".d2v",
		".m2ts",
		".m4v",
		".mkv",
		".mov",
		".mp4",
		".mpeg",
		".mpg",
		".ogm",
		".rm",
		".rmvb",
		".ts",
		".webm"
		".wmv",
		".y4m",
		".yuv"
	};

	// Subtitle formats
	const char *subsList[] = {
		".ass",
		".srt",
		".ssa",
		".sub",
		".ttxt"
	};

	// Audio formats
	const char *audioList[] = {
		".aac",
		".ac3",
		".ape",
		".dts",
		".flac",
		".m4a",
		".mka",
		".mp3",
		".ogg",
		".w64",
		".wav",
		".wma"
	};

	auto search = [](const char **begin, const char **end, std::string const& str) {
		return std::binary_search(begin, end, str.c_str(), [](const char *a, const char *b) {
			return strcmp(a, b) < 0;
		});
	};

	agi::fs::path audio, video, subs, timecodes, keyframes;
	for (auto file : files) {
		if (file.is_relative()) file = absolute(file);
		if (!agi::fs::FileExists(file)) continue;

		auto ext = file.extension().string();
		boost::to_lower(ext);

		// Could be subtitles, keyframes or timecodes, so try loading as each
		if (ext == ".txt" || ext == ".log") {
			if (timecodes.empty()) {
				try {
					DoLoadTimecodes(file);
					timecodes = file;
					continue;
				} catch (...) { }
			}

			if (keyframes.empty()) {
				try {
					DoLoadKeyframes(file);
					keyframes = file;
					continue;
				} catch (...) { }
			}

			if (subs.empty() && ext != ".log")
				subs = file;
			continue;
		}

		if (subs.empty() && search(std::begin(subsList), std::end(subsList), ext))
			subs = file;
		if (video.empty() && search(std::begin(videoList), std::end(videoList), ext))
			video = file;
		if (audio.empty() && search(std::begin(audioList), std::end(audioList), ext))
			audio = file;
	}

	ProjectProperties properties;
	if (!subs.empty()) {
		if (!DoLoadSubtitles(subs, "", properties))
			subs.clear();
	}

	if (!video.empty() && DoLoadVideo(video)) {
		double dar = video_provider->GetDAR();
		if (dar > 0)
			context->videoController->SetAspectRatio(dar);
		else
			context->videoController->SetAspectRatio(AspectRatio::Default);
		context->videoController->JumpToFrame(0);

		// We loaded these earlier, but loading video unloaded them
		// Non-Do version of Load in case they've vanished or changed between
		// then and now
		if (!timecodes.empty())
			LoadTimecodes(timecodes);
		if (!keyframes.empty())
			LoadKeyframes(keyframes);
	}
}
