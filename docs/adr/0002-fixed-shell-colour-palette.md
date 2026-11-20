# 0002. Screen colors are locked to 8 fixed shell-colour presets

## Status

Accepted

## Context

MakerPulseV2 inherited MakerPulse's freeform color system: independent
Accent, Text, and Theme(background) pickers, each with ~12 named presets
plus a custom RGB color-wheel. An earlier plan (see the superseded wording
of CONTEXT.md's "Shell colour" entry) was to let a physical case color
*seed* a complementary default for that system, while leaving it fully
customizable afterward.

That plan is replaced: MakerPulse now ships 8 physical case colors
("MakerPulse Colours.pdf" — Muted White, Pastel Ice, Muted Blue, Army Blue,
Muted Green, Army Red, Pastel Peach, Mandarin Orange), and the product
decision is that the on-device screen should only ever show one of 8
colorways that visibly correspond to those cases — not an unbounded custom
palette a user could pick independently of the hardware in their hand.

## Decision

Replace the three-channel Accent/Text/Theme system (and its custom color
wheel) with a single `shellColour` setting with exactly 8 values. Each
value is a case color's name; picking one derives the device's entire
on-screen palette (background, panel, divider, accent) from that case's
sampled body color, via one shared formula (hue preserved, lightened and
desaturated toward a pale tint for the background, a bolder fixed-lightness
tone for the accent). Ink and dim text are *not* derived per shell — they're
one hue-neutral constant shared by all 8, so every preset gets the same
WCAG AA contrast margin by construction rather than needing 8 separate
manual checks. There is no custom picker and no per-channel override.

The settings page shows 8 circular swatches colored with each case's raw
(undimmed) color, since that's what a user matches against the physical
object in front of them — not the derived, much paler on-screen background.

## Consequences

- One Preferences key (`shellColour`) replaces three (`accent`/`bg`/`text`)
  plus the `custom:#RRGGBB` encoding scheme. Old MakerPulse NVS data isn't
  compatible, but V2 never shipped the old scheme publicly, so no migration
  was written.
- Adding a 9th case color later means extending one lookup table
  (`SHELL_COLOURS` in `src/state.cpp`), not touching the settings page HTML
  structure, which already iterates it.
- A user can no longer theme their device to a color that doesn't match
  any case MakerPulse actually ships — a deliberate loss of flexibility in
  exchange for every unit's screen always looking intentional next to its
  shell, and for being able to guarantee contrast compliance without
  per-combination testing.
