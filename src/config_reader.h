#pragma once

#include "config.h"
#include <stdbool.h>

// Simple INI parser for launcher
// Reads zelda3.ini values into Config struct

bool ConfigReader_Read(const char *path, Config *config);
