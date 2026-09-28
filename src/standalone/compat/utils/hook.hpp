#pragma once

// The subset of BOIII's utils::hook the component uses, on MinHook like the
// original: detour (create / invoke / get_original) and call.

#include <cstddef>
#include <cstdint>

namespace utils::hook
{
	class detour
	{
	public:
		detour() = default;
		~detour();

		detour(const detour&) = delete;
		detour& operator=(const detour&) = delete;

		void create(void* place, void* target);
		void create(size_t place, void* target);
		void clear();

		void* get_place() const
		{
			return this->place_;
		}

		[[nodiscard]] void* get_original() const
		{
			return this->original_;
		}

		template <typename T>
		T* get() const
		{
			return static_cast<T*>(this->get_original());
		}

		template <typename T = void, typename... Args>
		T invoke(Args... args)
		{
			return static_cast<T(*)(Args...)>(this->get_original())(args...);
		}

	private:
		void* place_{};
		void* original_{};
	};

	// Writes `call data` (E8 rel32) at pointer, through a nearby absolute-jump
	// thunk when data is out of rel32 range.
	void call(void* pointer, void* data);
	void call(size_t pointer, void* data);
}
