## verdict

1. Persistence — resolved. PRODUCT.md now describes functional settings, Save-only persistence, draft reset/discard, operation locks and saved-instance shortcuts. DESIGN.md's scoped Settings/Verification sections describe the implemented native controls and explicitly distinguish synthetic production renders, hidden native form tests and untested live behavior. The shared design primitives remain intact.
2. Error demonstration truth — resolved. Reopened settings-error.png visibly shows initial RAM 8192 MiB and maximum 4096 MiB with validation guidance that initial cannot exceed maximum. The other three required recaptures were also reopened and remain complete, nonblank and aligned at their original dimensions. No regressions from the fix batch are visible.

## remaining

Clear for the two scored material fixes. This ship covers the scored fixes, not a new whole-surface review. The documented evidence remains limited to production preview renders and parent-reported Release /W4 and CTest 3/3, including a hidden test-owned native form. Live desktop appearance, focus/tab navigation, caret/selection, browse dialogs and DPI behavior remain unexercised; Minecraft launch, login and downloads remain unexercised.

disposition: ship
