#include "config.h"
#include "message.h"
#include "util.h"
#include "multiprocess-logger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

MAKE_LOGGER("SERVER");

static volatile sig_atomic_t g_stop_server = 0;

static void HandleSignal(int sig)
{
	(void)sig;
	g_stop_server = 1;
}

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

	LOG("Got message: name=\"%s\", number=%d",
		message.payload.message, message.payload.number);

	if (message.payload.number > 100 || message.payload.number < 1)
	{
		LOG_ERR("Got number (out of range 1-100)...");
		FreeMessage(&message);
		return 1;
	}

	PrintResult(config->name, message.payload.message, config->number, message.payload.number);

	LOG("Message sending (name: \"%s\", number: %d)...", config->name, config->number);
	if (SendMessage(conn, (uint8_t)config->number, config->name, strlen(config->name), RESPONSE_MESSAGE) != 0)
	{
		LOG_PERR("SendMessage failed");
	}
	else
	{
		LOG_INFO("Response was sent.");
	}

	FreeMessage(&message);
	return 0;
}

static void ServerLoop(int sock, const struct Config* config)
{
	struct sigaction sa_int;
	memset(&sa_int, 0, sizeof(sa_int));
	sa_int.sa_handler = HandleSignal;
	sigaction(SIGINT, &sa_int, NULL);
	sigaction(SIGTERM, &sa_int, NULL);

	struct sigaction sa_chld;
	memset(&sa_chld, 0, sizeof(sa_chld));
	sa_chld.sa_handler = SIG_IGN;
	sa_chld.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa_chld, NULL);

	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	while (!g_stop_server)
	{
		LOG("Waiting for request (port: %d)...", SERVER_PORT);
		const int conn = accept(sock, (struct sockaddr*)&client, &len);

		if (conn < 0)
		{
			if (errno == EINTR)
			{
				break;
			}
			LOG_PERR("Accept failed");
			break;
		}

		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client.sin_addr, client_ip, sizeof(client_ip));
		LOG_INFO("Client connected %s:%d (fd: %d).", client_ip, ntohs(client.sin_port), conn);

		pid_t pid = fork();

		if (pid < 0)
		{
			LOG_PERR("Fork failed");
			close(conn);
			continue;
		}

		if (pid == 0)
		{
			close(sock);

			int should_stop = ProcessClient(conn, config);

			LOG("Client connection closing (fd: %d)...", conn);
			close(conn);

			if (should_stop)
			{
				kill(getppid(), SIGINT);
			}

			_exit(0);
		}

		LOG("Created child process (pid: %d).", pid);
		close(conn);
	}
}

int main(const int argc, char** argv)
{
	InitLogger();

	struct Config config;
	if (ReadConfig(argc, argv, &config) != 0)
	{
		DestroyLogger();
		return 1;
	}

	LOG_INFO("Config: name=\"%s\", number=%d", config.name, config.number);

	const int sock = SetupServerSocket(SERVER_PORT);
	if (sock < 0)
	{
		DestroyLogger();
		return 1;
	}

	ServerLoop(sock, &config);

	LOG("Server closing (fd: %d)...", sock);
	close(sock);
	LOG_INFO("Server finished.");

	DestroyLogger();
	return 0;
}
