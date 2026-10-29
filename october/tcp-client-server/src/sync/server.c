#include <tcp-server/config.h>
#include <tcp-server/message.h>
#include <tcp-server/util.h>
#include <tcp-server/sync/logger.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

MAKE_LOGGER("SERVER");

static int SetupServerSocket(uint16_t port)
{
	LOG("Socket creating...");
	const int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		LOG_PERR("Socket creation failed");
		return -1;
	}
	LOG("Socket created (fd: %d).", sock);

	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		LOG_PERR("setsockopt failed");
		close(sock);
		return -1;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	LOG("Socket binding to port %d...", port);
	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		LOG_PERR("Socket bind failed");
		close(sock);
		return -1;
	}
	LOG_INFO("Socket bound successfully.");

	if (listen(sock, 5) < 0)
	{
		LOG_PERR("Listen failed");
		close(sock);
		return -1;
	}

	return sock;
}

static int ProcessClient(int conn, const struct Config* config)
{
	struct Message message;
	LOG("Parse client message...");

	if (ReceiveMessage(conn, &message) != 0)
	{
		LOG_ERR("Message parsing error.");
		return 0;
	}

	int32_t client_number = 0;
	if (GetMessageNumber(&message, &client_number) != 0)
	{
		LOG_PERR("Failed to extract client number");
		FreeMessage(&message);
		return 0;
	}

	const char* client_name = GetMessageText(&message);
	LOG("Got message (v%d): name=\"%s\", number=%d",
	    message.header.version, client_name ? client_name : "", client_number);

	PrintResult(config->name, client_name ? client_name : "", config->number, client_number);

	LOG("Message sending (v%d, name: \"%s\", number: %d)...",
	    message.header.version, config->name, config->number);

	if (SendMessage(conn, message.header.version, config->number, config->name, strlen(config->name), RESPONSE_MESSAGE) != 0)
	{
		LOG_PERR("SendMessage failed");
	}
	else
	{
		LOG_INFO("Response was sent.");
	}

	FreeMessage(&message);

	if (client_number < 1 || client_number > 100)
	{
		LOG_INFO("Received number %d out of range [1; 100]. Initiating server shutdown...", client_number);
		return 1;
	}

	return 0;
}

static void ServerLoop(int sock, const struct Config* config)
{
	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	for (;;)
	{
		LOG("Waiting for request (port: %d)...", SERVER_PORT);
		const int conn = accept(sock, (struct sockaddr*)&client, &len);

		if (conn < 0)
		{
			LOG_PERR("Accept failed");
			break;
		}

		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client.sin_addr, client_ip, sizeof(client_ip));
		LOG_INFO("Client connected %s:%d (fd: %d).", client_ip, ntohs(client.sin_port), conn);

		int should_stop = ProcessClient(conn, config);

		LOG("Client connection closing (fd: %d)...", conn);
		close(conn);

		if (should_stop)
		{
			break;
		}
	}
}

int main(const int argc, char** argv)
{
	struct Config config;
	if (ReadConfig(argc, argv, &config) != 0)
	{
		return 1;
	}

	LOG_INFO("Config: name=\"%s\", number=%d", config.name, config.number);

	const int sock = SetupServerSocket(SERVER_PORT);
	if (sock < 0)
	{
		return 1;
	}

	ServerLoop(sock, &config);

	LOG("Server closing listening socket (fd: %d)...", sock);
	close(sock);
	LOG_INFO("Server finished.");

	return 0;
}
