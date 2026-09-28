#pragma once

// The part of BOIII's component interface the splitscreen component uses:
// a component class with post_unpack(), registered with REGISTER_COMPONENT.

#include <functional>
#include <memory>

enum class component_priority
{
	min = 0,
};

enum class component_type
{
	client,
	server,
	any,
};

struct generic_component
{
	static constexpr component_type type = component_type::any;

	virtual ~generic_component() = default;

	virtual void post_load()
	{
	}

	virtual void pre_destroy()
	{
	}

	virtual void post_unpack()
	{
	}

	virtual component_priority priority() const
	{
		return component_priority::min;
	}
};

struct client_component : generic_component
{
	static constexpr component_type type = component_type::client;
};

namespace component_loader
{
	using registration_functor = std::function<std::unique_ptr<generic_component>()>;

	void register_component(registration_functor functor, component_type type);

	template <typename T>
	class installer final
	{
	public:
		installer()
		{
			register_component([]
			{
				return std::unique_ptr<generic_component>(new T());
			}, T::type);
		}
	};
}

#define REGISTER_COMPONENT(name)                          \
namespace                                                 \
{                                                         \
	component_loader::installer<name> component_installer; \
}
