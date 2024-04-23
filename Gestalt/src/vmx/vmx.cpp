#include "vmx/vmx.h"

//
// Verify if VMX is supported by the CPU
//
bool vmx::IsSupported()
{
	CPUID_EAX_01 cpuid = { 0 };
	__cpuid( ( int* ) &cpuid.CpuidVersionInformation.AsUInt, CPUID_VERSION_INFORMATION );

	return cpuid.CpuidFeatureInformationEcx.VirtualMachineExtensions;
}


//
// 
//
bool vmx::VMXInstructionSucceeded()
{
	RFLAGS rflags;
	rflags.AsUInt = __readeflags();

	return !rflags.CarryFlag && !rflags.ZeroFlag && !rflags.OverflowFlag && !rflags.SignFlag && !rflags.ParityFlag;
}


//
// Active VMX extensions by setting the CR4/CR0 values
//
bool vmx::Enable()
{
	PAGED_CODE();
		
	IA32_FEATURE_CONTROL_REGISTER FeatureControlRegister;

	__try
	{
		FeatureControlRegister.AsUInt = __readmsr( IA32_FEATURE_CONTROL );
		if ( FeatureControlRegister.LockBit && ( !FeatureControlRegister.EnableVmxInsideSmx && !FeatureControlRegister.EnableVmxOutsideSmx ) )
		{
			DbgInfo("Lockbit enabled, vmx disabled on bios, aborting...");
			return false;
		}

		FeatureControlRegister.LockBit = 1;
		FeatureControlRegister.EnableVmxInsideSmx = 1;
		FeatureControlRegister.EnableVmxOutsideSmx = 1;
		__writemsr( IA32_FEATURE_CONTROL, FeatureControlRegister.AsUInt );
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		//Logger::DbgError("Exception when enabling IA32_FEATURE_CONTROL values!");
		return false;
	}
	//
	// CR configuration
	//
	__try
	{
		//
		// Write adjusted CR values
		//
		__writecr4( VMXUtils::AdjustCR4( __readcr4() ) );
		__writecr0( VMXUtils::AdjustCR0( __readcr0() ) );
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		KD_DEBUG_BREAK();
		DbgInfo("Exception when writing into CR4/CR0 registers!");
		return false;
	}


	return true;
}


//
// Alloc VMXON region, set revision ID and issue the __vmx_on 
//  
bool vmx::StartVMX( vCPU* vcpu )
{
	PAGED_CODE();

	IA32_VMX_BASIC_REGISTER VMXBasicRegister;

	RtlSecureZeroMemory( &vcpu->vmxonRegion, sizeof( VMXON ) );
	vcpu->Phys.VMXOnRegion = VIRTUAL_TO_PHYSICAL( &vcpu->vmxonRegion );

	__try
	{
		VMXBasicRegister.AsUInt = __readmsr( IA32_VMX_BASIC );
		vcpu->vmxonRegion.RevisionId = VMXBasicRegister.VmcsRevisionId;
		int status = __vmx_on( &vcpu->Phys.VMXOnRegion );

		if ( status )
		{
			DbgError( "vmxon instruction failed with %d\n", status );
			return false;
		}
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		KD_DEBUG_BREAK();
		DbgInfo( "Exception when issueing the __vmx_on instruction!" );
		return false;
	}

	return true;
}


//
// Set the VMCS region, set it and configure
//
bool vmx::ConfigureVMCS( vCPU* vcpu )
{
	// 
	// Set the VMCS region ptr
	//
	IA32_VMX_BASIC_REGISTER VMXBasicRegister;

	vcpu->Phys.VMCS = VIRTUAL_TO_PHYSICAL( &vcpu->vmcs );
	RtlSecureZeroMemory( &vcpu->vmcs, sizeof( VMCS ) );

	__try
	{
		VMXBasicRegister.AsUInt = __readmsr( IA32_VMX_BASIC );
		vcpu->vmcs.RevisionId = VMXBasicRegister.VmcsRevisionId;

		__vmx_vmclear( &vcpu->Phys.VMCS );
		int status = __vmx_vmptrld( &vcpu->Phys.VMCS );

		if ( status )
		{
			DbgInfo( "vmptrld failed with %d\n", status );
			return false;
		}
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		DbgInfo( "An exception hanpped when running vmclear & vmptrld instructions!" );
		return false;
	}

	return ConfigureVMCSFields( vcpu->state );
}


