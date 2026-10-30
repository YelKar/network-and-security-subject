#include <webserver/HttpServer.h>
#include <webserver/MimeTypes.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <syncstream>
#include <system_error>
#include <utility>
#include <vector>


namespace webserver
{

namespace
{

constexpr int POLL_TIMEOUT_MS = 100;
constexpr std::size_t MAX_REQUEST_HEADER_BYTES = 16 * 1024;
constexpr std::chrono::milliseconds READ_TIMEOUT{ 5000 };
constexpr std::chrono::milliseconds SEND_TIMEOUT{ 10000 };

#if defined(MSG_NOSIGNAL)
constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
constexpr int SEND_FLAGS = 0;
#endif

void Log(std::string_view message)
{
	std::osyncstream{ std::cout } << message << '\n';
}

void LogError(std::string_view message)
{
	std::osyncstream{ std::cerr } << message << '\n';
}

std::string ErrnoMessage(int error)
{
	return std::string{ std::strerror(error) };
}

HttpResponse MakeTextResponse(HttpStatus status, std::string_view text)
{
	HttpResponse response;
	response.status = status;
	response.contentType = "text/plain";
	response.body.assign(text.begin(), text.end());
	return response;
}

int HexValue(char character)
{
	if (character >= '0' && character <= '9')
	{
		return character - '0';
	}
	if (character >= 'a' && character <= 'f')
	{
		return character - 'a' + 10;
	}
	if (character >= 'A' && character <= 'F')
	{
		return character - 'A' + 10;
	}
	return -1;
}

std::optional<std::string> DecodeRequestPath(std::string_view target)
{
	std::size_t pathEnd = target.size();

	if (const auto queryPosition = target.find('?'); queryPosition != std::string_view::npos)
	{
		pathEnd = std::min(pathEnd, queryPosition);
	}

	if (const auto fragmentPosition = target.find('#'); fragmentPosition != std::string_view::npos)
	{
		pathEnd = std::min(pathEnd, fragmentPosition);
	}

	const std::string_view path = target.substr(0, pathEnd);
	if (path.empty() || path.front() != '/')
	{
		return std::nullopt;
	}

	std::string decoded;
	decoded.reserve(path.size());

	for (std::size_t index = 0; index < path.size(); ++index)
	{
		const char character = path[index];

		if (character == '%')
		{
			if (index + 2 >= path.size())
			{
				return std::nullopt;
			}

			const int high = HexValue(path[index + 1]);
			const int low = HexValue(path[index + 2]);
			if (high < 0 || low < 0)
			{
				return std::nullopt;
			}

			const char value = static_cast<char>((high << 4) | low);
			const auto byte = static_cast<unsigned char>(value);
			if (value == '\0' || byte < 0x20 || byte == 0x7f)
			{
				return std::nullopt;
			}

			decoded.push_back(value);
			index += 2;
			continue;
		}

		const auto byte = static_cast<unsigned char>(character);
		if (byte < 0x20 || byte == 0x7f)
		{
			return std::nullopt;
		}

		decoded.push_back(character);
	}

	const auto firstNonSlash = decoded.find_first_not_of('/');
	if (firstNonSlash == std::string::npos)
	{
		return std::string{ "index.html" };
	}

	return decoded.substr(firstNonSlash);
}

bool HasParentComponent(const std::filesystem::path& path)
{
	for (const auto& component : path)
	{
		if (component == "..")
		{
			return true;
		}
	}
	return false;
}

enum class ReadStatus
{
	OK,
	CLOSED,
	BAD_REQUEST,
};

struct ReadResult
{
	ReadStatus status = ReadStatus::CLOSED;
	HttpRequest request;
};

ReadResult ParseRequestLine(std::string_view header)
{
	const auto lineEnd = header.find("\r\n");
	const std::string_view requestLine = header.substr(0, lineEnd);

	const auto firstSpace = requestLine.find(' ');
	if (firstSpace == std::string_view::npos)
	{
		return { ReadStatus::BAD_REQUEST, {} };
	}

	const auto secondSpace = requestLine.find(' ', firstSpace + 1);
	if (secondSpace == std::string_view::npos)
	{
		return { ReadStatus::BAD_REQUEST, {} };
	}

	if (requestLine.find(' ', secondSpace + 1) != std::string_view::npos)
	{
		return { ReadStatus::BAD_REQUEST, {} };
	}

	HttpRequest request;
	request.method = std::string{ requestLine.substr(0, firstSpace) };
	request.target = std::string{ requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1) };
	request.version = std::string{ requestLine.substr(secondSpace + 1) };

