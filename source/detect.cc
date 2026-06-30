/**
 * Citron-vs-hardware target detection. 2-of-3 vote across three independent
 * structural signals validated by the Step-1 feasibility spike:
 *
 *   A — hbloader env-block absent (Citron's NRO loader doesn't populate it):
 *       envIsNso() && loader_info_size==0 && !has_heap_override && !has_argv
 *       Robust because faking the env block requires implementing a full
 *       hbloader-style launch path including loader_info text + argv +
 *       heap-override + next-load + random-seed entries.
 *
 *   B — HOS application core-3 reservation not enforced (Citron grants all
 *       four cores to application processes; HOS reserves core 3 for
 *       system): svcGetInfo(CoreMask) & 0x8 set.
 *       Robust because matching HOS's scheduler policy in Citron has no
 *       user-visible benefit and could break homebrew tests.
 *
 *   C — Secure-monitor config queries unimplemented (Citron has no SMC
 *       emulation): splGetConfig(HardwareType) fails (the spike saw
 *       rc=0x21a, blanket-failing across all 15 config items).
 *       Robust because implementing splGetConfig requires emulating BPMP-FW
 *       secure-world code AND inventing plausible per-device values
 *       (hardware_type, dram_id, device_id, kernel_memory_configuration,
 *       ...). Substantial work with no benefit to Citron.
 *
 * Hardware-biased: when a signal can't be measured (e.g. svcGetInfo
 * inexplicably errors), it counts as "hardware" (0 in the score), not
 * "Citron". The score==2 threshold means a single drifted signal still
 * detects, but two of three would-be-Citron signals failing to reproduce
 * silently downgrade to "hardware" (= full JIT). On real Switch hardware
 * that's correct; on Citron that re-introduces the phantom-fault problem
 * the override flag is for ([v8] target=citron).
 */
#include "detect.h"

#include <switch.h>

nx_target_detection nx_detect_citron(void) {
	nx_target_detection r = {};

	// Signal A: hbloader env block absent. All four conditions must hold —
	// hbloader sets every one of them, so a real-hardware false positive
	// would require a launcher that intentionally elides every entry.
	r.sig_hbloader_absent = envIsNso() && envGetLoaderInfoSize() == 0 &&
	                        !envHasHeapOverride() && !envHasArgv();

	// Signal B: core 3 not reserved. svcGetInfo InfoType_CoreMask = 0.
	// CUR_PROCESS_HANDLE = -1 in libnx (special handle for "this process").
	// If svcGetInfo itself fails, count as hardware (0): we never want to
	// vote Citron on a measurement failure.
	{
		u64 core_mask = 0;
		Result rc = svcGetInfo(&core_mask, 0 /* CoreMask */,
		                       CUR_PROCESS_HANDLE, 0);
		r.sig_core3_unreserved = R_SUCCEEDED(rc) && (core_mask & 0x8u) != 0;
	}

	// Signal C: spl secure-monitor unavailable. splInitialize+splExit are
	// the cheapest spl session (just opens/closes an IPC session). We use a
	// fresh init+exit pair instead of touching nx_ctx->spl_initialized so
	// the main.cc lazy spl users (nx_version_get_ams_api_version etc.) are
	// unaffected — they'll splInitialize() again later as if we hadn't run.
	{
		Result rc = splInitialize();
		if (R_SUCCEEDED(rc)) {
			u64 hardware_type = 0;
			Result q = splGetConfig(SplConfigItem_HardwareType, &hardware_type);
			r.sig_spl_unavailable = R_FAILED(q);
			splExit();
		} else {
			// splInitialize() itself failed — service unavailable. Count as
			// Citron (this is the same direction the success+query-fails
			// pattern points). If Citron ever implements splInitialize but
			// still rejects every config query, the inner check above still
			// flags Citron.
			r.sig_spl_unavailable = true;
		}
	}

	r.score = (int)r.sig_hbloader_absent + (int)r.sig_core3_unreserved +
	          (int)r.sig_spl_unavailable;
	r.detected_citron = r.score >= 2;
	return r;
}
