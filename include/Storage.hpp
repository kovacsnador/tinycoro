#ifndef __STORAGE_HPP__
#define __STORAGE_HPP__

#include <concepts>
#include <utility>
#include <memory>
#include <cstring>
#include <type_traits>

template<typename BaseClassT, std::unsigned_integral auto SIZE, typename AlignasT = char>
	requires (SIZE >= sizeof(AlignasT))
struct Storage
{
	Storage() = default;

	template<typename ClassT, typename... Args>
		requires std::constructible_from<ClassT, Args...>
	Storage([[maybe_unused]] std::type_identity<ClassT>, Args&&... args)
		: _owner{true}
	{
		std::construct_at(GetAs<ClassT>(), std::forward<Args>(args)...);
	}

	Storage(Storage&& other) noexcept
		: _owner{ std::exchange(other._owner, false) }
	{
		std::memcpy(_buffer, other._buffer, SIZE);
	}

	Storage& operator=(Storage&& other) noexcept
	{
		if (std::addressof(other) != this)
		{
			Destroy();

			_owner = std::exchange(other._owner, false);
			std::memcpy(_buffer, other._buffer, SIZE);
		}

		return *this;
	}

	~Storage()
	{
		Destroy();
	}

	void reset()
	{
		Destroy();
	}

	auto* operator->()
	{
        return GetAs<BaseClassT>();
    }

	const auto* operator->() const
	{
        return GetAs<BaseClassT>();
    }

	operator bool() const noexcept
	{
		return _owner;
	}

	[[nodiscard]] bool operator==(std::nullptr_t) const noexcept
	{
		return !this->operator bool();
	}

private:

	template<typename T>
		requires (sizeof(T) <= SIZE)
	T* GetAs()
	{
		return std::launder(reinterpret_cast<T*>(_buffer));
	}

	template<typename T>
		requires (sizeof(T) <= SIZE)
	const T* GetAs() const
	{
		return std::launder(reinterpret_cast<const T*>(_buffer));
	}

	void Destroy()
	{
		if (_owner)
		{
			std::destroy_at(GetAs<BaseClassT>());
			_owner = false;
		}
	}


	alignas(AlignasT) unsigned char _buffer[SIZE];
	
	bool _owner{ false };
};

#endif // !__STORAGE_HPP__
