#pragma once

#include "common.h"


extern "C"
{
	UINT16 __reades();
	UINT16 __readcs();
	UINT16 __readds();
	UINT16 __readss();
	UINT16 __readfs();
	UINT16 __readgs();
	UINT16 __readldtr();
	UINT16 __readtr();

	UINT64 __get_rip();
	UINT64 __get_rsp();

	UINT32 __segment_access_rights( UINT16 SegmentValue );
	void _sgdt( void* );

}