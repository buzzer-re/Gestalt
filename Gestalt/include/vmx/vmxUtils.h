#pragma once

#include "common.h"
#include "ia32/x64.h"


#define FILL_SEGMENT_SELECTOR(SELECTOR_NAME, GDT_BASE, SELECTOR_VALUE) \
	__vmx_vmwrite(SELECTOR_NAME##_SELECTOR, SELECTOR_VALUE); \
	__vmx_vmwrite(SELECTOR_NAME##_BASE, VMXUtils::GetSegmentBase(GDT_BASE, SELECTOR_VALUE)); \
	__vmx_vmwrite(SELECTOR_NAME##_LIMIT, __segmentlimit(SELECTOR_VALUE)); \
	__vmx_vmwrite(SELECTOR_NAME##_ACCESS_RIGHTS, VMXUtils::GetSegmentAccessRights(SELECTOR_VALUE));


enum __vmexit_reason_e
{
    vmexit_nmi = 0,
    vmexit_ext_int,
    vmexit_triple_fault,
    vmexit_init_signal,
    vmexit_sipi,
    vmexit_smi,
    vmexit_other_smi,
    vmexit_interrupt_window,
    vmexit_nmi_window,
    vmexit_task_switch,
    vmexit_cpuid,
    vmexit_getsec,
    vmexit_hlt,
    vmexit_invd,
    vmexit_invlpg,
    vmexit_rdpmc,
    vmexit_rdtsc,
    vmexit_rsm,
    vmexit_vmcall,
    vmexit_vmclear,
    vmexit_vmlaunch,
    vmexit_vmptrld,
    vmexit_vmptrst,
    vmexit_vmread,
    vmexit_vmresume,
    vmexit_vmwrite,
    vmexit_vmxoff,
    vmexit_vmxon,
    vmexit_control_register_access,
    vmexit_mov_dr,
    vmexit_io_instruction,
    vmexit_rdmsr,
    vmexit_wrmsr,
    vmexit_vmentry_failure_due_to_guest_state,
    vmexit_vmentry_failure_due_to_msr_loading,
    vmexit_mwait = 36,
    vmexit_monitor_trap_flag,
    vmexit_monitor = 39,
    vmexit_pause,
    vmexit_vmentry_failure_due_to_machine_check_event,
    vmexit_tpr_below_threshold = 43,
    vmexit_apic_access,
    vmexit_virtualized_eoi,
    vmexit_access_to_gdtr_or_idtr,
    vmexit_access_to_ldtr_or_tr,
    vmexit_ept_violation,
    vmexit_ept_misconfiguration,
    vmexit_invept,
    vmexit_rdtscp,
    vmexit_vmx_preemption_timer_expired,
    vmexit_invvpid,
    vmexit_wbinvd,
    vmexit_xsetbv,
    vmexit_apic_write,
    vmexit_rdrand,
    vmexit_invpcid,
    vmexit_vmfunc,
    vmexit_encls,
    vmexit_rdseed,
    vmexit_pml_full,
    vmexit_xsaves,
    vmexit_xrstors,
};

enum VMX_CONTROL_FIELD
{
	VmxPinBasedControls,
	VmxProcessorBasedControls,
	VmxVmExitControls,
	VmxVmEntryControls,
	VmxProcessorBasedControls2,
};


namespace VMXUtils
{
	UINT64 AdjustCR0( UINT64 cr0Value );
	UINT64 AdjustCR4( UINT64 cr4Value );
	UINT64 AdjustControlValue( VMX_CONTROL_FIELD Field, UINT64 Value );

	UINT64 GetSegmentBase( UINT64 GDTBase, UINT16 SelectorValue );

	UINT64 GetSegmentBaseByDescriptor( IN CONST SEGMENT_DESCRIPTOR_32* SegmentDescriptor );
	SEGMENT_DESCRIPTOR_32* GetSegmentDescriptor( UINT64 DescriptorTableBase, UINT16 SegmentSelector );
	UINT64 GetAdjustedControlValue( VMX_CONTROL_FIELD ControlField, UINT64 RequestedValue );
	UINT64 GetSegmentAccessRights( UINT16 SegmentSelector );

}