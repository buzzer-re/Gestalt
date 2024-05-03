#include "vmx/vmxUtils.h"


//
// Fix the CR0 fields based on the IA32_VMX_CR0_FIXEDX MSR, which holds what can/can't be enabled/disabled
//
UINT64 VMXUtils::AdjustCR0( UINT64 cr0Value )
{
	UINT64 Fixed0CR0 = __readmsr( IA32_VMX_CR0_FIXED0 );
	UINT64 Fixed1CR0 = __readmsr( IA32_VMX_CR0_FIXED1 );

	cr0Value &= Fixed1CR0;
	cr0Value |= Fixed0CR0;

	return cr0Value;
}


//
// Fix the CR1 fields based on the IA32_VMX_CR4_FIXEDX MSR, which holds what can/can't be enabled/disabled
//
UINT64 VMXUtils::AdjustCR4( UINT64 cr4Value )
{   
	UINT64 Fixed0CR4 = __readmsr( IA32_VMX_CR4_FIXED0 );
	UINT64 Fixed1CR4 = __readmsr( IA32_VMX_CR4_FIXED1 );


	// ( cr4Value & Fixed1CR4 ) | Fixed0CR4;
	cr4Value &= Fixed1CR4;
	cr4Value |= Fixed0CR4;

	return cr4Value;
}


//
// Adjust control value based on the supported features written in the VMX MSRs
//
UINT64 VMXUtils::AdjustControlValue( VMX_CONTROL_FIELD Field, UINT64 Value )
{
	// MSR that contains basic information of VMX env, including the VmxControls support
	IA32_VMX_BASIC_REGISTER VMXBasicMSR;
	UINT32 MSRAddress;
	IA32_VMX_TRUE_CTLS_REGISTER Capabilities;
	UINT64 EffectiveValue = Value;

	VMXBasicMSR.AsUInt = __readmsr( IA32_VMX_BASIC );


	switch ( Field )
	{
	case VmxPinBasedControls:
		MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS;
		break;
	case VmxProcessorBasedControls:
		MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS;
		break;
	case VmxVmExitControls:
		MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS;
		break;
	case VmxVmEntryControls:
		MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;
		break;
	case VmxProcessorBasedControls2:
		MSRAddress = IA32_VMX_PROCBASED_CTLS2;
		break;
	default:
		KD_DEBUG_BREAK();
		goto exit;
	}

	Capabilities.AsUInt = __readmsr( MSRAddress );


	EffectiveValue |= Capabilities.Allowed0Settings;
	EffectiveValue &= Capabilities.Allowed1Settings;
	exit:
	return EffectiveValue;
}


//
// Get the segment base address 
//
UINT64 VMXUtils::GetSegmentBase( UINT64 DescriptorTableBase, UINT16 SegmentSelector )
{
    UINT64 segmentBase;
    SEGMENT_SELECTOR segmentSelector;

    segmentSelector.AsUInt = SegmentSelector;

    if ( ( segmentSelector.Table == 0 ) &&
        ( segmentSelector.Index == 0 ) )
    {
        //
        // The null segment selectors technically does not point to a valid
        // segment descriptor, hence no valid base address either. We return
        // 0 for convenience, however.
        //
        // "The first entry of the GDT is not used by the processor. A segment
        //  selector that points to this entry of the GDT (that is, a segment
        //  selector with an index of 0 and the TI flag set to 0) is used as a
        //  "null segment selector."".
        // See: 3.4.2 Segment Selectors
        //
        segmentBase = 0;
        goto Exit;
    }

    //
    // For practical reasons, we do not support LDT. This will not be an issue
    // as we are running as a SYSTEM which will not use LDT.
    //
    // "Specifies the descriptor table to use: clearing this flag selects the GDT;
    //  setting this flag selects the current LDT."
    // See: 3.4.2 Segment Selectors
    //
    segmentBase = VMXUtils::GetSegmentBaseByDescriptor( GetSegmentDescriptor( DescriptorTableBase,
        SegmentSelector ) );

Exit:
    return segmentBase;
}


