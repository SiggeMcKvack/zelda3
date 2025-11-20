#pragma once

#include "config.h"
#include <stdbool.h>

// Config Writer API
// Generates zelda3.ini files from Config structures

// Initialize Config structure with default values
// Matches defaults from zelda3.ini template
void ConfigWriter_InitDefaults(Config *config);

// Write Config structure to INI file
// Returns true on success, false on failure
// Error messages written to stderr
bool ConfigWriter_Write(const char *path, const Config *config);

// Validate Config structure
// Returns true if valid, false otherwise
// error_buf: buffer for error message (256 bytes recommended)
// error_buf_size: size of error buffer
bool ConfigWriter_Validate(const Config *config, char *error_buf, size_t error_buf_size);
