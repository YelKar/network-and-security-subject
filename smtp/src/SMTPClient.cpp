#include <smtp/SMTPClient.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace smtp
{

SMTPClient::SMTPClient(std::string host, int port)
	: m_host(std::move(host))
	, m_port(port)
{
}

SMTPClient::~SMTPClient()
{
	Disconnect();
}

void SMTPClient::Connect()
{
	struct addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	std::string portStr = std::to_string(m_port);
	int status = getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &res);
	if (status != 0)
	{
		throw std::runtime_error("DNS resolution error: " + std::string(gai_strerror(status)));
	}

	m_sockFd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (m_sockFd < 0)
	{
		freeaddrinfo(res);
		throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
	}

	if (::connect(m_sockFd, res->ai_addr, res->ai_addrlen) < 0)
	{
		freeaddrinfo(res);
		::close(m_sockFd);
		m_sockFd = -1;
		throw std::runtime_error("Connection failed: " + std::string(strerror(errno)));
	}
	freeaddrinfo(res);

	std::string response = ReadResponse();
	int code = ParseStatusCode(response);
	if (code != 220)
	{
		throw std::runtime_error("Expected greeting 220, got: " + std::to_string(code));
	}
}

void SMTPClient::StartTLS()
{
	ExecuteCommand("EHLO localhost", 250);
	ExecuteCommand("STARTTLS", 220);

	m_sslCtx = SSL_CTX_new(TLS_client_method());
	if (!m_sslCtx)
	{
		throw std::runtime_error("Failed to create SSL_CTX");
	}

	m_ssl = SSL_new(m_sslCtx);
	if (!m_ssl)
	{
		throw std::runtime_error("Failed to create SSL structure");
	}

	SSL_set_fd(m_ssl, m_sockFd);
	SSL_set_tlsext_host_name(m_ssl, m_host.c_str());

	std::cout << "[TLS Handshake starting...]" << std::endl;
	if (SSL_connect(m_ssl) <= 0)
	{
		throw std::runtime_error("TLS handshake failed");
	}
	m_isTLSActive = true;
	std::cout << "[TLS Handshake complete: " << SSL_get_cipher(m_ssl) << "]" << std::endl;

	ExecuteCommand("EHLO localhost", 250);
}

void SMTPClient::Authenticate(const std::string& userLogin, const std::string& appPassword)
{
	ExecuteCommand("AUTH LOGIN", 334);
	ExecuteCommand(Base64Encode(userLogin), 334);
	ExecuteCommand(Base64Encode(appPassword), 235);
	std::cout << "[Authentication successful]" << std::endl;
}

void SMTPClient::SendEmail(const Email& email, const std::string& heloHost)
{
	if (!m_isTLSActive)
	{
		ExecuteCommand("HELO " + heloHost, 250);
	}

	ExecuteCommand("MAIL FROM:<" + email.from + ">", 250);
	ExecuteCommand("RCPT TO:<" + email.to + ">", 250);
	ExecuteCommand("DATA", 354);

	std::ostringstream msg;
	msg << "From: <" << email.from << ">\r\n";
	msg << "To: <" << email.to << ">\r\n";
	msg << "Subject: " << email.subject << "\r\n";
	msg << "MIME-Version: 1.0\r\n";
	msg << "Content-Type: text/plain; charset=UTF-8\r\n";
	msg << "\r\n";
	msg << email.body << "\r\n";
	msg << ".\r\n";

	std::string msgData = msg.str();
	std::cout << "[CLIENT DATA SENDING] (" << msgData.size() << " bytes)..." << std::endl;

	ssize_t bytesWritten = RawWrite(msgData.c_str(), msgData.size());
	if (bytesWritten < 0)
	{
		throw std::runtime_error("Failed to write email data: " + std::string(strerror(errno)));
	}

	std::string response = ReadResponse();
	int code = ParseStatusCode(response);
	if (code != 250)
	{
		throw std::runtime_error("Failed to deliver message body, expected 250, got: " + std::to_string(code));
	}
}

