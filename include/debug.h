#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include "config.h"

#ifdef DEBUG
#include <stdio.h>

#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

#endif
