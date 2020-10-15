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

#include "subs_controller.h"

#include "ass_attachment.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_info.h"
#include "ass_style.h"
#include "command/command.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "subtitle_format.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/format_path.h>
#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/util.h>

SubsController::SubsController(agi::Context *context)
: context(context)
{
}

SubsController::~SubsController() {
}

ProjectProperties SubsController::Load(agi::fs::path const& filename, std::string charset) {
	AssFile temp;

	SubtitleFormat::GetReader(filename, charset)->ReadFile(&temp, filename, context->project->Timecodes(), charset);

	context->ass->swap(temp);
	auto props = context->ass->Properties;

	SetFileName(filename);

	// Push the initial state of the file onto the undo stack
	context->ass->Commit(AssFile::COMMIT_NEW);

	FileOpen(filename);
	return props;
}

void SubsController::Save(agi::fs::path const& filename, std::string const& encoding) {
	const SubtitleFormat *writer = SubtitleFormat::GetWriter(filename);
	if (!writer)
		throw agi::InvalidInputException("Unknown file type.");

	int old_saved_commit_id = saved_commit_id;
	try {
		saved_commit_id = commit_id;

		// Have to set this now for the sake of things that want to save paths
		// relative to the script in the header
		this->filename = filename;
		context->path->SetToken("?script", filename.parent_path());

		context->ass->CleanExtradata();
		writer->WriteFile(context->ass.get(), filename, 0, encoding);
		FileSave();
	}
	catch (...) {
		saved_commit_id = old_saved_commit_id;
		throw;
	}

	SetFileName(filename);
}

void SubsController::Close() {
	saved_commit_id = commit_id + 1;
	filename.clear();
	AssFile blank;
	blank.swap(*context->ass);
	context->ass->LoadDefault(true);
	context->ass->Commit(AssFile::COMMIT_NEW);
	FileOpen(filename);
}

bool SubsController::CanSave() const {
	try {
		return SubtitleFormat::GetWriter(filename)->CanSave(context->ass.get());
	}
	catch (...) {
		return false;
	}
}

void SubsController::SetFileName(agi::fs::path const& path) {
	filename = path;
	context->path->SetToken("?script", path.parent_path());
	config::mru->Add("Subtitle", path);
	OPT_SET("Path/Last/Subtitles")->SetString(filename.parent_path().string());
}

agi::fs::path SubsController::Filename() const {
	if (!filename.empty()) return filename;

	// Apple HIG says "untitled" should not be capitalised
#ifndef __WXMAC__
	return "Untitled";
#else
	return "untitled";
#endif
}