//
// Configure all the VMCS fields necessary to switch to guest/virtualized mode
//
bool vmx::ConfigureVMCSFields( GlobalState* state )
{
	IA32_VMX_ENTRY_CTLS_REGISTER VMEntryControls;
	IA32_VMX_EXIT_CTLS_REGISTER VMExitControls;
	IA32_VMX_PINBASED_CTLS_REGISTER PinBasedControls;
	IA32_VMX_PROCBASED_CTLS_REGISTER PrimaryProcBasedControls;
	IA32_VMX_PROCBASED_CTLS2_REGISTER SecondaryProcBasedControls;

	_sgdt( &state->GuestState.GDTR);
	__sidt( &state->GuestState.IDTR );

	_sgdt( &state->HostState.GDTR );
	__sidt( &state->HostState.IDTR );
	
	__vmx_vmwrite( VMCS_GUEST_ACTIVITY_STATE, 0 );
	//
	// Guest CR values
	//
	__vmx_vmwrite( VMCS_GUEST_CR0, __readcr0() );
	__vmx_vmwrite( VMCS_GUEST_CR3, __readcr3() );
	__vmx_vmwrite( VMCS_GUEST_CR4, __readcr4() );
	//
	// RFLAGS & MSR
	//
	__vmx_vmwrite( VMCS_GUEST_DEBUGCTL,		__readmsr( IA32_DEBUGCTL ) );
	__vmx_vmwrite( VMCS_GUEST_SYSENTER_ESP, __readmsr( IA32_SYSENTER_ESP ) );
	__vmx_vmwrite( VMCS_GUEST_SYSENTER_EIP, __readmsr( IA32_SYSENTER_EIP ) );
	__vmx_vmwrite( VMCS_GUEST_SYSENTER_CS,	__readmsr( IA32_SYSENTER_CS ) );
	__vmx_vmwrite( VMCS_GUEST_RFLAGS, __readeflags() );
	//
	// Fill Segment selector information
	//
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_CS, state->GuestState.GDTR.BaseAddress, __readcs() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_DS, state->GuestState.GDTR.BaseAddress, __readds() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_SS, state->GuestState.GDTR.BaseAddress, __readss() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_ES, state->GuestState.GDTR.BaseAddress, __reades() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_FS, state->GuestState.GDTR.BaseAddress, __readfs() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_GS, state->GuestState.GDTR.BaseAddress, __readgs() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_LDTR, state->GuestState.GDTR.BaseAddress, __readldtr() );
	FILL_SEGMENT_SELECTOR( VMCS_GUEST_TR, state->GuestState.GDTR.BaseAddress, __readtr() );
	__vmx_vmwrite( VMCS_GUEST_FS_BASE, __readmsr( IA32_FS_BASE ) );
	__vmx_vmwrite( VMCS_GUEST_GS_BASE, __readmsr( IA32_GS_BASE ) );
	__vmx_vmwrite( VMCS_GUEST_IDTR_BASE, state->GuestState.IDTR.BaseAddress );
	__vmx_vmwrite( VMCS_GUEST_GDTR_BASE, state->GuestState.GDTR.BaseAddress );
	__vmx_vmwrite( VMCS_GUEST_GDTR_LIMIT, state->GuestState.GDTR.Limit );
	__vmx_vmwrite( VMCS_GUEST_IDTR_LIMIT, state->GuestState.IDTR.Limit );
	//
	// Link pointer
	//
	__vmx_vmwrite( VMCS_GUEST_VMCS_LINK_POINTER, MAXUINT64 );
	//
	// VM Entry/Exit controls fields
	//
	VMEntryControls.AsUInt = 0;
	VMExitControls.AsUInt = 0;
	VMExitControls.HostAddressSpaceSize = 1;
	VMEntryControls.Ia32EModeGuest = 1;

	VMExitControls.AsUInt = VMXUtils::AdjustControlValue( VmxVmExitControls, VMExitControls.AsUInt );
	VMEntryControls.AsUInt = VMXUtils::AdjustControlValue( VmxVmEntryControls, VMEntryControls.AsUInt );
	//
	// PinBasedControls
	//
	PinBasedControls.AsUInt = VMXUtils::AdjustControlValue( VmxPinBasedControls, 0 );

	//
	// ProcBased controls fields
	//
	PrimaryProcBasedControls.AsUInt = 0;
	PrimaryProcBasedControls.ActivateSecondaryControls = 1;
	PrimaryProcBasedControls.UseMsrBitmaps = 1;
	PrimaryProcBasedControls.AsUInt = VMXUtils::AdjustControlValue( VmxProcessorBasedControls, PrimaryProcBasedControls.AsUInt );

	SecondaryProcBasedControls.AsUInt = 0;
	SecondaryProcBasedControls.EnableRdtscp = 1;
	SecondaryProcBasedControls.EnableXsaves = 1;
	SecondaryProcBasedControls.EnableInvpcid = 1;
	SecondaryProcBasedControls.AsUInt = VMXUtils::AdjustControlValue( VmxProcessorBasedControls2, SecondaryProcBasedControls.AsUInt );
	//
	// Write control fields values
	//
	__vmx_vmwrite( VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS, PinBasedControls.AsUInt );
	__vmx_vmwrite( VMCS_CTRL_PRIMARY_VMEXIT_CONTROLS, VMExitControls.AsUInt );
	__vmx_vmwrite( VMCS_CTRL_VMENTRY_CONTROLS, VMEntryControls.AsUInt );
	__vmx_vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, PrimaryProcBasedControls.AsUInt );
	__vmx_vmwrite( VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, SecondaryProcBasedControls.AsUInt );
	//
	// Load MSR bitmap
	//
	__vmx_vmwrite( VMCS_CTRL_MSR_BITMAP_ADDRESS, VIRTUAL_TO_PHYSICAL( &state->MSRBitMap ) );
	//
	// Shadow CR0/4
	//
	__vmx_vmwrite( VMCS_CTRL_CR0_READ_SHADOW, __readcr0() );
	__vmx_vmwrite( VMCS_CTRL_CR4_READ_SHADOW, __readcr4() & ~CR4_VMX_ENABLE_BIT );
	//
	// Configure host state area
	//
	//
	// Host CR
	//
	__vmx_vmwrite( VMCS_HOST_CR0, __readcr0() );
	__vmx_vmwrite( VMCS_HOST_CR3, __readcr3() );
	__vmx_vmwrite( VMCS_HOST_CR4, __readcr4() );
	//
	// Host segment selectors
	//
	__vmx_vmwrite( VMCS_HOST_GDTR_BASE, state->HostState.GDTR.BaseAddress );
	__vmx_vmwrite( VMCS_HOST_IDTR_BASE, state->HostState.IDTR.BaseAddress );
	__vmx_vmwrite( VMCS_HOST_CS_SELECTOR, MASK_SELECTOR( __readcs() ) );
	__vmx_vmwrite( VMCS_HOST_SS_SELECTOR, MASK_SELECTOR( __readss() ) );
	__vmx_vmwrite( VMCS_HOST_DS_SELECTOR, MASK_SELECTOR( __readds() ) );
	__vmx_vmwrite( VMCS_HOST_ES_SELECTOR, MASK_SELECTOR( __reades() ) );
	__vmx_vmwrite( VMCS_HOST_FS_SELECTOR, MASK_SELECTOR( __readfs() ) );
	__vmx_vmwrite( VMCS_HOST_GS_SELECTOR, MASK_SELECTOR( __readgs() ) );
	__vmx_vmwrite( VMCS_HOST_TR_SELECTOR, MASK_SELECTOR( __readtr() ) );
	__vmx_vmwrite( VMCS_HOST_FS_BASE, __readmsr( IA32_FS_BASE ) );
	__vmx_vmwrite( VMCS_HOST_GS_BASE, __readmsr( IA32_GS_BASE ) );
	__vmx_vmwrite( VMCS_HOST_SYSENTER_CS, __readmsr( IA32_SYSENTER_CS ) );
	__vmx_vmwrite( VMCS_HOST_TR_BASE, VMXUtils::GetSegmentBase( state->HostState.GDTR.BaseAddress, __readtr() ) );

	//
	// Host RSP should point to the middle of the stack, that way we avoid PAGE faults when pushing into it
	//
	__vmx_vmwrite( VMCS_HOST_RSP, state->HostState.RSP + ( state->HostState.StackSize/2 ) );
	__vmx_vmwrite( VMCS_HOST_RIP, ( size_t ) vmx::__vmx_default_exit_handler );

	//
	// Simple, isn't ?
	//

	return true;
}

