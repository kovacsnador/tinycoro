#ifndef __STORAGE_HPP__
#define __STORAGE_HPP__

#include <concepts>
#include <utility>
#include <memory>
#include <cstring>

template<typename BaseClassT, std::unsigned_integral auto SIZE, typename AlignasT = char>
	requires (SIZE >= sizeof(AlignasT))
struct Storage
{
	Storage() = default;

	template<typename TypeCarrierT, typename... Args>
		requires std::constructible_from<typename TypeCarrierT::value_type, Args...>
	Storage([[maybe_unused]] TypeCarrierT typeCarrier, Args&&... args)
		: _owner{true}
	{
		std::construct_at(GetAs<typename TypeCarrierT::value_type>(), std::forward<Args>(args)...);
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

	auto* Get()
	{
		return GetAs<BaseClassT>();
	}

	operator bool() const noexcept
	{
		return _owner;
	}

private:

	template<typename T>
		requires (sizeof(T) <= SIZE)
	T* GetAs()
	{
		return std::launder(reinterpret_cast<T*>(_buffer));
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

	//unsigned char _buffer[SIZE];
	bool _owner{ false };
};

#endif // !__STORAGE_HPP__
