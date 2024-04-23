#pragma once

#include <ntddk.h>
#include <intrin.h>

#include "ia32/ia32.h"
#include "Logger.h"

#define VIRTUAL_TO_PHYSICAL(ADDRESS) MmGetPhysicalAddress ( (PVOID) ADDRESS ).QuadPart
#define ALIGN_TO_PAGE(ADDRESS) (UINT64) ( ( ( UINT64 ) ADDRESS + PAGE_SIZE - 1 ) & ~( PAGE_SIZE - 1 ) )

#define KD_DEBUG_BREAK() \
	if (!KD_DEBUGGER_NOT_PRESENT) __debugbreak();

typedef unsigned char BYTE;