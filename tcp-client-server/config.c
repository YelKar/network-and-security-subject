#include "config.h"
#include "message.h"

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

	if (argc == 2 && strcmp(argv[1], "-h") == 0)
	{
		printf("Usage: %s [<name> <number>]\n", argv[0]);
		return 1;
	}

	int number;

	if (argc > 2)
	{
		char *end;

		errno = 0;
		number = (int)strtol(argv[2], &end, 10);

		if (errno != 0 || end == argv[2] || *end != '\0')
		{
			printf("invalid number %s\n", argv[2]);
			return 1;
		}
	}
	else
	{
		printf("Введите число (1-100): ");
		while (scanf("%d", &number) != 1)
		{
			int c;
			while ((c = getchar()) != '\n' && c != EOF)
			{
			}
			printf("Число не распознано.\nПовторите ввод (1-100): ");
		}
	}

	if (number > 100 || number < 1)
	{
		number = 0;
	}

	static char nameBuffer[MAX_MESSAGE_LENGTH];
	const char *name = NULL;

	if (argc > 1)
	{
		name = argv[1];
	}
	else
	{
		char hostname[MAX_MESSAGE_LENGTH];

		if (gethostname(hostname, sizeof(hostname)) != 0)
		{
			perror("gethostname");
			return 1;
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

	return 0;
}
