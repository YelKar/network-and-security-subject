#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PAYLOAD_LENGTH 255
#define MAX_MESSAGE_LENGTH (MAX_PAYLOAD_LENGTH - 4)
#define SERVER_PORT 5001

#define MAGIC 0x01
#define PROTOCOL_V1 0x01
#define PROTOCOL_V2 0x02

enum MessageType : uint8_t
{
	REQUEST_MESSAGE = 1,
	RESPONSE_MESSAGE = 2,
	ERROR_MESSAGE = 3,
	STOP_MESSAGE = 4,
};

#pragma pack(push, 1)
struct Header
{
	uint8_t magic;
	uint8_t version;
	uint8_t type;
	uint8_t payload_len;
};
#pragma pack(pop)

struct Payload8
{
	int8_t number;
	char* message;
};

struct Payload32
{
	int32_t number;
	char* message;
};

struct Message
{
	struct Header header;
	union
	{
		struct Payload8 payload8;
		struct Payload32 payload32;
	};
};

int GetMessageNumber(const struct Message* msg, int32_t* out_number);
const char* GetMessageText(const struct Message* msg);

int ReceiveMessage(int conn, struct Message* msg);
int SendMessage(int conn, uint8_t version, int32_t number, const char* message, size_t messageLen, enum MessageType type);
void FreeMessage(struct Message* msg);
