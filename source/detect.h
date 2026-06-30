#pragma once

#include <stdbool.h>

// Citron-vs-hardware target detection. Three independent structural signals
// from the Step-1 feasibility spike, each robust for a different reason
// (launcher model / kernel policy / secure-world emulation). 2-of-3 vote.
//
// Hardware-biased on purpose: forced-jitless-on-hardware is a silent
// recoverable perf cost; full-JIT-on-Citron brings back unattributable
// phantom JIT-execute faults. Do not "fix" this bias.
//
// Self-contained: callable at any time after libnx env setup (envSetup
// already ran by entry to main()). Does its own splInitialize/splExit
// pair, leaving spl in the same uninitialized state it found it in so
// later lazy spl users (see main.cc's nx_version_get_* helpers) are not
// affected.
struct nx_target_detection {
	bool sig_hbloader_absent;  // A — env block matches Citron's NRO loader, not hbloader
	bool sig_core3_unreserved; // B — svc.core_mask has bit 3 set (Citron doesn't reserve core 3)
	bool sig_spl_unavailable;  // C — splGetConfig(HardwareType) fails (Citron has no SMC)
	int score;                 // 0..3 — number of signals voting Citron
	bool detected_citron;      // true iff score >= 2
};

nx_target_detection nx_detect_citron(void);
