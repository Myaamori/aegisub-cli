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

#ifndef SUBS_CONTROLLER_H_INCLUDED
#define SUBS_CONTROLLER_H_INCLUDED

#include <libaegisub/fs_fwd.h>
#include <libaegisub/signal.h>

#include <boost/container/list.hpp>
#include <boost/filesystem/path.hpp>

namespace agi {
	namespace dispatch {
		class Queue;
	}
	struct Context;
}
struct AssFileCommit;
struct ProjectProperties;

class SubsController {
	agi::Context *context;

	/// Revision counter for undo coalescing and modified state tracking
	int commit_id = 0;
	/// Last saved version of this file
	int saved_commit_id = 0;
	/// Version to use for the next commit
	/// Needed to handle Save -> Undo -> Edit, which would result in the file
	/// being marked unmodified if we reused commit IDs
	int next_commit_id = 1;

	/// A new file has been opened (filename)
	agi::signal::Signal<agi::fs::path> FileOpen;
	/// The file has been saved
	agi::signal::Signal<> FileSave;

	/// The filename of the currently open file, if any
	agi::fs::path filename;

	/// Set the filename, updating things like the MRU and last used path
	void SetFileName(agi::fs::path const& file);

	/// Autosave the file if there have been any chances since the last autosave
	void AutoSave();

public:
	SubsController(agi::Context *context);
	~SubsController();

	/// The file's path and filename if any, or platform-appropriate "untitled"
	agi::fs::path Filename() const;

	/// @brief Load from a file
	/// @param file File name
	/// @param charset Character set of file
	ProjectProperties Load(agi::fs::path const& file, std::string charset);

	/// @brief Save to a file
	/// @param file Path to save to
	/// @param encoding Encoding to use, or empty to let the writer decide (which usually means "App/Save Charset")
	void Save(agi::fs::path const& file, std::string const& encoding="");

	/// Close the currently open file (i.e. open a new blank file)
	void Close();

	/// Can the file be saved in its current format?
	bool CanSave() const;

	/// The file is about to be saved
	/// This signal is intended for adding metadata which is awkward or
	/// expensive to always keep up to date
	agi::signal::Signal<> UpdateProperties;

	DEFINE_SIGNAL_ADDERS(FileOpen, AddFileOpenListener)
	DEFINE_SIGNAL_ADDERS(FileSave, AddFileSaveListener)

};

#endif // SUBS_CONTROLLER_H_INCLUDED
