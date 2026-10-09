# Settings-entry optimization review — 2026-10-04

Disposition: ship. Scoped self-review, not an independent review.

The owner approved the appearance and requested better frame rate entering
settings. The implementation preserves geometry, native input, motion durations,
easing, particles and button feedback. Only repeated form drawing is cached.
Native interaction invalidates the cache immediately, retaining active editing,
focus, hover, locking and validation. It is released when the transition stops
or the layout changes. No new dependencies or preference changes.

Warm settings rendering falls from 9.45/9.61ms to 2.65/2.38ms, normal/compact.
Median navigation paint intervals fall from 19.05/17.78ms to 16.31/15.02ms.
Paint counts rise from 69/76 to 80/83 over 1.4s. p95 is mixed: normal rises from
24.18 to 31.09ms; compact falls from 35.83 to 24.09ms. Cold form preparation and
home return still contribute peaks; do not claim constant physical 60 FPS.

Release /W4: no warnings. All six CTest suites pass (47.89s), including AutoUpdate.
Tests verify raster reuse, invalidation on editing/focus, pixel agreement with
the current uncached form and release on exit/settling, in addition to existing
native Tab, focus/font/text preservation, hit targets and reduced motion tests.
The first batched visual inspection exposed a pre-existing capture fixture error:
DefWindowProc WM_PRINT/PRF_CHILDREN adds the non-client offset to printed children.
The fixture now paints the parent client and explicitly maps/clips each child's
native print. A second batched inspection confirms aligned, nonduplicated controls
at normal/compact sizes. The initial before/after screenshot equality is superseded
by this fixture correction; production cache equivalence is tested against uncached
form pixels. No aesthetic changes. Detector returns [] (limited native analysis),
and scoped whitespace checks pass. No release or publication.
