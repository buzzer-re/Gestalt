#include "vmx/vmx.h"


//
// Handle the CPUID instruction by executing it on the processor normally, returns the leaf for further modifications
//
int vmx::vm::HandleCPUID( GuestContext* context )
{
	vmx::vm::CPUID regs;

	regs.rax = context->cpu->rax;
	regs.rbx = context->cpu->rbx;
	regs.rcx = context->cpu->rcx;
	regs.rdx = context->cpu->rdx;
	
	int leaf = ( UINT32 ) regs.rax;

	__cpuidex( ( int* ) &regs, ( int ) context->cpu->rax, ( int ) context->cpu->rcx );
	//__cpuid( ( int* ) &regs, ( UINT32 ) context->cpu->rax );

	context->cpu->rax = regs.rax;
	context->cpu->rbx = regs.rbx;
	context->cpu->rcx = regs.rcx;
	context->cpu->rdx = regs.rdx;

	vmx::vm::NextInstruction(context);

	return leaf;
}

//
// Passtrought every MSR access
//
int vmx::vm::HandleMSRAccess( GuestContext* context, MSR_ACCESS AccessType )
{
	UINT64 Value;
	UINT32 MSRId = ( UINT32 ) context->cpu->rcx;

	switch ( AccessType )
	{
	case MSR_READ:
		Value = ( UINT64 ) __readmsr( ( UINT32 ) MSRId );
		context->cpu->rdx = Value >> 32;
		context->cpu->rax = Value & MSR_MASK_LOW;
		break;
	case MSR_WRITE:
		Value = context->cpu->rdx << 32;
		Value |= context->cpu->rax & MSR_MASK_LOW;
		__writemsr( ( UINT32 ) MSRId, Value );
	}
	
	vmx::vm::NextInstruction( context );

	return 1;
}


//
// Incriment the RIP register by the instruction length
//
void vmx::vm::NextInstruction( GuestContext* context )
{
	SIZE_T InstructionLength;
	__vmx_vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH, &InstructionLength );

	context->ExtRegs.rip += InstructionLength;
	
	__vmx_vmwrite( VMCS_GUEST_RIP, context->ExtRegs.rip );
}