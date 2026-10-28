#pragma once

#include <string>

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

namespace smtp {

struct Email {
	std::string from;
	std::string to;
	std::string subject;
	std::string body;
};

class SMTPClient {
public:
	SMTPClient(std::string host, int port);
	~SMTPClient();

	SMTPClient(const SMTPClient&) = delete;
	SMTPClient& operator=(const SMTPClient&) = delete;

	void Connect();
	void StartTLS();
	void Authenticate(const std::string& userLogin, const std::string& appPassword);
	void SendEmail(const Email& email, const std::string& heloHost = "localhost");
	void Disconnect();

private:
	std::string m_host;
	int m_port;
	int m_sockFd{-1};

	SSL_CTX* m_sslCtx{nullptr};
	SSL* m_ssl{nullptr};
	bool m_isTLSActive{false};

	ssize_t RawWrite(const char* data, size_t size);
	ssize_t RawRead(char* buffer, size_t size);

	void SendCommand(const std::string& cmd);
	std::string ReadResponse();
	void ExecuteCommand(const std::string& cmd, int expectedCode);

	static int ParseStatusCode(const std::string& response);
	static std::string Base64Encode(const std::string& input);
};

} // namespace smtp