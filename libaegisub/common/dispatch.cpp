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

#include "libaegisub/dispatch.h"

#include "libaegisub/util.h"

namespace {
	std::function<void (agi::dispatch::Thunk)> invoke_main;

	class MainQueue final : public agi::dispatch::Queue {
		void DoInvoke(agi::dispatch::Thunk thunk) override {
			invoke_main(thunk);
		}
	};

	class BackgroundQueue final : public agi::dispatch::Queue {
		void DoInvoke(agi::dispatch::Thunk thunk) override {
			invoke_main(thunk);
		}
	};

	class SerialQueue final : public agi::dispatch::Queue {
		void DoInvoke(agi::dispatch::Thunk thunk) override {
			thunk();
		}
	public:
		SerialQueue() { }
	};
}

namespace agi { namespace dispatch {

void Init(std::function<void (Thunk)> invoke_main) {
	::invoke_main = invoke_main;
}

void Queue::Async(Thunk thunk) {
	DoInvoke([=] {
		try {
			thunk();
		}
		catch (...) {
			auto e = std::current_exception();
			invoke_main([=] { std::rethrow_exception(e); });
		}
	});
}

void Queue::Sync(Thunk thunk) {
	std::exception_ptr e;
	bool done = false;
	DoInvoke([&]{
		try {
			thunk();
		}
		catch (...) {
			e = std::current_exception();
		}
		done = true;
	});
	if (e) std::rethrow_exception(e);
}

Queue& Main() {
	static MainQueue q;
	return q;
}

Queue& Background() {
	static BackgroundQueue q;
	return q;
}

std::unique_ptr<Queue> Create() {
	return std::unique_ptr<Queue>(new SerialQueue);
}

} }