	if (request.method.empty() || request.target.empty() || request.version.empty())
	{
		return { ReadStatus::BAD_REQUEST, {} };
	}

	if (request.version != "HTTP/1.1" && request.version != "HTTP/1.0")
	{
		return { ReadStatus::BAD_REQUEST, {} };
	}

	return { ReadStatus::OK, std::move(request) };
}

ReadResult ReadRequest(int clientFd, std::stop_token stopToken)
{
	std::string buffer;
	buffer.reserve(4096);
	std::array<char, 4096> chunk{};
	const auto deadline = std::chrono::steady_clock::now() + READ_TIMEOUT;

	while (true)
	{
		if (stopToken.stop_requested())
		{
			return { ReadStatus::CLOSED, {} };
		}

		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
		{
			return { buffer.empty() ? ReadStatus::CLOSED : ReadStatus::BAD_REQUEST, {} };
		}

		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
		const int timeout = static_cast<int>(std::min<std::int64_t>(POLL_TIMEOUT_MS, remaining));

		pollfd descriptor{ clientFd, POLLIN, 0 };
		const int pollResult = poll(&descriptor, 1, timeout);
		if (pollResult < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return { ReadStatus::CLOSED, {} };
		}

		if (pollResult == 0)
		{
			continue;
		}

		if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 && (descriptor.revents & POLLIN) == 0)
		{
			return { buffer.empty() ? ReadStatus::CLOSED : ReadStatus::BAD_REQUEST, {} };
		}

		const ssize_t received = recv(clientFd, chunk.data(), chunk.size(), 0);
		if (received == 0)
		{
			return { buffer.empty() ? ReadStatus::CLOSED : ReadStatus::BAD_REQUEST, {} };
		}

		if (received < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return { ReadStatus::CLOSED, {} };
		}

		buffer.append(chunk.data(), static_cast<std::size_t>(received));

		if (buffer.size() > MAX_REQUEST_HEADER_BYTES)
		{
			return { ReadStatus::BAD_REQUEST, {} };
		}

		if (const auto headerEnd = buffer.find("\r\n\r\n"); headerEnd != std::string::npos)
		{
			return ParseRequestLine(std::string_view{ buffer }.substr(0, headerEnd));
		}
	}
}

bool SendAll(int clientFd, const char* data, std::size_t size, std::stop_token stopToken)
{
	std::size_t sent = 0;
	const auto deadline = std::chrono::steady_clock::now() + SEND_TIMEOUT;

	while (sent < size)
	{
		if (stopToken.stop_requested())
		{
			return false;
		}

		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
		{
			return false;
		}

		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
		const int timeout = static_cast<int>(std::min<std::int64_t>(POLL_TIMEOUT_MS, remaining));

		pollfd descriptor{ clientFd, POLLOUT, 0 };
		const int pollResult = poll(&descriptor, 1, timeout);
		if (pollResult < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return false;
		}

		if (pollResult == 0)
		{
			continue;
		}

		if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 && (descriptor.revents & POLLOUT) == 0)
		{
			return false;
		}

		const ssize_t written = send(clientFd, data + sent, size - sent, SEND_FLAGS);
		if (written < 0)
		{
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			{
				continue;
			}
			return false;
		}

		if (written == 0)
		{
			return false;
		}

		sent += static_cast<std::size_t>(written);
	}

	return true;
}

} // namespace

HttpServer::HttpServer(ServerConfig config)
	: m_config(std::move(config))
{
}

HttpServer::~HttpServer() = default;

