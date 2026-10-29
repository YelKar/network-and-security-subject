#pragma once

#include <stdint.h>

struct Config
{
	const char* name;
	int32_t number;
	uint8_t version;
};

int ReadConfig(int argc, char** argv, struct Config* config);
