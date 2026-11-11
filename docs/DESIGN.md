# Interface design

The launcher draws its whole interface itself: one GDI+ frame per window, with
native owner-drawn `BUTTON` and `EDIT` controls placed over it so keyboard
navigation, focus and accessibility behave like any Windows app. This document
covers the visual rules; [ANIMATIONS.md](ANIMATIONS.md) covers motion and
[ARCHITECTURE.md](ARCHITECTURE.md) covers the code.

## Canvas and layout

- Every page is laid out on a **1280 × 800 logical canvas** (`src/components/Layout.h`)
  and scaled uniformly to the window, so the layout never reflows at odd sizes.
  The title bar and navigation rail are anchored to the window edges instead.
- Content lives between x 100–1256 and y 68–776. Home splits it into a left column
  (hero, three status tiles, news and leaderboard) and a 340 px player card.
- While the game is prepared, the installation panel takes the hero's slot only,
  so status, news and the leaderboard stay readable.

## Color

Tokens live in `src/styles/Theme.h`. The palette is a dark indigo base with a
single violet accent; status colors are used only for status.

| Token | Value | Use |
| --- | --- | --- |
| `Background` | `#0D0E14` | Window |
| `Surface` | `#161721` | Panels and cards |
| `SurfaceElevated` | `#1E1F2C` | Hovered cards, inputs |
| `Border` | `#343446` | Outlines, inactive markers |
| `Accent` | `#A582FF` | Primary action, selection, progress |
| `AccentLight` | `#C9B6FF` | Accent text, focus rings |
| `Success` / `Warning` / `Error` | `#37D690` / `#FBBF24` / `#F87171` | Status only |
| `TextPrimary` / `Secondary` / `Muted` | `#F5F7FA` / `#B2B3C3` / `#9697AC` | Text hierarchy |

## Type

- **Sora Bold** (embedded, OFL) for display text: page titles, the player name and
  large percentages.
- **Segoe UI** for everything else, from 11 px captions to 15 px button labels.
- Interface copy is Brazilian Portuguese, with decimal commas (`6,5 GB`).

## Shape

Panels use a 20 px radius, cards 16 px and controls 12 px (`Theme::PanelRadius`,
`CardRadius`, `ControlRadius`). Pills are used for progress tracks and chips.

## Principles

- **Show real state.** Progress shows real bytes, files and stages; when a duration
  is unknown the bar is indeterminate instead of inventing a percentage.
- **Nothing blocks.** Network and disk work runs on worker threads; the window keeps
  painting and responding while the game is prepared.
- **Native behavior first.** Tab order, Enter/Space activation, focus rings and the
  Windows "show animations" preference are respected on every page.
- **Recover in place.** Errors appear where the work happened, with the next step
  and a shortcut to the relevant log.
