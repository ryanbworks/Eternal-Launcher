## verdict
1. Semantic operation mapping — resolved. The regenerated first-install.png shows amber installation guidance and “INSTALAR E JOGAR”; auth-error.png and nickname-error.png show red problem states and recovery; sign-in.png shows an active disabled action. Source assigns NeedsInstallation, SigningIn, AuthenticationError and InvalidNickname explicitly, and isLauncherError now drives both headline and activity classification (src/types/LauncherState.h; src/pages/MainPage.cpp:306, 484, 508, 629).
2. Keyboard task order — resolved in source. layoutControls places the nickname HWND immediately after Original using SetWindowPos, retaining native controls, existing keyboard handling, and visibility behavior (src/pages/MainPage.cpp:940). The corrected sequence was not exercised live after the user stopped Computer Use; still captures cannot prove keyboard navigation. Prior live observations cover the previous version only.
3. Demonstration labeling — resolved. “Notícia de demonstração” is visible in every regenerated preview, including compact.png, and is the literal shipping label (src/components/NewsPanelComponent.cpp:10).

## remaining
clear — No material fix remains open and no visual regression from this fix batch was observed. All six required captures were reread and remain valid; the four new semantic-state renders were also inspected. The optional Sora hero/wordmark change remains legible and introduces no observed clipping. This ship verdict covers the three scored fixes, not a new full review or live acceptance of keyboard, motion, DPI, authentication, installation, or game launch. Release compilation without warnings and CTest 1/1 are parent-reported.

disposition: ship
