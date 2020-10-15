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

/// @file dialog_progress.cpp
/// @brief Progress-bar dialog box for displaying during long operations
/// @ingroup utility
///

#include "dialog_progress.h"

#include "utils.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/log.h>

#include <atomic>

using agi::dispatch::Main;

class DialogProgressSink final : public agi::ProgressSink {
	DialogProgress *dialog;
	std::atomic<bool> cancelled{false};
	int progress = 0;

public:
	DialogProgressSink(DialogProgress *dialog) : dialog(dialog) { }

	void SetTitle(std::string const& title) override {
		LOG_I("agi/dialog_progress") << title;
	}

	void SetMessage(std::string const& msg) override {
		LOG_I("agi/dialog_progress") << msg;
	}

	void SetProgress(int64_t cur, int64_t max) override {
		int new_progress = mid<int>(0, double(cur) / max * 300, 300);
		if (new_progress != progress) {
			progress = new_progress;
			LOG_I("agi/dialog_progress") << "Progress: " << new_progress / 3 << "%";
		}
	}

	void Log(std::string const& str) override {
		LOG_I("agi/dialog_progress") << str;
	}

	bool IsCancelled() override {
		return false;
	}

	void SetIndeterminate() override {
	}
};

DialogProgress::DialogProgress(const std::string& title, const std::string& message)
{
	LOG_I("agi/dialog_progress") << title;
	LOG_I("agi/dialog_progress") << message;
}

void DialogProgress::Run(std::function<void(agi::ProgressSink*)> task) {
	DialogProgressSink ps(this);
	this->ps = &ps;

	agi::dispatch::Background().Async([=]{
		try {
			task(this->ps);
		}
		catch (agi::Exception const& e) {
			this->ps->Log(e.GetMessage());
		}
	});
}

void DialogProgress::SetProgress(int target) {
	if (target == progress_target) return;
	using namespace std::chrono;

	progress_anim_start_value = progress_current;
	auto now = steady_clock::now();
	if (progress_target == 0)
		progress_anim_duration = 1000;
	else
		progress_anim_duration = std::max<int>(100, duration_cast<milliseconds>(now - progress_anim_start_time).count() * 11 / 10);
	progress_anim_start_time = now;
	progress_target = target;
}
