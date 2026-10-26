#include <tcp-server/message.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int GetMessageNumber(const struct Message* msg, int32_t* out_number)
{
	if (msg == NULL || out_number == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	switch (msg->header.version)
	{
	case PROTOCOL_V1:
		*out_number = (int32_t)msg->payload8.number;
		return 0;
	case PROTOCOL_V2:
		*out_number = msg->payload32.number;
		return 0;
	default:
		errno = EPROTONOSUPPORT;
		return -1;
	}
}

const char* GetMessageText(const struct Message* msg)
{
	if (msg == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	switch (msg->header.version)
	{
	case PROTOCOL_V1:
		return msg->payload8.message;
	case PROTOCOL_V2:
		return msg->payload32.message;
	default:
		errno = EPROTONOSUPPORT;
		return NULL;
	}
}

static int RecvExact(const int conn, void* buffer, const size_t length)
{
	uint8_t* data = (uint8_t*)buffer;
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

static int SendExact(const int conn, const void* buffer, const size_t length)
{
	const uint8_t* data = (const uint8_t*)buffer;
	size_t sent = 0;

	while (sent < length)
	{
		const ssize_t n = send(conn, data + sent, length - sent, 0);
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

	if (header.magic != MAGIC)
	{
		errno = EBADMSG;
		return -1;
	}

	if (header.version == PROTOCOL_V1)
	{
		if (header.payload_len < sizeof(int8_t))
		{
			errno = EBADMSG;
			return -1;
		}

		int8_t number;
		if (RecvExact(conn, &number, sizeof(number)) != 1)
		{
			return -1;
		}

		const size_t messageLen = (size_t)header.payload_len - sizeof(int8_t);
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

		msg->header = header;
		msg->payload8.number = number;
		msg->payload8.message = messageText;
		return 0;
	}
	else if (header.version == PROTOCOL_V2)
	{
		if (header.payload_len < sizeof(int32_t))
		{
			errno = EBADMSG;
			return -1;
		}

		uint32_t net_number;
		if (RecvExact(conn, &net_number, sizeof(net_number)) != 1)
		{
			return -1;
		}
		int32_t number = (int32_t)ntohl(net_number);

		const size_t messageLen = (size_t)header.payload_len - sizeof(int32_t);
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

		msg->header = header;
		msg->payload32.number = number;
		msg->payload32.message = messageText;
		return 0;
	}

	errno = EPROTONOSUPPORT;
	return -1;
}

int SendMessage(
	const int conn,
	const uint8_t version,
	const int32_t number,
	const char* message,
	const size_t messageLen,
	const enum MessageType type)
{
	if (messageLen > 0 && message == NULL)
	{
		return -1;
	}

	if (version == PROTOCOL_V1)
	{
		if (number < -128 || number > 127)
		{
			errno = ERANGE;
			return -1;
		}
		if (messageLen + sizeof(int8_t) > MAX_PAYLOAD_LENGTH)
		{
			errno = EMSGSIZE;
			return -1;
		}

		struct Header header = {
			.magic = MAGIC,
			.version = PROTOCOL_V1,
			.type = (uint8_t)type,
			.payload_len = (uint8_t)(sizeof(int8_t) + messageLen)
		};

		int8_t num8 = (int8_t)number;
		if (SendExact(conn, &header, sizeof(header)) != 0 ||
		    SendExact(conn, &num8, sizeof(num8)) != 0)
		{
			return -1;
		}

		if (messageLen > 0 && SendExact(conn, message, messageLen) != 0)
		{
			return -1;
		}
		return 0;
	}
	else if (version == PROTOCOL_V2)
	{
		if (messageLen + sizeof(int32_t) > MAX_PAYLOAD_LENGTH)
		{
			errno = EMSGSIZE;
			return -1;
		}

		struct Header header = {
			.magic = MAGIC,
			.version = PROTOCOL_V2,
			.type = (uint8_t)type,
			.payload_len = (uint8_t)(sizeof(int32_t) + messageLen)
		};

		uint32_t net_number = htonl((uint32_t)number);
		if (SendExact(conn, &header, sizeof(header)) != 0 ||
		    SendExact(conn, &net_number, sizeof(net_number)) != 0)
		{
			return -1;
		}

		if (messageLen > 0 && SendExact(conn, message, messageLen) != 0)
		{
			return -1;
		}
		return 0;
	}

	errno = EPROTONOSUPPORT;
	return -1;
}

void FreeMessage(struct Message* msg)
{
	if (msg == NULL)
	{
		return;
	}

	const char* text = GetMessageText(msg);
	if (text != NULL)
	{
		free((void*)text);
		if (msg->header.version == PROTOCOL_V1)
		{
			msg->payload8.message = NULL;
		}
		else if (msg->header.version == PROTOCOL_V2)
		{
			msg->payload32.message = NULL;
		}
	}
}
