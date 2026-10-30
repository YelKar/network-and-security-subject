#include <gtest/gtest.h>
#include <openssl/evp.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifndef SERVER_BIN_PATH
#define SERVER_BIN_PATH "./webserver"
#endif

#include <webserver/FileDescriptor.h>

namespace
{

constexpr std::chrono::milliseconds START_TIMEOUT{ 5000 };
constexpr std::chrono::milliseconds STOP_TIMEOUT{ 5000 };
constexpr std::chrono::milliseconds POLL_INTERVAL{ 20 };
constexpr int REQUEST_COUNT = 100;

#if defined(MSG_NOSIGNAL)
constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
constexpr int SEND_FLAGS = 0;
#endif

std::string ErrnoMessage(int error)
{
	return std::error_code{ error, std::system_category() }.message();
}

std::filesystem::path ResolvePath(const char* variable, const char* fallback)
{
	const char* value = std::getenv(variable);
	const std::string pathText = value != nullptr && *value != '\0' ? value : fallback;

	std::error_code error;
	const auto path = std::filesystem::absolute(pathText, error);
	if (error)
	{
		throw std::runtime_error{ "Failed to resolve path " + pathText + ": " + error.message() };
	}

	return path;
}

std::filesystem::path BinaryPath()
{
	return ResolvePath("WEBSERVER_BINARY", SERVER_BIN_PATH);
}

std::filesystem::path PublicPath()
{
	return ResolvePath("WEBSERVER_PUBLIC", "public");
}

std::string ReadTextFile(const std::filesystem::path& path)
{
	std::ifstream file{ path, std::ios::binary };
	if (!file)
	{
		return {};
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	return stream.str();
}

std::string Sha256Hex(std::string_view data)
{
	using EvpContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

	EvpContext context{ EVP_MD_CTX_new(), &EVP_MD_CTX_free };
	if (!context)
	{
		throw std::runtime_error{ "EVP_MD_CTX_new() failed" };
	}

	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int digestLength = 0;

	if (
		EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1
		|| (!data.empty() && EVP_DigestUpdate(context.get(), data.data(), data.size()) != 1)
		|| EVP_DigestFinal_ex(context.get(), digest.data(), &digestLength) != 1)
	{
		throw std::runtime_error{ "SHA-256 calculation failed" };
	}

	static constexpr char HEX_DIGITS[] = "0123456789abcdef";

	std::string result;
	result.reserve(static_cast<std::size_t>(digestLength) * 2);

	for (unsigned int index = 0; index < digestLength; ++index)
	{
		const unsigned char byte = digest[index];
		result.push_back(HEX_DIGITS[byte >> 4]);
		result.push_back(HEX_DIGITS[byte & 0x0f]);
	}

	return result;
}

std::uint16_t ReserveFreePort()
{
	webserver::UniqueFd socketFd{ socket(AF_INET, SOCK_STREAM, 0) };
	if (!socketFd.IsValid())
	{
		throw std::runtime_error{ "socket() failed: " + ErrnoMessage(errno) };
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = 0;

	if (bind(socketFd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		throw std::runtime_error{ "bind() failed: " + ErrnoMessage(errno) };
	}

	socklen_t addressLength = sizeof(address);
	if (getsockname(socketFd.Get(), reinterpret_cast<sockaddr*>(&address), &addressLength) != 0)
	{
		throw std::runtime_error{ "getsockname() failed: " + ErrnoMessage(errno) };
	}

	return ntohs(address.sin_port);
}

bool CanConnect(std::uint16_t port)
{
	webserver::UniqueFd socketFd{ socket(AF_INET, SOCK_STREAM, 0) };
	if (!socketFd.IsValid())
	{
		return false;
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1)
	{
		return false;
	}

	int result = 0;
	do
	{
		result = connect(socketFd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
	} while (result != 0 && errno == EINTR);

	return result == 0;
}

class ServerProcess
{
public:
	ServerProcess(std::filesystem::path binary, std::filesystem::path publicDir)
		: m_binary(std::move(binary))
		, m_publicDir(std::move(publicDir))
		, m_port(ReserveFreePort())
		, m_logPath(
			  std::filesystem::temp_directory_path()
			  / ("webserver-test-" + std::to_string(getpid()) + "-" + std::to_string(m_port) + ".log"))
	{
		if (!std::filesystem::is_regular_file(m_binary))
		{
			throw std::runtime_error{ "Server binary not found: " + m_binary.string() };
		}

		if (!std::filesystem::is_directory(m_publicDir))
		{
			throw std::runtime_error{ "Public directory not found: " + m_publicDir.string() };
		}

		Start();
	}

	~ServerProcess()
	{
		Stop();
	}

	ServerProcess(const ServerProcess&) = delete;
	ServerProcess& operator=(const ServerProcess&) = delete;
	ServerProcess(ServerProcess&&) = delete;
	ServerProcess& operator=(ServerProcess&&) = delete;

	std::uint16_t Port() const
	{
		return m_port;
	}

	const std::filesystem::path& LogPath() const
	{
		return m_logPath;
	}

	int StopWithSigint()
	{
		if (!m_running)
		{
			return -1;
		}

		if (m_pid > 0)
		{
			kill(m_pid, SIGINT);
		}

		int status = 0;
		const bool exited = WaitForExit(status);

		if (!exited)
		{
			if (m_pid > 0)
			{
				kill(m_pid, SIGKILL);
			}
			waitpid(m_pid, &status, 0);
		}

		m_running = false;

		if (!exited || !WIFEXITED(status))
		{
			return -1;
		}

		return WEXITSTATUS(status);
	}

	void Stop()
	{
		if (m_running)
		{
			StopWithSigint();
		}
	}

private:
	std::filesystem::path m_binary;
	std::filesystem::path m_publicDir;
	std::uint16_t m_port = 0;
	pid_t m_pid = -1;
	std::filesystem::path m_logPath;
	bool m_running = false;

	void Start()
	{
		const std::string binaryText = m_binary.string();
		const std::string publicText = m_publicDir.string();
		const std::string portText = std::to_string(m_port);
		const std::string logText = m_logPath.string();

		m_pid = fork();
		if (m_pid < 0)
		{
			throw std::runtime_error{ "fork() failed: " + ErrnoMessage(errno) };
		}

		if (m_pid == 0)
		{
			const int logFd = open(logText.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
			if (logFd >= 0)
			{
				dup2(logFd, STDOUT_FILENO);
				dup2(logFd, STDERR_FILENO);
				close(logFd);
			}

			execl(
				binaryText.c_str(),
				binaryText.c_str(),
				"-h",
				"127.0.0.1",
				"-p",
				portText.c_str(),
				"-d",
				publicText.c_str(),
				static_cast<char*>(nullptr));
			_exit(127);
		}

		m_running = true;

		try
		{
			WaitUntilReady();
		}
		catch (...)
		{
			Stop();
			throw;
		}
	}

	void WaitUntilReady()
	{
		const auto deadline = std::chrono::steady_clock::now() + START_TIMEOUT;

		while (std::chrono::steady_clock::now() < deadline)
		{
			if (CanConnect(m_port))
			{
				return;
			}

			int status = 0;
			const pid_t result = waitpid(m_pid, &status, WNOHANG);

			if (result == m_pid)
			{
				m_running = false;
				throw std::runtime_error{
					"Server exited before accepting connections\n" + ReadTextFile(m_logPath)
				};
			}

			if (result < 0 && errno != EINTR)
			{
				m_running = false;
				throw std::runtime_error{
					"waitpid() failed while waiting for server: " + ErrnoMessage(errno)
				};
			}

			std::this_thread::sleep_for(POLL_INTERVAL);
		}

		Stop();
		throw std::runtime_error{ "Timed out waiting for server\n" + ReadTextFile(m_logPath) };
	}

	bool WaitForExit(int& status)
	{
		const auto deadline = std::chrono::steady_clock::now() + STOP_TIMEOUT;

		while (std::chrono::steady_clock::now() < deadline)
		{
			const pid_t result = waitpid(m_pid, &status, WNOHANG);

			if (result == m_pid)
			{
				return true;
			}

			if (result < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				return errno == ECHILD;
			}

			std::this_thread::sleep_for(POLL_INTERVAL);
		}

		return false;
	}
};

struct HttpResponse
{
	int status = 0;
	std::string headers;
	std::string contentType;
	std::string body;
};

std::string ToLower(std::string value)
{
	for (char& character : value)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	return value;
}

std::string Trim(std::string_view text)
{
	std::size_t begin = 0;
	std::size_t end = text.size();

	while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
	{
		++begin;
	}

	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
	{
		--end;
	}

	return std::string{ text.substr(begin, end - begin) };
}

std::string BuildRequest(std::string_view method, std::string_view target)
{
	std::string request;
	request += method;
	request += ' ';
	request += target;
	request += " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
	return request;
}

std::string SendRaw(std::uint16_t port, std::string_view request)
{
	webserver::UniqueFd socketFd{ socket(AF_INET, SOCK_STREAM, 0) };
	if (!socketFd.IsValid())
	{
		throw std::runtime_error{ "socket() failed: " + ErrnoMessage(errno) };
	}

	timeval timeout{};
	timeout.tv_sec = 5;
	setsockopt(socketFd.Get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(socketFd.Get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1)
	{
		throw std::runtime_error{ "inet_pton() failed" };
	}

	int connectResult = 0;
	do
	{
		connectResult = connect(socketFd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
	} while (connectResult != 0 && errno == EINTR);

	if (connectResult != 0)
	{
		throw std::runtime_error{ "connect() failed: " + ErrnoMessage(errno) };
	}

	std::size_t sent = 0;
	while (sent < request.size())
	{
		const ssize_t written = send(socketFd.Get(), request.data() + sent, request.size() - sent, SEND_FLAGS);

		if (written < 0)
		{
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			{
				continue;
			}
			throw std::runtime_error{ "send() failed: " + ErrnoMessage(errno) };
		}

		if (written == 0)
		{
			throw std::runtime_error{ "send() returned zero" };
		}

		sent += static_cast<std::size_t>(written);
	}

	shutdown(socketFd.Get(), SHUT_WR);

	std::string response;
	std::array<char, 8192> buffer{};

	while (true)
	{
		const ssize_t received = recv(socketFd.Get(), buffer.data(), buffer.size(), 0);

		if (received == 0)
		{
			break;
		}

		if (received < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			throw std::runtime_error{ "recv() failed: " + ErrnoMessage(errno) };
		}

		response.append(buffer.data(), static_cast<std::size_t>(received));
	}

	return response;
}

HttpResponse ParseResponse(std::string_view raw)
{
	const auto headerEnd = raw.find("\r\n\r\n");
	if (headerEnd == std::string_view::npos)
	{
		throw std::runtime_error{ "Incomplete HTTP response: " + std::string{ raw } };
	}

	HttpResponse response;
	const std::string_view headerBlock = raw.substr(0, headerEnd);
	response.headers = std::string{ headerBlock };
	response.body = std::string{ raw.substr(headerEnd + 4) };

	const auto lineEnd = headerBlock.find("\r\n");
	const std::string_view statusLine = headerBlock.substr(0, lineEnd);

	const auto firstSpace = statusLine.find(' ');
	if (firstSpace == std::string_view::npos)
	{
		throw std::runtime_error{ "Malformed status line: " + std::string{ statusLine } };
	}

	const auto secondSpace = statusLine.find(' ', firstSpace + 1);
	const std::string_view codeText = statusLine.substr(
		firstSpace + 1,
		secondSpace == std::string_view::npos ? std::string_view::npos : secondSpace - firstSpace - 1);

	response.status = std::stoi(std::string{ codeText });

	std::size_t position = 0;
	while (position < headerBlock.size())
	{
		const auto end = headerBlock.find("\r\n", position);
		const std::string_view line = headerBlock.substr(
			position,
			end == std::string_view::npos ? std::string_view::npos : end - position);

		const auto colon = line.find(':');
		if (colon != std::string_view::npos)
		{
			const std::string name = ToLower(Trim(line.substr(0, colon)));
			if (name == "content-type")
			{
				response.contentType = Trim(line.substr(colon + 1));
			}
		}

		if (end == std::string_view::npos)
		{
			break;
		}

		position = end + 2;
	}

	return response;
}

HttpResponse HttpGet(std::uint16_t port, std::string_view target)
{
	return ParseResponse(SendRaw(port, BuildRequest("GET", target)));
}

class ServerTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		try
		{
			s_server = std::make_unique<ServerProcess>(BinaryPath(), PublicPath());
		}
		catch (const std::exception& error)
		{
			s_startError = error.what();
		}
	}

	static void TearDownTestSuite()
	{
		s_server.reset();
	}

	void SetUp() override
	{
		ASSERT_TRUE(s_server != nullptr) << s_startError;
	}

	static std::uint16_t Port()
	{
		return s_server->Port();
	}

	static std::filesystem::path PublicDir()
	{
		return PublicPath();
	}

	static inline std::unique_ptr<ServerProcess> s_server;
	static inline std::string s_startError;
};

TEST_F(ServerTest, ServesIndexAndRoot)
{
	const std::string expected = ReadTextFile(PublicDir() / "index.html");
	ASSERT_FALSE(expected.empty());

	const HttpResponse root = HttpGet(Port(), "/");
	ASSERT_EQ(root.status, 200);
	EXPECT_EQ(root.contentType, "text/html");
	EXPECT_NE(root.headers.find("Connection: close"), std::string::npos);
	EXPECT_EQ(root.body, expected);

	const HttpResponse index = HttpGet(Port(), "/index.html");
	ASSERT_EQ(index.status, 200);
	EXPECT_EQ(index.contentType, "text/html");
	EXPECT_EQ(index.body, expected);
	EXPECT_EQ(root.body, index.body);
}

TEST_F(ServerTest, ServesStylesheet)
{
	const std::string expected = ReadTextFile(PublicDir() / "style.css");
	ASSERT_FALSE(expected.empty());

	const HttpResponse response = HttpGet(Port(), "/style.css");
	ASSERT_EQ(response.status, 200);
	EXPECT_EQ(response.contentType, "text/css");
	EXPECT_EQ(response.body, expected);
}

TEST_F(ServerTest, ServesPngWithMatchingSha256)
{
	const std::filesystem::path pngPath = PublicDir() / "test.png";
	const std::string expected = ReadTextFile(pngPath);
	ASSERT_FALSE(expected.empty());

	const HttpResponse response = HttpGet(Port(), "/test.png");
	ASSERT_EQ(response.status, 200);
	EXPECT_EQ(response.contentType, "image/png");
	EXPECT_EQ(response.body.size(), expected.size());
	EXPECT_EQ(response.body, expected);
	EXPECT_EQ(Sha256Hex(response.body), Sha256Hex(expected));
}

TEST_F(ServerTest, ReturnsNotFoundAndKeepsRunning)
{
	const HttpResponse missing = HttpGet(Port(), "/does_not_exist.html");
	EXPECT_EQ(missing.status, 404);
	EXPECT_EQ(missing.contentType, "text/plain");
	EXPECT_EQ(missing.body, "File Not Found");

	const HttpResponse index = HttpGet(Port(), "/index.html");
	EXPECT_EQ(index.status, 200);
}

TEST_F(ServerTest, BlocksDirectoryTraversal)
{
	const std::vector<std::string> targets{
		"/../../etc/passwd",
		"/%2e%2e/%2e%2e/etc/passwd",
		"/%2e%2e/%2e%2e/file",
	};

	for (const auto& target : targets)
	{
		const HttpResponse response = ParseResponse(SendRaw(Port(), BuildRequest("GET", target)));
		EXPECT_TRUE(response.status == 403 || response.status == 404) << target;
		EXPECT_EQ(response.body.find("root:"), std::string::npos) << target;
	}
}

TEST_F(ServerTest, RejectsUnsupportedMethods)
{
	for (const std::string_view method : { "POST", "DELETE", "PUT" })
	{
		const HttpResponse response = ParseResponse(
			SendRaw(Port(), BuildRequest(method, "/")));

		EXPECT_EQ(response.status, 405) << method;
		EXPECT_EQ(response.contentType, "text/plain") << method;
		EXPECT_NE(response.headers.find("Allow: GET"), std::string::npos) << method;
	}
}

TEST_F(ServerTest, RejectsMalformedRequest)
{
	const HttpResponse response = ParseResponse(SendRaw(Port(), "GARBAGE\r\n\r\n"));
	EXPECT_EQ(response.status, 400);
	EXPECT_EQ(response.contentType, "text/plain");
	EXPECT_EQ(response.body, "Bad Request");
}

TEST_F(ServerTest, HandlesConcurrentRequests)
{
	std::atomic<int> successes{ 0 };
	std::atomic<int> failures{ 0 };
	std::mutex failureMutex;
	std::string firstFailure;

	const std::uint16_t port = Port();

	const auto recordFailure = [&failureMutex, &firstFailure](std::string message) {
		std::scoped_lock lock{ failureMutex };
		if (firstFailure.empty())
		{
			firstFailure = std::move(message);
		}
	};

	std::vector<std::jthread> workers;
	workers.reserve(REQUEST_COUNT);

	for (int index = 0; index < REQUEST_COUNT; ++index)
	{
		workers.emplace_back(
			[port, &successes, &failures, &recordFailure]() {
				try
				{
					const HttpResponse response = HttpGet(port, "/index.html");

					if (response.status == 200 && !response.body.empty())
					{
						successes.fetch_add(1, std::memory_order_relaxed);
					}
					else
					{
						failures.fetch_add(1, std::memory_order_relaxed);
						recordFailure(
							"Unexpected response: status=" + std::to_string(response.status)
							+ " size=" + std::to_string(response.body.size()));
					}
				}
				catch (const std::exception& error)
				{
					failures.fetch_add(1, std::memory_order_relaxed);
					recordFailure(std::string{ "Request failed: " } + error.what());
				}
			});
	}

	for (auto& worker : workers)
	{
		worker.join();
	}

	EXPECT_EQ(successes.load(), REQUEST_COUNT);
	EXPECT_EQ(failures.load(), 0) << firstFailure;
}

TEST_F(ServerTest, GracefulShutdownReleasesPort)
{
	ServerProcess server{ BinaryPath(), PublicDir() };
	const std::uint16_t port = server.Port();

	EXPECT_EQ(server.StopWithSigint(), 0);

	webserver::UniqueFd probe{ socket(AF_INET, SOCK_STREAM, 0) };
	ASSERT_TRUE(probe.IsValid());

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(port);

	EXPECT_EQ(bind(probe.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
	EXPECT_EQ(listen(probe.Get(), 1), 0);
}

} // namespace
