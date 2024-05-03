#include "Hypervisor.h"


//
// Starts the Gestalt hypervisor engine, it will check if it already enabled and start it on all cores
//
bool Hypervisor::Start()
{

	/*
	if ( IsSVM() )
	{
		//
		// AMD: TODO (Learn)
		//
		return SVMVirtualize();
	}
	else if ( IsVMX() )
	{
	*/
	IsVMX = true;
	return VMXVirtualize();
	/*  }
	else
	{
		// Well, not my task them
		return false
	}
	*/
}


//
// Check if the virtualization extensions are avaiable and enable it, wrapper for both Intel & AMD
//
bool Hypervisor::Enable()
{
	//
	// Future release Check the CPU virtualization type, VMX or SVM
	//
	return ( vmx::IsSupported() && vmx::Enable() ) /* || ( svm::IsSupported() && svm::Enable() )*/;
}

bool Hypervisor::Stop()
{
	__vmx_off();
	return false;
}


//
// Virtualize all the processors using Intel-VTx
//
#pragma optimize("", off)
bool Hypervisor::VMXVirtualize()
{
	if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
		KeRaiseIrqlToDpcLevel();
	
	PAGED_CODE();

	PHYSICAL_ADDRESS High = { 0 };
	High.QuadPart = MAXUINT64;
	Virtualized = false;
	int status;
	this->NumberOfCpus = KeQueryActiveProcessors();
	this->VirtualMachineMonitor.vcpu = ( vCPU* ) MmAllocateContiguousMemory( sizeof( vCPU ) * NumberOfCpus, High );

	if ( !VirtualMachineMonitor.vcpu )
	{
		DbgInfo( "Unable to allocate vcpu structures, system is out-of-memory!" );
		return false;
	}

	//
	// Zero-out important structures
	//
	RtlSecureZeroMemory( VirtualMachineMonitor.vcpu, sizeof( vCPU ) * NumberOfCpus );
	RtlSecureZeroMemory( &VirtualMachineMonitor.state.MSRBitMap, sizeof( VMX_MSR_BITMAP ) );
	RtlSecureZeroMemory( &VirtualMachineMonitor.state, sizeof( GlobalState ) );
	RtlSecureZeroMemory( VmExitBitMap, MAX_EVENT_FILTERS_COUNT );
	//
	// The only information that we need to care here is the stack size/allocation
	// the vmx library has it's own custom vm_exit handler that will call ours when calling the VMLaunch function
	//
	VirtualMachineMonitor.state.HostState.StackSize = PAGE_SIZE;
	VirtualMachineMonitor.state.HostState.RSP = (UINT64) MmAllocateContiguousMemory( PAGE_SIZE, High );

	if ( !VirtualMachineMonitor.state.HostState.RSP )
	{
		DbgInfo( "Unable to allocate Host stack, system is out-of-memory!" );
		MmFreeContiguousMemory( VirtualMachineMonitor.vcpu );
		return false;
	}
	//
	// Enter in VMX operation in each processor and configure the VMCS
	//
	for ( SIZE_T i = 0; i < NumberOfCpus; i ++ )
	{
		KeSetSystemAffinityThread( i );
		DbgInfo( "Entering VMX operation on processor %d\n", ( int ) i );

		VirtualMachineMonitor.vcpu[i].state = &VirtualMachineMonitor.state;

		if ( !vmx::StartVMX( &VirtualMachineMonitor.vcpu[i] ) )
		{
			DbgInfo( "Unable to start VMX on logical processor %d! Aborting...\n", ( int ) i );
			return false;
		}

		DbgInfo( "VMX Operation enabled on %d processor!\n", ( int ) i );
		DbgInfo( "Configuring VMCS..." );

		if ( !vmx::ConfigureVMCS( &VirtualMachineMonitor.vcpu[i] ) )
		{
			DbgInfo( "Unable to configure the VMCS on logical processor %d! Aborting...\n", ( int ) i );
			return false;
		}

		DbgInfo( "VMCS configured on logical processor %d\n", i );
	}

	//
	// Enable VMExits that we want to handle
	//
	VmExitBitMap[vmexit_cpuid] = true;
	VmExitBitMap[vmexit_control_register_access] = true; // Handle CR access to detect SMEP disable
	VmExitBitMap[vmexit_access_to_gdtr_or_idtr] = true; // Detect access to the IDTR (Research that, maybe some syscall hooking approach here ?)
	//
	// Time to fly - Guest switch
	//

	VirtualMachineMonitor.state.GuestState.RSP = __get_rsp(); // Save guest RSP
	VirtualMachineMonitor.state.GuestState.RIP = __get_rip(); // Guest will start from here, but will not repeat the code bellow since we will flip the "Virtualized" boolean

	if ( !Virtualized )
	{
		DbgInfo( "Lanching VM!" );
		Virtualized = true;
		__try
		{
			//
			// Entering in the virtualized world
			//
			status = vmx::VMLaunch( VirtualMachineMonitor.state.GuestState.RIP, VirtualMachineMonitor.state.GuestState.RSP, VMXExitHandler, this, VmExitBitMap );
			//
			// If we reach here, an instruction error has happened
			//
			if ( status )
			{
				KD_DEBUG_BREAK();
				DbgInfo( "Error on virtualize, exited with: %d -> %d\n", status, vmx::GetVMXErrorCode() );
			}

			return false;
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			DbgInfo( "Exception when executing the __vmx_launch instruction, aborting!" );
			KD_DEBUG_BREAK();
			return false;
		}

	}
	else
	{
		DbgInfo( "System Virtualized!\n" );
	}


	return true;
}
#pragma optimize("", on)



//
// VMX VMExit handler, called after the default vm_exit_handler
//
int Hypervisor::VMXExitHandler( GCPUContext* context, void* HypervisorPtr, VMX_VMEXIT_REASON ExitReason )
{
	UNREFERENCED_PARAMETER( HypervisorPtr );
	
	int status = 0;
	
	switch ( ExitReason.BasicExitReason )
	{
	case vmexit_cpuid:
		vmx::vm::HandleCPUID( context, true ); // Hide our hypervisor
		status = 1;
		break;
	default:
		status = 0;
		break;
	}

	if ( !status )
	{
		DbgInfo( "VMMM: VMExit unhandled: %d\nRIP: 0x%x\n", ExitReason.BasicExitReason, context->ExtRegs.rip );
		KD_DEBUG_BREAK();
	}


	return status;
}

