#pragma once
#include "common.h"
#include "vmxUtils.h"
#include "ia32/x64.h"

#define MAX_VMEXIT_REASON_FILTER 64
#define MSR_MASK_LOW ((UINT64) (UINT32) -1)
#define CPUID_HV_VENDOR_INFORMATION ( UINT32 ) 0x40000000
#define MASK_SELECTOR(VALUE) VALUE & ~0x7


struct State
{
	UINT64 RSP;
	UINT64 RIP;
	UINT64 StackSize;

	SEGMENT_DESCRIPTOR_REGISTER_64 GDTR;
	SEGMENT_DESCRIPTOR_REGISTER_64 IDTR;
};

struct GlobalState
{
	__declspec( align(PAGE_SIZE) ) VMX_MSR_BITMAP MSRBitMap;
	State GuestState;
	State HostState;
};

struct PhysicalAddresses
{
	UINT64 VMCS;
	UINT64 VMXOnRegion;
};


struct vCPU
{
	__declspec( align( PAGE_SIZE ) ) VMCS vmcs;
	__declspec( align( PAGE_SIZE ) ) VMXON vmxonRegion;

	int CpuNumber;
	PhysicalAddresses Phys;
	GlobalState* state;
};


struct VMM
{
	vCPU* vcpu;
	GlobalState state;
};

// 
// VMExit guest state structures
//

struct GCPUExtendedRegs
{
	UINT64 rsp;
	UINT64 rip;
	RFLAGS rflags;
};

#include <pshpack1.h>
struct GCPUContext
{
	__m128 xmm[6];
	void* padding;
	UINT64 r15;
	UINT64 r14;
	UINT64 r13;
	UINT64 r12;
	UINT64 r11;
	UINT64 r10;
	UINT64 r9;
	UINT64 r8;
	UINT64 rdi;
	UINT64 rsi;
	UINT64 rbp;
	UINT64 rbx;
	UINT64 rdx;
	UINT64 rcx;
	UINT64 rax;
	GCPUExtendedRegs ExtRegs;
};
#include <poppack.h>

struct GuestContext
{
	GCPUContext* cpu;
	GCPUExtendedRegs ExtRegs;
};

namespace vmx
{
	//
	// Guest specific functions
	//
	namespace vm
	{
		enum MSR_ACCESS
		{
			MSR_READ,
			MSR_WRITE
		};

		enum CPUID_REGISTERS
		{
			eax = 0,
			ebx,
			ecx,
			edx,
		};

		int HandleCPUID( GCPUContext* context, bool hide );
		int HandleMSRAccess( GCPUContext* context, MSR_ACCESS AccessType );
		void NextInstruction( GCPUContext* context );
		
	}

	static struct
	{
		int ( *Handler )( GCPUContext* context, void* Args, VMX_VMEXIT_REASON ExitReason );
		void* Args;
		BYTE* ExitReasonBitMap;
	} VMExit;

	bool IsSupported();
	bool Enable();

	bool StartVMX( vCPU* vcpu );
	bool ConfigureVMCS( vCPU* vcpu );
	bool ConfigureVMCSFields (GlobalState* state);

	size_t GetVMXErrorCode();
	
	extern "C" inline int VMLaunch( UINT64 Rip, UINT64 Rsp, int ( *Handler )( GCPUContext* context, void* Args, VMX_VMEXIT_REASON ExitReason ), void* Argss, BYTE * ExitReasonBitMap );
	extern "C" int __vmx_default_exit_handler();
	extern "C" int VMExitHandler( GCPUContext * gcpuContext );


	// helper functions, TODO

	bool VMXInstructionSucceeded();
}