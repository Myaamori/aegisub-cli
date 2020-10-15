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

#include "async_video_provider.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "export_fixstyle.h"
#include "video_frame.h"
#include "video_provider_manager.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/log.h>

enum {
	NEW_SUBS_FILE = -1,
	SUBS_FILE_ALREADY_LOADED = -2
};

AsyncVideoProvider::AsyncVideoProvider(agi::fs::path const& video_filename, std::string const& colormatrix, agi::BackgroundRunner *br)
: worker(agi::dispatch::Create())
, source_provider(VideoProviderFactory::GetProvider(video_filename, colormatrix, br))
{
}

AsyncVideoProvider::~AsyncVideoProvider() {
	// Block until all currently queued jobs are complete
	worker->Sync([]{});
}

void AsyncVideoProvider::SetColorSpace(std::string const& matrix) {
	worker->Async([=] { source_provider->SetColorSpace(matrix); });
}

VideoProviderErrorEvent::VideoProviderErrorEvent(VideoProviderError const& err)
: agi::Exception(err.GetMessage())
{
	LOG_E("agi/async_video_provider") << (
		"Failed seeking video. The video file may be corrupt or incomplete.\n"
		"Error message reported: ") << err.GetMessage();
}
SubtitlesProviderErrorEvent::SubtitlesProviderErrorEvent(std::string const& err)
: agi::Exception(err)
{
	LOG_E("agi/async_video_provider") << "Failed rendering subtitles. Error message reported: " << err;
}
