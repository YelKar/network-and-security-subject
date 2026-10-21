#pragma once

enum
{
	MAGIC = 0x01,
	VERSION = 0x01,
};

struct Config
{
	const char* name;
	int number;
};

int ReadConfig(int argc, char** argv, struct Config* config);
