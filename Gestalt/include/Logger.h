#pragma once
#include "common.h"

#define DbgInfo(x, ...) DbgPrint("Info: " x "\n", __VA_ARGS__)
#define DbgError(x, ...) DbgPrint("Error: " x "\n", __VA_ARGS__)