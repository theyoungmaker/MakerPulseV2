# 0003. Adopt the V2.1.0 spec's 6-colourway system; keep existing provisioning architecture

## Status

Accepted

## Context

A complete, pixel-exact UI spec (`docs/MAKERPULSE_V2_SPEC.md`, with reference
screens in `docs/screens/`) was handed down as the authoritative "final
version" for MakerPulseV2, to ship as firmware v2.1.0. It redefines the
color system as 6 named colourways (Sage, Ivory, Sky, Peach, Lilac,
Charcoal) with pre-computed RGB565 values and exactly five roles each (bg,
ink, muted, rule, accent) — a different palette and a different, simpler
role model than the 8 MakerPulse-filament "shell colours" shipped one
iteration earlier (`docs/adr/0002-fixed-shell-colour-palette.md`).

The spec also assumes a different *provisioning* architecture than
anything MakerPulse has shipped: per-student `config.h` constants
(WiFi password, API keys, location, owner name) edited by hand before each
compile, no browser settings page at all — a workshop/educational pattern
("the name on the student's Name Cap") rather than the "one firmware
image, flash once, configure over WiFi afterward" model this project has
used since MakerPulse v1.0 (see `docs/adr/0001` in the MakerPulse repo).

## Decision

Follow the spec's UI, layout, typography, color, and interaction rules
exactly. Do **not** adopt its `config.h`-constants provisioning model —
keep runtime Preferences storage, Improv WiFi provisioning, and the
browser settings page as the configuration surface, consistent with
`docs/adr/0001-screen-orientation-runtime-setting.md`'s reasoning (one
firmware image, no ongoing per-unit build cost). The Status page gains the
spec's on-device colourway/brightness/night-mode touch controls *in
addition to* the existing web settings page, not instead of it.

Replace the 8 shell-colour presets with the spec's 6 colourways verbatim
(no re-derivation — the spec states its values already clear WCAG AA
4.5:1). `COL_PANEL`/`COL_PANEL_ALT` are kept as legacy fields mapped to
`bg`/`rule` only until Weather/Bus/Status are rewritten to the spec's
"no boxes" rule in later phases; they're retired once nothing references
them.

## Consequences

- One Preferences key (`shellColour`, kept as the storage key name for
  continuity even though it now holds a Colourway key like `"sage"`)
  continues to drive the whole on-screen palette from one source of truth.
- `BUS_STOPS`, `LAT`/`LON`, `OWNER_NAME`, and API keys from the spec's
  `config.h` field list become ordinary Preferences-backed settings-page
  fields instead of compile-time constants — every unit still runs the
  same firmware image.
- The spec's own acceptance checklists and build-prompt sequence (Prompts
  0–8) are followed for UI/behavior scope, but any instruction in them
  that assumes per-student config.h editing is reinterpreted as "add this
  as a runtime setting" instead.
