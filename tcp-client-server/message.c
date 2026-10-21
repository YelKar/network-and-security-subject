#include "message.h"
#include "config.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

static int RecvExact(const int conn, void* buffer, const size_t length)
{
	uint8_t* data = buffer;
	size_t received = 0;

	while (received < length)
	{
		const ssize_t n = recv(conn, data + received, length - received, 0);

		if (n == 0)
		{
			return 0;
		}

		if (n < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}

			return -1;
		}

		received += (size_t)n;
	}

	return 1;
}

int ReceiveMessage(const int conn, struct Message* msg)
{
	if (msg == NULL)
	{
		return -1;
	}

	struct Header header;

	if (RecvExact(conn, &header, sizeof(header)) != 1)
	{
		return -1;
	}

	if (header.magic != MAGIC || header.version != VERSION)
	{
		return -1;
	}

	if (header.payload_len < 1)
	{
		return -1;
	}

	const size_t messageLen = (size_t)header.payload_len - 1;

	uint8_t number;

	if (RecvExact(conn, &number, sizeof(number)) != 1)
	{
		return -1;
	}

	char* messageText = malloc(messageLen + 1);

	if (messageText == NULL)
	{
		return -1;
	}

	if (messageLen > 0 && RecvExact(conn, messageText, messageLen) != 1)
	{
		free(messageText);
		return -1;
	}

	messageText[messageLen] = '\0';

	*msg = (struct Message){
		.header = header,
		.payload = {
			.number = number,
			.message = messageText,
		}
	};

	return 0;
}

static int SendExact(
	const int conn,
	const void* buffer,
	const size_t length)
{
	const uint8_t* data = buffer;
	size_t sent = 0;

	while (sent < length)
	{
		const ssize_t n = send(
			conn,
			data + sent,
			length - sent,
			0);

		if (n < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}

			return -1;
		}

		if (n == 0)
		{
			return -1;
		}

		sent += (size_t)n;
	}

	return 0;
}

int SendMessageStruct(const int conn, const struct Message msg)
{
	if (msg.header.payload_len < 1)
	{
		return -1;
	}

	const size_t messageLen = (size_t)msg.header.payload_len - 1;

	if (messageLen > 0 && msg.payload.message == NULL)
	{
		return -1;
	}

	if (SendExact(conn, &msg.header, sizeof(msg.header)) < 0)
	{
		return -1;
	}

	if (SendExact(conn, &msg.payload.number, sizeof(msg.payload.number)) < 0)
	{
		return -1;
	}

	if (messageLen > 0 && SendExact(conn, msg.payload.message, messageLen) < 0)
	{
		return -1;
	}

	return 0;
}

int SendMessage(
	const int conn,
	const uint8_t number,
	const char* message,
	const size_t messageLen,
	const enum MessageType type)
{
	if (messageLen > UINT8_MAX - 1 || (messageLen > 0 && message == NULL))
	{
		return -1;
	}

	return SendMessageStruct(
		conn,
		(struct Message){
			.header = {
				.magic = MAGIC,
				.version = VERSION,
				.type = type,
				.payload_len = (uint8_t)(messageLen + 1) },
			.payload = { .number = number, .message = (char*)message } });
}

void FreeMessage(struct Message* msg)
{
	if (msg != NULL && msg->payload.message != NULL)
	{
		free((void*)msg->payload.message);
		msg->payload.message = NULL;
	}
}

void PrintMessage(struct Message message)
{
	printf("Message{type: %d, number: %d, text: %s}\n", message.header.type, message.payload.number, message.payload.message);
}
