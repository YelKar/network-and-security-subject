#include <tcp-server/config.h>
#include <tcp-server/message.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ReadConfig(const int argc, char** argv, struct Config* config)
{
	if (config == NULL)
	{
		return -1;
	}

	if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
	{
		printf("Usage: %s [-v1|-v2] [<name> <number>]\n", argv[0]);
		return 1;
	}

	uint8_t version = PROTOCOL_V2;
	int arg_offset = 1;

	if (argc > 1 && strcmp(argv[1], "-v1") == 0)
	{
		version = PROTOCOL_V1;
		arg_offset++;
	}
	else if (argc > 1 && strcmp(argv[1], "-v2") == 0)
	{
		version = PROTOCOL_V2;
		arg_offset++;
	}

	int remaining_args = argc - arg_offset;
	int32_t number = 0;

	if (remaining_args >= 2)
	{
		char* end;
		errno = 0;
		long val = strtol(argv[arg_offset + 1], &end, 10);

		if (errno == ERANGE || val < INT32_MIN || val > INT32_MAX || end == argv[arg_offset + 1] || *end != '\0')
		{
			fprintf(stderr, "Invalid number: %s\n", argv[arg_offset + 1]);
			return 1;
		}
		number = (int32_t)val;
	}
	else
	{
		printf("Введите число (1-100 для работы, вне диапазона для остановки): ");
		long val;
		while (scanf("%ld", &val) != 1 || val < INT32_MIN || val > INT32_MAX)
		{
			int c;
			while ((c = getchar()) != '\n' && c != EOF)
			{
			}
			printf("Некорректный ввод. Повторите: ");
		}
		number = (int32_t)val;
	}

	if (version == PROTOCOL_V1 && (number < -128 || number > 127))
	{
		fprintf(stderr, "Ошибка: в версии v1 (-v1) число должно быть в диапазоне [-128; 127].\n");
		return 1;
	}

	static char nameBuffer[MAX_MESSAGE_LENGTH];
	const char* name = NULL;

	if (remaining_args >= 1)
	{
		name = argv[arg_offset];
	}
	else
	{
		char hostname[128];
		if (gethostname(hostname, sizeof(hostname)) != 0)
		{
			strncpy(hostname, "localhost", sizeof(hostname));
		}

		if (argv[0] != NULL && strstr(argv[0], "server") != NULL)
		{
			snprintf(nameBuffer, sizeof(nameBuffer), "Server of %s", hostname);
		}
		else
		{
			snprintf(nameBuffer, sizeof(nameBuffer), "Client of %s", hostname);
		}
		name = nameBuffer;
	}

	config->number = number;
	config->name = name;
	config->version = version;

	return 0;
}
