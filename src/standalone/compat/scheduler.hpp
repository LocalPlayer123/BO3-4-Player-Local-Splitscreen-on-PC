#pragma once

// BOIII's scheduler interface, with the same semantics (runtime.cpp).
// The mod runs two pipelines, the two the component uses:
//   async     a background thread, ticking every 10 ms
//   renderer  the game's render thread, once per frame (R_EndFrame)
// build.ps1 refuses to build a component that schedules on any other pipeline.

#include <chrono>
#include <functional>

namespace scheduler
{
	enum pipeline
	{
		async = 0,
		renderer,
		server,
		main,
		dvars_flags_patched,
		dvars_loaded,
		count,
	};

	static const bool cond_continue = false;
	static const bool cond_end = true;

	void schedule(const std::function<bool()>& callback, pipeline type,
	              std::chrono::milliseconds delay = std::chrono::milliseconds(0));
	void loop(const std::function<void()>& callback, pipeline type,
	          std::chrono::milliseconds delay = std::chrono::milliseconds(0));
	void once(const std::function<void()>& callback, pipeline type,
	          std::chrono::milliseconds delay = std::chrono::milliseconds(0));
}
