#pragma once

#include <cstddef>
#include <cstdint>

namespace splitscreen::safety
{
	struct patch
	{
		void* target;
		const void* before;
		const void* after;
		size_t size;
	};

	// The writer returns false only when it has not modified the target. Keep
	// replacement storage alive even on failure: rollback may itself be refused.
	template <typename Writer>
	bool apply_patches(const patch* patches, const size_t count, Writer&& write)
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (!write(patches[i].target, patches[i].after, patches[i].size))
			{
				while (i)
				{
					--i;
					write(patches[i].target, patches[i].before, patches[i].size);
				}
				return false;
			}
		}
		return true;
	}

	inline bool range_fits(const uintptr_t address, const size_t size,
	                       const uintptr_t region, const size_t region_size)
	{
		return address >= region && address - region <= region_size
			&& size <= region_size - (address - region);
	}

	inline uint64_t object_index(const uint64_t object, const uint64_t objects,
	                             const uint64_t stride, const uint64_t count)
	{
		if (!objects || !stride || object < objects)
		{
			return count;
		}
		const auto offset = object - objects;
		return offset % stride == 0 && offset / stride < count ? offset / stride : count;
	}

	inline bool valid_cache_count(const int32_t count, const size_t capacity)
	{
		return count >= 0 && static_cast<size_t>(count) <= capacity;
	}

	inline bool valid_decode_size(const int32_t size, const size_t capacity)
	{
		return size >= 0 && static_cast<size_t>(size) <= capacity;
	}
}