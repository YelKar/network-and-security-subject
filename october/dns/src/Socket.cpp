#include <DNS/Socket.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace dns
{

Socket::Socket(const std::string_view serverIp, const std::uint16_t port, const Protocol protocol, const int timeoutSeconds)
	: m_protocol(protocol)
{
	const int sockType = (protocol == Protocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
	m_sockFd = socket(AF_INET, sockType, 0);
	if (m_sockFd < 0)
	{
		throw SocketException("socket() creation failed: " + std::string(std::strerror(errno)));
	}

	struct timeval tv{};
	tv.tv_sec = timeoutSeconds;
	tv.tv_usec = 0;
	if (setsockopt(m_sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
		Close();
		throw SocketException("setsockopt(SO_RCVTIMEO) failed: " + std::string(std::strerror(errno)));
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, serverIp.data(), &addr.sin_addr) <= 0)
	{
		Close();
		throw SocketException("Invalid IP address: " + std::string(serverIp));
	}

	if (connect(m_sockFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		const int err = errno;
		Close();
		if (err == ETIMEDOUT || err == EINPROGRESS)
		{
			throw TimeoutException("connect() timed out");
		}
		throw SocketException("connect() failed: " + std::string(std::strerror(err)));
	}
}

Socket::~Socket()
{
	Close();
}

Socket::Socket(Socket&& other) noexcept
	: m_sockFd(other.m_sockFd)
	, m_protocol(other.m_protocol)
{
	other.m_sockFd = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept
{
	if (this != &other)
	{
		Close();
		m_sockFd = other.m_sockFd;
		m_protocol = other.m_protocol;
		other.m_sockFd = -1;
	}
	return *this;
}

void Socket::Close() noexcept
{
	if (m_sockFd >= 0)
	{
		close(m_sockFd);
		m_sockFd = -1;
	}
}

void Socket::Send(const std::vector<std::byte>& data) const
{
	if (m_sockFd < 0)
	{
		throw SocketException("Socket is not open");
	}

	std::size_t totalSent = 0;
	while (totalSent < data.size())
	{
		const ssize_t sent = send(m_sockFd, data.data() + totalSent, data.size() - totalSent, 0);
		if (sent < 0)
		{
			throw SocketException("send() failed: " + std::string(std::strerror(errno)));
		}
		totalSent += static_cast<std::size_t>(sent);

		if (m_protocol == Protocol::UDP)
		{
			break;
		}
	}
}

std::vector<std::byte> Socket::Receive(const std::size_t maxBytes) const
{
	if (m_sockFd < 0)
	{
		throw SocketException("Socket is not open");
	}

	std::vector<std::byte> buffer(maxBytes);
	const ssize_t bytesRead = recv(m_sockFd, buffer.data(), buffer.size(), 0);
	if (bytesRead < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			throw TimeoutException("recv() timed out");
		}
		throw SocketException("recv() failed: " + std::string(std::strerror(errno)));
	}
	buffer.resize(static_cast<std::size_t>(bytesRead));
	return buffer;
}

std::vector<std::byte> Socket::ReceiveExact(const std::size_t exactBytes) const
{
	if (m_sockFd < 0)
	{
		throw SocketException("Socket is not open");
	}

	std::vector<std::byte> buffer(exactBytes);
	std::size_t totalRead = 0;

	while (totalRead < exactBytes)
	{
		const ssize_t bytes = recv(m_sockFd, buffer.data() + totalRead, exactBytes - totalRead, 0);
		if (bytes < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				throw TimeoutException("recv() timed out waiting for exact bytes");
			}
			throw SocketException("recv() failed: " + std::string(std::strerror(errno)));
		}
		if (bytes == 0)
		{
			throw SocketException("Peer closed connection before receiving all bytes");
		}
		totalRead += static_cast<std::size_t>(bytes);
	}

	return buffer;
}

} // namespace dns