size_t vmx::GetVMXErrorCode()
{
	size_t error;
	__vmx_vmread( VMCS_VM_INSTRUCTION_ERROR, &error );

	return error;
}

//
// Set guest RIP/RSP and a custom handler to be invoked after the default __vmx_exit_handler
//
int vmx::VMLaunch(UINT64 Rip, UINT64 Rsp, int ( *Handler )( GCPUContext* context, void* Args, VMX_VMEXIT_REASON ExitReasonBitMap ), void* Args, BYTE* ExitReasonBitMap)
{
	VMExit.Handler = Handler;
	VMExit.ExitReasonBitMap = ExitReasonBitMap;
	VMExit.Args = Args;
	//
	// RIP & RSP from both Guest/Host
	//
	__vmx_vmwrite( VMCS_GUEST_RIP, Rip );
	__vmx_vmwrite( VMCS_GUEST_RSP, Rsp );

	return __vmx_vmlaunch();
}

//
// VMExit wrapper, it can handle VM exit events that are not inteeded to be handled by the VMExitBitmap
//

int vmx::VMExitHandler( GCPUContext* gcpuContext )
{
	int status = 0;
	VMX_VMEXIT_REASON ExitReason;
	UINT64 Rip;

	__vmx_vmread( VMCS_GUEST_RIP, &gcpuContext->ExtRegs.rip );
	__vmx_vmread( VMCS_GUEST_RSP, &gcpuContext->ExtRegs.rsp );
	__vmx_vmread( VMCS_GUEST_RFLAGS, &gcpuContext->ExtRegs.rflags.AsUInt );

	__vmx_vmread( VMCS_EXIT_REASON, ( size_t* ) &ExitReason.AsUInt );
/*
	if ( VMExit.ExitReasonBitMap[ExitReason.BasicExitReason] )
	{
		return vmx::VMExit.Handler( gcpuContext, VMExit.Args, ExitReason );
	}
	*/

	Rip = gcpuContext->ExtRegs.rip;
	//
	// Default handler
	//
	switch ( ExitReason.BasicExitReason )
	{
	case vmexit_cpuid:
		vmx::vm::HandleCPUID( gcpuContext );
		status = 1; // HandleCPUID returns the leaf, we don't really care about it here
		break;
	case vmexit_rdmsr:
		status = vmx::vm::HandleMSRAccess( gcpuContext, vmx::vm::MSR_ACCESS::MSR_READ );
		break;
	case vmexit_wrmsr:
		status = vmx::vm::HandleMSRAccess( gcpuContext, vmx::vm::MSR_ACCESS::MSR_WRITE );
		break;
	default:
		status = 0;
		break;
	}

	if ( !status )
	{
		DbgInfo( "VMExit unhandled: %d\nRIP: 0x%x\n", ExitReason.BasicExitReason, Rip );
		KD_DEBUG_BREAK();
	}

	return status;
}