void SMTPClient::Disconnect()
{
	if (m_sockFd >= 0)
	{
		try
		{
			ExecuteCommand("QUIT", 221);
		}
		catch (...)
		{
		}

		if (m_ssl)
		{
			SSL_shutdown(m_ssl);
			SSL_free(m_ssl);
			m_ssl = nullptr;
		}

		if (m_sslCtx)
		{
			SSL_CTX_free(m_sslCtx);
			m_sslCtx = nullptr;
		}

		::close(m_sockFd);
		m_sockFd = -1;
		m_isTLSActive = false;
	}
}

ssize_t SMTPClient::RawWrite(const char* data, size_t size)
{
	if (m_isTLSActive && m_ssl)
	{
		return SSL_write(m_ssl, data, static_cast<int>(size));
	}
	return ::write(m_sockFd, data, size);
}

ssize_t SMTPClient::RawRead(char* buffer, size_t size)
{
	if (m_isTLSActive && m_ssl)
	{
		return SSL_read(m_ssl, buffer, static_cast<int>(size));
	}
	return ::read(m_sockFd, buffer, size);
}

void SMTPClient::SendCommand(const std::string& cmd)
{
	std::string fullCmd = cmd + "\r\n";
	std::cout << "C: " << cmd << "\n";

	ssize_t bytesWritten = RawWrite(fullCmd.c_str(), fullCmd.size());
	if (bytesWritten < 0)
	{
		throw std::runtime_error("Failed to write to socket: " + std::string(strerror(errno)));
	}
}

std::string SMTPClient::ReadResponse()
{
	char buffer[1024];
	std::string fullResponse;

	while (true)
	{
		std::memset(buffer, 0, sizeof(buffer));
		ssize_t bytesRead = RawRead(buffer, sizeof(buffer) - 1);
		if (bytesRead <= 0)
		{
			throw std::runtime_error("Connection closed by server or read error");
		}

		fullResponse.append(buffer, bytesRead);

		size_t lastCrlf = fullResponse.rfind("\r\n");
		if (lastCrlf != std::string::npos)
		{
			size_t prevLineStart = fullResponse.rfind("\n", lastCrlf - 1);
			size_t lineStart = (prevLineStart == std::string::npos) ? 0 : prevLineStart + 1;

			if (lastCrlf - lineStart >= 4 && fullResponse[lineStart + 3] == ' ')
			{
				break;
			}
		}
	}

	std::istringstream stream(fullResponse);
	std::string line;
	while (std::getline(stream, line))
	{
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
		{
			if (line.back() == '\r')
			{
				line.pop_back();
			}
			else
			{
				break;
			}
		}
		if (!line.empty())
		{
			std::cout << "S: " << line << "\n";
		}
	}

	return fullResponse;
}

void SMTPClient::ExecuteCommand(const std::string& cmd, int expectedCode)
{
	SendCommand(cmd);
	std::string response = ReadResponse();
	int actualCode = ParseStatusCode(response);

	if (actualCode != expectedCode)
	{
		throw std::runtime_error("Command failed: '" + cmd + "', expected code " + std::to_string(expectedCode) + ", got " + std::to_string(actualCode));
	}
}

int SMTPClient::ParseStatusCode(const std::string& response)
{
	if (response.size() < 3)
	{
		return -1;
	}
	try
	{
		return std::stoi(response.substr(0, 3));
	}
	catch (...)
	{
		return -1;
	}
}

std::string SMTPClient::Base64Encode(const std::string& input)
{
	static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string output;
	size_t i = 0;
	while (i < input.size())
	{
		uint32_t octetA = i < input.size() ? static_cast<unsigned char>(input[i++]) : 0;
		uint32_t octetB = i < input.size() ? static_cast<unsigned char>(input[i++]) : 0;
		uint32_t octetC = i < input.size() ? static_cast<unsigned char>(input[i++]) : 0;

		uint32_t triple = (octetA << 16) + (octetB << 8) + octetC;

		output.push_back(base64Chars[(triple >> 18) & 0x3F]);
		output.push_back(base64Chars[(triple >> 12) & 0x3F]);
		output.push_back(base64Chars[(triple >> 6) & 0x3F]);
		output.push_back(base64Chars[triple & 0x3F]);
	}

	size_t mod = input.size() % 3;
	if (mod == 1)
	{
		output[output.size() - 1] = '=';
		output[output.size() - 2] = '=';
	}
	else if (mod == 2)
	{
		output[output.size() - 1] = '=';
	}

	return output;
}

} // namespace smtp