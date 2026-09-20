---
name: diy-electronics-catalog
description: "用公开可核验资料维护厂商、产品、配件和中英文官方文档，并沉淀去标识化工程经验；不记录私有项目信息、未验证引脚图或个人库存。 Research public, verifiable catalog facts and de-identified engineering lessons; excludes private project data, unverified pin maps, and personal inventory."
---

# Electronics Lab Catalog

Resolve the checkout with `scripts/repo-root.sh`, read its `AGENTS.md`, `catalog/README.md`, schemas, target
vendor knowledge page, and existing product records before editing.

## Workflow

1. Establish the exact manufacturer, model, SKU and revision. Purchase titles are discovery hints, not public
   product evidence or proof of stock.
2. Prefer official product pages, documentation, schematics, datasheets, certifications and manufacturer
   repositories. Use stable HTTPS URLs and record the access date.
3. Add normalized product facts to `catalog/vendors/<vendor>/products/`. Put stable vendor search routes,
   language mappings and product-family conventions in that vendor's `README.md`.
4. Name records with the manufacturer and exact model where ambiguity is possible. Use the most specific
   category supported by the schema rather than calling every Unit, HAT or Base an accessory.
5. Do not infer voltage, pins, connector orientation or revision details from similar products. Exact verified
   pin facts belong in `boards/`, normally through `$diy-electronics-project`.
6. Run the catalog validator and its tests. State unknowns and validation level precisely.

## Public-information boundary

Catalog content may contain only:

- facts independently verifiable from public official or manufacturer-authorized sources; and
- reusable engineering experience rewritten as de-identified, product-agnostic guidance.

Never record private repository names or paths, private commit or branch identifiers, internal board/profile
names, customer or organization names, device IDs, service URLs, credentials, unpublished architecture,
business workflows, private logs, local evidence paths, or details traceable to a user's private project.
User-supplied files, photos, schematics, logs, and measurements are not public sources merely because they are
available in the workspace. Omit their facts from the catalog unless the same facts are independently confirmed
by an allowed public source.

Generalize useful private experience before writing it: remove project identity, exact deployment topology,
business purpose, unique configuration, timestamps, and evidence identifiers. Label it as engineering
experience rather than a sourced product fact, and do not use it to raise a validation level. If useful guidance
cannot be safely de-identified, do not record it.

Never commit order IDs, addresses, account data, personal inventory exports, cookies, redistributed vendor
documents, or other personal/private data. Use `$diy-electronics-contribute` when the completed catalog update
should be committed or proposed as a pull request.
