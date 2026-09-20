---
name: diy-electronics-project
description: "创建或完善可复现的板卡档案、Demo、固件、完整项目和硬件验证证据。 Create or improve reproducible board profiles, demos, firmware, complete electronics projects, and hardware validation evidence."
---

# Electronics Lab Project Engineering

Resolve the checkout with `scripts/repo-root.sh`, then read the root and nearest `AGENTS.md`, target README,
manifest, templates, and existing validation evidence.

Route work deliberately:

- `boards/` for exact revision-specific board facts, pins, power, conflicts and verified profiles.
- `demos/` for one uncertain or focused hardware/protocol claim.
- `projects/` for independently buildable products with a complete user flow.
- `components/` only after two consumers or an explicit API review.
- `hardware/` for repository-owned schematic, PCB, wiring, enclosure and manufacturing sources.

Every demo or project needs its README, `project.yaml`, useful design notes, tests where practical, expected
output and limitations. Confirm part number, voltage, peak current, logic level, connector orientation and pin
conflicts before wiring.

For ESP-IDF/FreeRTOS changes, size task stacks for the largest call path, measure high-water marks under
realistic load, and sustain serial monitoring after flashing. Inspect the first fatal line, reset reason,
panic/Guru Meditation, stack overflow, task-start failure, USB reconnects and heartbeat continuity. Keep
observability independent from blocking storage, radio or modem operations.

Never promote compilation to hardware support. Hardware evidence identifies exact revision, firmware commit,
power arrangement, duration, pass criteria and unresolved checks. Use `$diy-electronics-contribute` to package
a completed milestone into a commit or pull request.
