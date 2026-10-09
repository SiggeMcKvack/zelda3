// The default zelda3.ini contents (generated from zelda3.ini at build time into
// src/config_defaults_generated.h). Exposed as an extern and defined exactly once
// in default_config_ini.c so launcher_core_lib can write a default/reset config
// without the multiple-definition that including the generated header twice causes.
#pragma once

extern const char *kDefaultConfigIni;
