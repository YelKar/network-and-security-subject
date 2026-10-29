#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace dns
{

enum class Protocol : int
{
	TCP,
	UDP
};

class SocketException : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

class TimeoutException : public SocketException
{
public:
	using SocketException::SocketException;
};

class Socket
{
public:
	Socket(std::string_view serverIp, std::uint16_t port, Protocol protocol, int timeoutSeconds = 2);
	~Socket();

	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	Socket(Socket&& other) noexcept;
	Socket& operator=(Socket&& other) noexcept;

	void Send(const std::vector<std::byte>& data) const;
	[[nodiscard]] std::vector<std::byte> Receive(std::size_t maxBytes = 4096) const;
	[[nodiscard]] std::vector<std::byte> ReceiveExact(std::size_t exactBytes) const;

	void Close() noexcept;

private:
	int m_sockFd{ -1 };
	Protocol m_protocol{ Protocol::UDP };
};

} // namespace dns