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

#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include <memory>

class AssFile;
class Project;
class InitialLineState;
class SelectionController;
class SubsController;
class VideoController;
namespace Automation4 { class ScriptManager; }

namespace agi {
class Path;

struct Context {
	// Note: order here matters quite a bit, as things need to be set up and
	// torn down in the correct order
	std::unique_ptr<AssFile> ass;
	std::unique_ptr<SubsController> subsController;
	std::unique_ptr<Project> project;
	std::unique_ptr<Automation4::ScriptManager> local_scripts;
	std::unique_ptr<SelectionController> selectionController;
	std::unique_ptr<VideoController> videoController;
	std::unique_ptr<InitialLineState> initialLineState;
	std::unique_ptr<Path> path;

	Context();
	~Context();
};

}

#endif // CONTEXT_H_INCLUDED
