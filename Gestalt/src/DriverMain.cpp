#include "common.h"
#include "Hypervisor.h"

Hypervisor hv;

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER( DriverObject );
	//
	if (hv.IsVirtualized())
		hv.Stop();
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER( RegistryPath );

	DriverObject->DriverUnload = DriverUnload;
	
	
	if ( hv.Enable() )
	{
		hv.Start();
	}

	//
	//
	// 

	return STATUS_SUCCESS;
}