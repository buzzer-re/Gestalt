#include "vmx/vmx.h"


//
// Handle the CPUID instruction by executing it on the processor normally, returns the leaf for further modifications
//
int vmx::vm::HandleCPUID( GCPUContext* context, bool hide )
{
	//vmx::vm::CPUID regs = { 0 };
	CPUID_EAX_01 VersionInformation;
	int regs[4] = { 0 };
	int leaf = ( UINT32 ) context->rax;
	int subLeaf = ( UINT32 ) context->rcx;

	__cpuidex( regs, leaf, subLeaf );


	if ( hide )
	{
		switch ( leaf )
		{
		case CPUID_VERSION_INFORMATION:
			//
			// Clear the Virtual machine extensions flag in the ECX register, not only helps to hide but we can't support other hypervisors
			// because we don't support nested
			//
			VersionInformation.CpuidFeatureInformationEcx.AsUInt = regs[ecx];
			VersionInformation.CpuidFeatureInformationEcx.VirtualMachineExtensions = 0;
			VersionInformation.CpuidFeatureInformationEcx.Reserved2 = 0;
			regs[ecx] = VersionInformation.CpuidFeatureInformationEcx.AsUInt;
			break;
		case CPUID_HV_VENDOR_INFORMATION:
			//
			// Return a maximum supported hypervisor CPUID leaf range and a vendor
			// ID signature as required by the spec.
			//
			regs[eax] = ( ( UINT32 ) 0x40000001 );
			regs[ebx] = 'tseG';  // "MiniVisor   "
			regs[ecx] = 'vtla';
			regs[edx] = 'rosi';
			break;
		}
	}


	context->rax = (UINT64) regs[eax];
	context->rbx = (UINT64) regs[ebx];
	context->rcx = (UINT64) regs[ecx];
	context->rdx = (UINT64) regs[edx];

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