int HttpServer::Run(std::stop_token stopToken)
{
	std::string errorMessage;
	if (!Initialize(errorMessage))
	{
		LogError("[server] " + errorMessage);
		return 1;
	}

	bool failed = false;

	while (!stopToken.stop_requested())
	{
		pollfd descriptor{ m_listenFd.Get(), POLLIN, 0 };
		const int pollResult = poll(&descriptor, 1, POLL_TIMEOUT_MS);

		if (pollResult < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}

			LogError("[server] poll() failed: " + ErrnoMessage(errno));
			failed = true;
			break;
		}

		if (stopToken.stop_requested())
		{
			break;
		}

		if (pollResult > 0 && (descriptor.revents & POLLIN) != 0)
		{
			const int clientFd = accept(m_listenFd.Get(), nullptr, nullptr);
			if (clientFd >= 0)
			{
				SpawnWorker(UniqueFd{ clientFd });
			}
			else if (errno != EINTR)
			{
				LogError("[server] accept() failed: " + ErrnoMessage(errno));
			}
		}

		ReapFinishedWorkers();
	}

	m_listenFd.Reset();
	Log("[server] shutdown requested");
	StopWorkers();
	Log("[server] stopped");

	return failed ? 1 : 0;
}

bool HttpServer::Initialize(std::string& errorMessage)
{
	std::error_code error;
	const auto absoluteRoot = std::filesystem::absolute(m_config.rootDir, error);
	if (error)
	{
		errorMessage = "Failed to resolve document root: " + error.message();
		return false;
	}

	m_rootDir = std::filesystem::weakly_canonical(absoluteRoot, error);
	if (error)
	{
		errorMessage = "Failed to canonicalize document root: " + error.message();
		return false;
	}

	if (!std::filesystem::is_directory(m_rootDir, error) || error)
	{
		errorMessage = "Document root is not a directory: " + m_rootDir.string();
		return false;
	}

	m_listenFd.Reset(socket(AF_INET, SOCK_STREAM, 0));
	if (!m_listenFd.IsValid())
	{
		errorMessage = "socket() failed: " + ErrnoMessage(errno);
		return false;
	}

	int reuseAddress = 1;
	if (setsockopt(m_listenFd.Get(), SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) != 0)
	{
		errorMessage = "setsockopt(SO_REUSEADDR) failed: " + ErrnoMessage(errno);
		return false;
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(m_config.port);

	if (inet_pton(AF_INET, m_config.host.c_str(), &address.sin_addr) != 1)
	{
		errorMessage = "Invalid host address: " + m_config.host;
		return false;
	}

	if (bind(m_listenFd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		errorMessage = "bind() failed: " + ErrnoMessage(errno);
		return false;
	}

	if (listen(m_listenFd.Get(), SOMAXCONN) != 0)
	{
		errorMessage = "listen() failed: " + ErrnoMessage(errno);
		return false;
	}

	Log("[server] listening on " + m_config.host + ":" + std::to_string(m_config.port) + " root=" + m_rootDir.string());
	return true;
}

void HttpServer::SpawnWorker(UniqueFd clientFd)
{
	auto finished = std::make_shared<std::atomic_bool>(false);

	auto thread = std::jthread(
		[this, fd = std::move(clientFd), finished](std::stop_token stopToken) mutable {
			try
			{
				HandleClient(std::move(fd), stopToken);
			}
			catch (const std::exception& error)
			{
				LogError("[conn] unhandled exception: " + std::string{ error.what() });
			}
			catch (...)
			{
				LogError("[conn] unhandled unknown exception");
			}

			finished->store(true, std::memory_order_release);
		});

	std::scoped_lock lock{ m_workersMutex };
	m_workers.push_back(Worker{ std::move(thread), std::move(finished) });
}

void HttpServer::ReapFinishedWorkers()
{
	std::vector<Worker> finishedWorkers;

	{
		std::scoped_lock lock{ m_workersMutex };
		std::erase_if(
			m_workers,
			[&finishedWorkers](Worker& worker) {
				if (!worker.finished->load(std::memory_order_acquire))
				{
					return false;
				}

				worker.thread.request_stop();
				finishedWorkers.push_back(std::move(worker));
				return true;
			});
	}
}

void HttpServer::StopWorkers()
{
	std::vector<Worker> workers;

	{
		std::scoped_lock lock{ m_workersMutex };
		workers.swap(m_workers);
	}

	for (auto& worker : workers)
	{
		worker.thread.request_stop();
	}

	workers.clear();
}

void HttpServer::HandleClient(UniqueFd clientFd, std::stop_token stopToken)
{
	const int fd = clientFd.Get();
	const std::string prefix = "[conn " + std::to_string(fd) + "] ";

#if defined(SO_NOSIGPIPE)
	int noSigPipe = 1;
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif

	Log(prefix + "accepted");

	const ReadResult readResult = ReadRequest(fd, stopToken);

	if (readResult.status == ReadStatus::CLOSED)
	{
		Log(prefix + "closed without request");
		return;
	}

	if (readResult.status == ReadStatus::BAD_REQUEST)
	{
		Log(prefix + "400 Bad Request");
		SendResponse(fd, MakeTextResponse(HttpStatus::BAD_REQUEST, "Bad Request"), stopToken);
		Log(prefix + "connection closed");
		return;
	}

	const HttpRequest& request = readResult.request;
	Log(prefix + request.method + " " + request.target + " " + request.version);

	HttpResponse response;
	if (request.method != "GET")
	{
		response = MakeTextResponse(HttpStatus::METHOD_NOT_ALLOWED, "Method Not Allowed");
	}
	else
	{
		response = BuildFileResponse(request.target);
	}

	Log(prefix + std::to_string(static_cast<int>(response.status)) + " " + std::string{ GetReasonPhrase(response.status) });

	SendResponse(fd, response, stopToken);
	Log(prefix + "connection closed");
}

HttpResponse HttpServer::BuildFileResponse(std::string_view requestTarget) const
{
	const auto relativePath = DecodeRequestPath(requestTarget);
	if (!relativePath.has_value())
	{
		return MakeTextResponse(HttpStatus::BAD_REQUEST, "Bad Request");
	}

	std::error_code error;
	const auto canonical = std::filesystem::weakly_canonical(m_rootDir / *relativePath, error);
	if (error)
	{
		return MakeTextResponse(HttpStatus::NOT_FOUND, "File Not Found");
	}

	const auto relativeToRoot = canonical.lexically_relative(m_rootDir);
	if (
		(relativeToRoot.empty() && canonical != m_rootDir)
		|| relativeToRoot.is_absolute()
		|| HasParentComponent(relativeToRoot))
	{
		return MakeTextResponse(HttpStatus::FORBIDDEN, "Forbidden");
	}

	if (!std::filesystem::is_regular_file(canonical, error) || error)
	{
		return MakeTextResponse(HttpStatus::NOT_FOUND, "File Not Found");
	}

	const auto fileSize = std::filesystem::file_size(canonical, error);
	if (error)
	{
		return MakeTextResponse(HttpStatus::INTERNAL_SERVER_ERROR, "Internal Server Error");
	}

	if (
		fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())
		|| fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
	{
		return MakeTextResponse(HttpStatus::INTERNAL_SERVER_ERROR, "Internal Server Error");
	}

	std::ifstream file{ canonical, std::ios::binary };
	if (!file)
	{
		return MakeTextResponse(HttpStatus::FORBIDDEN, "Forbidden");
	}

	HttpResponse response;
	response.status = HttpStatus::OK;
	response.contentType = std::string{ GetMimeType(canonical) };
	response.body.resize(static_cast<std::size_t>(fileSize));

	if (fileSize > 0)
	{
		file.read(reinterpret_cast<char*>(response.body.data()), static_cast<std::streamsize>(fileSize));

		if (!file || static_cast<std::uintmax_t>(file.gcount()) != fileSize)
		{
			return MakeTextResponse(HttpStatus::INTERNAL_SERVER_ERROR, "Internal Server Error");
		}
	}

	return response;
}

void HttpServer::SendResponse(int clientFd, const HttpResponse& response, std::stop_token stopToken) const
{
	std::string header;
	header.reserve(256);
	header += "HTTP/1.1 ";
	header += std::to_string(static_cast<int>(response.status));
	header += ' ';
	header += GetReasonPhrase(response.status);
	header += "\r\nContent-Type: ";
	header += response.contentType;
	header += "\r\nContent-Length: ";
	header += std::to_string(response.body.size());
	header += "\r\nConnection: close\r\n";

	if (response.status == HttpStatus::METHOD_NOT_ALLOWED)
	{
		header += "Allow: GET\r\n";
	}

	header += "\r\n";

	if (!SendAll(clientFd, header.data(), header.size(), stopToken))
	{
		return;
	}

	if (!response.body.empty())
	{
		SendAll(
			clientFd,
			reinterpret_cast<const char*>(response.body.data()),
			response.body.size(),
			stopToken);
	}
}

} // namespace webserver
