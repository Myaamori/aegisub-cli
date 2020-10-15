// Copyright (c) 2005-2007, Rodrigo Braz Monteiro
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

#include "video_controller.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "time_range.h"
#include "async_video_provider.h"
#include "utils.h"

#include <libaegisub/log.h>
#include <libaegisub/ass/time.h>

VideoController::VideoController(agi::Context *c)
: context(c)
, playAudioOnStep(OPT_GET("Audio/Plays When Stepping Video"))
, connections(agi::signal::make_vector({
	context->ass->AddCommitListener(&VideoController::OnSubtitlesCommit, this),
	context->project->AddVideoProviderListener(&VideoController::OnNewVideoProvider, this),
	context->selectionController->AddActiveLineListener(&VideoController::OnActiveLineChanged, this),
}))
{
}

void VideoController::OnNewVideoProvider(AsyncVideoProvider *new_provider) {
	provider = new_provider;
	color_matrix = provider ? provider->GetColorSpace() : "";
}

void VideoController::OnSubtitlesCommit(int type, const AssDialogue *changed) {
	if (!provider) return;

	if ((type & AssFile::COMMIT_SCRIPTINFO) || type == AssFile::COMMIT_NEW) {
		auto new_matrix = context->ass->GetScriptInfo("YCbCr Matrix");
		if (!new_matrix.empty() && new_matrix != color_matrix) {
			color_matrix = new_matrix;
			provider->SetColorSpace(new_matrix);
		}
	}
}

void VideoController::OnActiveLineChanged(AssDialogue *line) {
	if (line && provider && OPT_GET("Video/Subtitle Sync")->GetBool()) {
		JumpToTime(line->Start);
	}
}

void VideoController::RequestFrame() {
	context->ass->Properties.video_position = frame_n;
}

void VideoController::JumpToFrame(int n) {
	if (!provider) return;

	frame_n = mid(0, n, provider->GetFrameCount() - 1);
	RequestFrame();
	Seek(frame_n);
}

void VideoController::JumpToTime(int ms, agi::vfr::Time end) {
	JumpToFrame(FrameAtTime(ms, end));
}

double VideoController::GetARFromType(AspectRatio type) const {
	switch (type) {
		case AspectRatio::Default:    return (double)provider->GetWidth()/provider->GetHeight();
		case AspectRatio::Fullscreen: return 4.0/3.0;
		case AspectRatio::Widescreen: return 16.0/9.0;
		case AspectRatio::Cinematic:  return 2.35;
		default: throw agi::InternalError("Bad AR type");
	}
}

void VideoController::SetAspectRatio(double value) {
	ar_type = AspectRatio::Custom;
	ar_value = mid(.5, value, 5.);
	context->ass->Properties.ar_mode = (int)ar_type;
	context->ass->Properties.ar_value = ar_value;
	ARChange(ar_type, ar_value);
}

void VideoController::SetAspectRatio(AspectRatio type) {
	ar_value = mid(.5, GetARFromType(type), 5.);
	ar_type = type;
	context->ass->Properties.ar_mode = (int)ar_type;
	context->ass->Properties.ar_value = ar_value;
	ARChange(ar_type, ar_value);
}

int VideoController::TimeAtFrame(int frame, agi::vfr::Time type) const {
	return context->project->Timecodes().TimeAtFrame(frame, type);
}

int VideoController::FrameAtTime(int time, agi::vfr::Time type) const {
	return context->project->Timecodes().FrameAtTime(time, type);
}

void VideoController::OnVideoError(std::string const& err) {
}

void VideoController::OnSubtitlesError(std::string const& err) {
}
