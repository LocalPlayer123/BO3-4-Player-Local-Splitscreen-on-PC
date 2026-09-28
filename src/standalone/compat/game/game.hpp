#pragma once

#include <cstddef>

namespace game
{
	// Base address of the game image. Set once, when the runtime has verified
	// that the running game is the supported build.
	size_t get_base();

	// The runtime only starts the component in the supported client build,
	// so this is always false inside the mod.
	bool is_server();
	bool is_client();

	// Same game function BOIII exposes as a symbol (client RVA 0x02148350).
	bool Com_IsRunningUILevel();
}
