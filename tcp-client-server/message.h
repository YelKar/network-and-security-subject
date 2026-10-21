#pragma once
#include <inttypes.h>
#include <stdio.h>

#define MAX_PAYLOAD_LENGTH 255
#define MAX_MESSAGE_LENGTH (MAX_PAYLOAD_LENGTH - 1)
#define SERVER_PORT 5001

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
	enum MessageType type;
	uint8_t payload_len;
};
#pragma pack(pop)

struct Payload
{
	uint8_t number;
	const char* message;
};

struct Message
{
	struct Header header;
	struct Payload payload;
};

int ReceiveMessage(int conn, struct Message* msg);
int SendMessageStruct(int conn, struct Message msg);
int SendMessage(int conn, uint8_t number, const char* message, size_t messageLen, enum MessageType type);
void PrintMessage(struct Message message);
void FreeMessage(struct Message* msg);
