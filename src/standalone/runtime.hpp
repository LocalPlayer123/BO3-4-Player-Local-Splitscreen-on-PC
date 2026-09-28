#pragma once

namespace runtime
{
	// Starts the splitscreen component if the running game is the supported
	// build. Returns false (and does nothing) for any other executable.
	bool start();
}
