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

#include "include/aegisub/video_provider.h"

#include <libaegisub/exception.h>
#include <libaegisub/fs_fwd.h>

#include <atomic>
#include <memory>
#include <set>

class AssDialogue;
class VideoProvider;
class VideoProviderError;
struct AssDialogueBase;
struct VideoFrame;
namespace agi {
	class BackgroundRunner;
	namespace dispatch { class Queue; }
}

/// An asynchronous video decoding and subtitle rendering wrapper
class AsyncVideoProvider {
	/// Asynchronous work queue
	std::unique_ptr<agi::dispatch::Queue> worker;

	/// Video provider
	std::unique_ptr<VideoProvider> source_provider;

	int frame_number = -1; ///< Last frame number requested
	double time = -1.; ///< Time of the frame to pass to the subtitle renderer

	/// If >= 0, the subtitles provider current has just the lines visible on
	/// that frame loaded. If -1, the entire file is loaded. If -2, the
	/// currently loaded file is out of date.
	int single_frame = -1;

	/// Last rendered frame number
	int last_rendered = -1;
	/// Last rendered subtitles on that frame
	std::vector<AssDialogueBase> last_lines;

	/// Monotonic counter used to drop frames when changes arrive faster than
	/// they can be rendered
	std::atomic<uint_fast32_t> version{ 0 };

	std::vector<std::shared_ptr<VideoFrame>> buffers;

public:
	/// Ask the video provider to change YCbCr matricies
	void SetColorSpace(std::string const& matrix);

	int GetFrameCount() const             { return source_provider->GetFrameCount(); }
	int GetWidth() const                  { return source_provider->GetWidth(); }
	int GetHeight() const                 { return source_provider->GetHeight(); }
	double GetDAR() const                 { return source_provider->GetDAR(); }
	agi::vfr::Framerate GetFPS() const    { return source_provider->GetFPS(); }
	std::vector<int> GetKeyFrames() const { return source_provider->GetKeyFrames(); }
	std::string GetColorSpace() const     { return source_provider->GetColorSpace(); }
	std::string GetRealColorSpace() const { return source_provider->GetRealColorSpace(); }
	std::string GetWarning() const        { return source_provider->GetWarning(); }
	std::string GetDecoderName() const    { return source_provider->GetDecoderName(); }
	bool ShouldSetVideoProperties() const { return source_provider->ShouldSetVideoProperties(); }
	bool HasAudio() const                 { return source_provider->HasAudio(); }

	/// @brief Constructor
	/// @param videoFileName File to open
	/// @param parent Event handler to send FrameReady events to
	AsyncVideoProvider(const agi::fs::path& video_filename, const std::string& colormatrix, agi::BackgroundRunner* br);
	~AsyncVideoProvider();
};

/// Event which signals that a requested frame is ready
struct FrameReadyEvent final {
	/// Frame which is ready
	std::shared_ptr<VideoFrame> frame;
	/// Time which was used for subtitle rendering
	double time;
	FrameReadyEvent(std::shared_ptr<VideoFrame> frame, double time)
	: frame(std::move(frame)), time(time) { }
};

struct VideoProviderErrorEvent final : public agi::Exception {
	VideoProviderErrorEvent(VideoProviderError const& err);
};

struct SubtitlesProviderErrorEvent final : public agi::Exception {
	SubtitlesProviderErrorEvent(std::string const& msg);
};
