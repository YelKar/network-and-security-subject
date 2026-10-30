#pragma once

#include <unistd.h>

#include <utility>

namespace webserver
{

class UniqueFd
{
public:
	explicit UniqueFd(int fd = -1) noexcept
		: m_fd(fd)
	{
	}

	~UniqueFd()
	{
		Reset();
	}

	UniqueFd(const UniqueFd&) = delete;
	UniqueFd& operator=(const UniqueFd&) = delete;

	UniqueFd(UniqueFd&& other) noexcept
		: m_fd(std::exchange(other.m_fd, -1))
	{
	}

	UniqueFd& operator=(UniqueFd&& other) noexcept
	{
		if (this != &other)
		{
			Reset(std::exchange(other.m_fd, -1));
		}
		return *this;
	}

	int Get() const noexcept
	{
		return m_fd;
	}

	bool IsValid() const noexcept
	{
		return m_fd >= 0;
	}

	explicit operator bool() const noexcept
	{
		return IsValid();
	}

	int Release() noexcept
	{
		return std::exchange(m_fd, -1);
	}

	void Reset(int fd = -1) noexcept
	{
		if (m_fd >= 0)
		{
			::close(m_fd);
		}
		m_fd = fd;
	}

private:
	int m_fd = -1;
};

} // namespace webserver
