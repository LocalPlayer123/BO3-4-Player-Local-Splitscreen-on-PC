#pragma once

#include <utility>

namespace utils
{
	template <typename F>
	class final_action
	{
	public:
		explicit final_action(F f) noexcept
			: f_(std::move(f))
		{
		}

		final_action(final_action&& other) noexcept
			: f_(std::move(other.f_)), invoke_(std::exchange(other.invoke_, false))
		{
		}

		final_action(const final_action&) = delete;
		final_action& operator=(const final_action&) = delete;
		final_action& operator=(final_action&&) = delete;

		~final_action() noexcept
		{
			if (this->invoke_)
			{
				this->f_();
			}
		}

	private:
		F f_;
		bool invoke_ = true;
	};

	template <typename F>
	[[nodiscard]] final_action<F> finally(F f) noexcept
	{
		return final_action<F>(std::move(f));
	}
}
