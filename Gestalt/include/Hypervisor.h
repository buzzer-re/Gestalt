#pragma once


#include "vmx/vmx.h"


class Hypervisor
{
public:
	bool Start();
	bool Enable();
	bool Stop();
	inline bool IsVirtualized() const { return Virtualized; }
	bool DeVirtualize();
private:
	bool VMXVirtualize();
	static int VMXExitHandler( GCPUContext* context, void* HypervisorPtr, VMX_VMEXIT_REASON ExitReason );
	bool Virtualized;
//
// Intel
//
	bool IsVMX;
	VMM VirtualMachineMonitor;
	SIZE_T NumberOfCpus;
	BYTE VmExitBitMap[MAX_VMEXIT_REASON_FILTER];
};