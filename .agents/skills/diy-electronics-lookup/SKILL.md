---
name: diy-electronics-lookup
description: "查询 AI DIY Electronics Lab 中可复用的电子工程知识，用于选型、接线、对比和排障；默认只读。 Query reusable electronics knowledge for component selection, wiring, comparison, and troubleshooting; read-only by default."
---

# Electronics Lab Lookup

Use AI DIY Electronics Lab as an evidence-backed knowledge source from any project. Resolve its checkout with
`scripts/repo-root.sh`, then read the repository `AGENTS.md` before using its content.

Search the narrowest relevant location:

| Need | Location |
| --- | --- |
| Officially sourced vendor and product facts | `catalog/vendors/<vendor>/` |
| Vendor documentation routes and bilingual lookup | `catalog/vendors/<vendor>/README.md` |
| Exact revision-specific pins, power and conflicts | `boards/<vendor-model>/` |
| One reproducible hardware claim | `demos/<slug>/` |
| Complete systems and project-local evidence | `projects/<slug>/` |
| Reviewed reusable code | `components/` |
| Schematics, PCB, wiring and mechanics | `hardware/` |

Search exact manufacturer, model, SKU, chip ID or LCSC code with `rg` before trying family names. Interpret
`cataloged`, `build-verified`, and `hardware-verified` literally; compilation is not hardware evidence. Read
linked limitations and the exact revision before reusing pin maps or electrical guidance.

This skill does not authorize editing or publishing the knowledge repository. When verified reusable findings
should be added, also use `$diy-electronics-catalog` or `$diy-electronics-project`; use
`$diy-electronics-contribute` for commits and pull requests.