UINT64 VMXUtils::GetSegmentBaseByDescriptor( IN CONST SEGMENT_DESCRIPTOR_32* SegmentDescriptor )
{
    UINT64 segmentBase;
    UINT64 baseHigh, baseMiddle, baseLow;

    baseHigh = ( ( UINT64 ) SegmentDescriptor->BaseAddressHigh ) << ( 6 * 4 );
    baseMiddle = ( ( UINT64 ) SegmentDescriptor->BaseAddressMiddle ) << ( 4 * 4 );
    baseLow = SegmentDescriptor->BaseAddressLow;
    segmentBase = ( baseHigh | baseMiddle | baseLow ) & 0xFFFFFFFF;

    //
    // Few system descriptors are expanded to 16 bytes on x64. For practical
    // reasons, we only detect TSS descriptors (that is the System field is
    // cleared, and the Type field has either one of specific values).
    //
    // See: 3.5.2 Segment Descriptor Tables in IA-32e Mode
    //

    if ( ( SegmentDescriptor->System == 0 ) &&
        ( ( SegmentDescriptor->Type == SEGMENT_DESCRIPTOR_TYPE_TSS_AVAILABLE ) ||
            ( SegmentDescriptor->Type == SEGMENT_DESCRIPTOR_TYPE_TSS_BUSY ) ) )
    {
        CONST SEGMENT_DESCRIPTOR_64* descriptor64;

        descriptor64 = ( CONST SEGMENT_DESCRIPTOR_64* )SegmentDescriptor;
        segmentBase |= ( ( UINT64 ) descriptor64->BaseAddressUpper << 32 );
    }
    return segmentBase;
}


UINT64 VMXUtils::GetAdjustedControlValue( VMX_CONTROL_FIELD ControlField, UINT64 RequestedValue )
{
    // MSR that contains basic information of VMX env, including the VmxControls support
    IA32_VMX_BASIC_REGISTER VMXBasicMSR;
    UINT32 MSRAddress;
    IA32_VMX_TRUE_CTLS_REGISTER Capabilities;
    UINT64 EffectiveValue = RequestedValue;

    VMXBasicMSR.AsUInt = __readmsr( IA32_VMX_BASIC );


    switch ( ControlField )
    {
    case VmxPinBasedControls:
        MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS;
        break;
    case VmxProcessorBasedControls:
        MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS;
        break;
    case VmxVmExitControls:
        MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS;
        break;
    case VmxVmEntryControls:
        MSRAddress = VMXBasicMSR.VmxControls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;
        break;
    case VmxProcessorBasedControls2:
        MSRAddress = IA32_VMX_PROCBASED_CTLS2;
        break;
    default:
        KD_DEBUG_BREAK();
        goto exit;
    }

    Capabilities.AsUInt = __readmsr( MSRAddress );


    EffectiveValue |= Capabilities.Allowed0Settings;
    EffectiveValue &= Capabilities.Allowed1Settings;
exit:
    return EffectiveValue;
}


UINT64 VMXUtils::GetSegmentAccessRights( UINT16 SegmentSelector )
{
    SEGMENT_SELECTOR segmentSelector;
    UINT32 nativeAccessRight;
    VMX_SEGMENT_ACCESS_RIGHTS accessRight;

    segmentSelector.AsUInt = SegmentSelector;

    //
    // "In general, a segment register is unusable if it has been loaded with a
    //  null selector."
    // See: 25.4.1 Guest Register State
    //
    if ( ( segmentSelector.Table == 0 ) &&
        ( segmentSelector.Index == 0 ) )
    {
        accessRight.AsUInt = 0;
        accessRight.Unusable = TRUE;
        goto Exit;
    }

    //
    // Convert the native access right to the format for VMX. Those two formats
    // are almost identical except that first 8 bits of the native format does
    // not exist in the VMX format, and that few fields are undefined in the
    // native format but reserved to be zero in the VMX format.
    //
    nativeAccessRight = __segment_access_rights( SegmentSelector );
    accessRight.AsUInt = ( nativeAccessRight >> 8 );
    accessRight.Reserved1 = 0;
    accessRight.Reserved2 = 0;
    accessRight.Unusable = FALSE;

Exit:
    return accessRight.AsUInt;
}


SEGMENT_DESCRIPTOR_32* VMXUtils::GetSegmentDescriptor( UINT64 DescriptorTableBase, UINT16 SegmentSelector )
{
    SEGMENT_SELECTOR segmentSelector;
    SEGMENT_DESCRIPTOR_32* segmentDescriptors;

    //
    // "Selects one of 8192 descriptors in the GDT or LDT. The processor multiplies
    //  the index value by 8 (the number of bytes in a segment descriptor) and
    //  adds the result to the base address of the GDT or LDT (from the GDTR or
    //  LDTR register, respectively)."
    // See: 3.4.2 Segment Selectors
    //
    segmentSelector.AsUInt = SegmentSelector;
    segmentDescriptors = ( SEGMENT_DESCRIPTOR_32* ) DescriptorTableBase;
    return &segmentDescriptors[segmentSelector.Index];
}
