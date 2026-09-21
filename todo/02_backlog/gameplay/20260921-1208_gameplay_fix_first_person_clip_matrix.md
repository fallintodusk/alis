# Fix First-Person Clip Matrix

Status: confirmed; implementation pending

## Owner

`ProjectSkeletalCapabilities` owns local first-person pose correction. The
shared full-matrix evidence route is owned by `ProjectIntegrationTests` and
the character parity runner.

## Current defect

No existing correction mode satisfies the complete 15-phase matrix for the
fixed definition-driven Hero. Fresh same-tree runs produced:

| Mode | Failed phases | Samples | Camera intrusion | Ray hits | Proxy hits | Chain errors | Fold samples |
|---|---:|---:|---:|---:|---:|---:|---:|
| Disabled | 12/15 | 2918 | 20 | 554 | 395 | 1 | 1 |
| TransitionGuard | 12/15 | 2906 | 0 | 0 | 0 | 2 | 131 |
| AngleClamp | 12/15 | 3029 | 0 | 388 | 372 | 1672 | 1684 |
| ChainIK | 14/15 | 3043 | 14 | 75 | 31 | 2862 | 2906 |

TransitionGuard removes the measured camera intersections but still fails the
complete product acceptance. ChainIK reduces some intersections while
introducing severe chain/fold failures. Changing the default from test counts
alone is therefore not justified.

Historical pre-clean fixed-Hero runs also failed. This is inherited character
behavior, not a regression caused by Shipping dependency-closure cleanup.

## Required change

1. Reconcile each failed metric with the edge screenshots and actual
   first-person player-visible defect.
2. Correct the smallest local-body owner that resolves the visible clipping
   without changing Motion Matching, camera placement, or mesh ownership in
   the same step.
3. Prefer the simplest existing correction strategy that passes all 15 phases.
   Do not preserve ChainIK merely because it is the current default.
4. Keep the four named test modes as comparable diagnostics; do not weaken the
   common acceptance matrix to make one mode pass.

## Acceptance

- all 15 phases pass the shared matrix for the selected production mode;
- edge screenshots show acceptable body and arm presentation;
- no camera-volume, ray, proxy, chain, or fold failure remains unexplained;
- the configured runtime default names the accepted mode;
- the other diagnostic modes remain runnable unless their code is proven
  unowned and safely removed.
