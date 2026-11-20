## MakerPulse V2
- UI spec: docs/MAKERPULSE_V2_SPEC.md. Reference screens: docs/screens/ (2x scale). Read both before changing UI code.
- Keep the existing graphics library. Device pixels are 240x320 portrait; all coordinates in the spec are TL datum.
- Provisioning stays runtime (Preferences + Improv WiFi + browser settings page) rather than the spec's own alternate suggestion of per-student config.h constants — see docs/adr/0003-v2-spec-colourway-system.md.
- Log each step (what changed, what failed and why) in PROJECT_LOG.md.
