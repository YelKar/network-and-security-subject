#pragma once

#include <tcp-server/config.h>
#include <stdint.h>
#include <stdbool.h>

int ConnectToServer(const char* host, uint16_t port, bool verbose);
int CommunicateWithServer(int sock, const struct Config* config, bool verbose);
int RunClientSession(const char* host, uint16_t port, const struct Config* config, bool verbose);
