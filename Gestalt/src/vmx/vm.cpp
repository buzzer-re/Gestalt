#include "vmx/vmx.h"


//
// Handle the CPUID instruction by executing it on the processor normally, returns the leaf for further modifications
//
int vmx::vm::HandleCPUID( GCPUContext* context )
{
	//vmx::vm::CPUID regs = { 0 };
	int regs[4] = { 0 };

	int leaf = ( UINT32 ) context->rax;
	int subLeaf = ( UINT32 ) context->rcx;

	__cpuidex( regs, leaf, subLeaf );
	//__cpuid( ( int* ) &regs, ( UINT32 ) context->rax );

	switch ( leaf )
	{

	case ( ( UINT32 ) 0x40000000 ):
		//
		// Return a maximum supported hypervisor CPUID leaf range and a vendor
		// ID signature as required by the spec.
		//
		__debugbreak();
		regs[0] = ( ( UINT32 ) 0x40000001 );
		regs[1] = 'tseG';  // "MiniVisor   "
		regs[2] = 'vtla';
		regs[3] = 'rosi';
		break;
	}

	context->rax = (UINT64) regs[0];
	context->rbx = (UINT64) regs[1];
	context->rcx = (UINT64) regs[2];
	context->rdx = (UINT64) regs[3];

	vmx::vm::NextInstruction(context);

	return leaf;
}

//
// Passtrought every MSR access
//
int vmx::vm::HandleMSRAccess( GCPUContext* context, MSR_ACCESS AccessType )
{
	UINT64 Value;
	UINT32 MSRId = ( UINT32 ) context->rcx;

	switch ( AccessType )
	{
	case MSR_READ:
		Value = ( UINT64 ) __readmsr( MSRId );
		context->rdx = Value >> 32;
		context->rax = Value & MSR_MASK_LOW;
		break;
	case MSR_WRITE:
		Value = context->rdx << 32;
		Value |= context->rax & MSR_MASK_LOW;
		__writemsr( MSRId, Value );
		break;
	}
	
	vmx::vm::NextInstruction( context );

	return 1;
}


//
// Incriment the RIP register by the instruction length
//
void vmx::vm::NextInstruction( GCPUContext* context )
{
	SIZE_T InstructionLength;
	__vmx_vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH, &InstructionLength );

	context->ExtRegs.rip += InstructionLength;
	
	__vmx_vmwrite( VMCS_GUEST_RIP, context->ExtRegs.rip );
}