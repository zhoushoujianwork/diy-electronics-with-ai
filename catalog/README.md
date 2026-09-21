# Hardware catalog

This catalog records normalized, sourceable facts about boards and modules. It is an index for
choosing hardware and locating the manufacturer's current documentation; it is not a mirror of
datasheets and it is not proof that hardware has been tested.

## Layout

```text
catalog/
├── schema/
│   ├── vendor.schema.json
│   └── product.schema.json
├── tools/
│   └── validate_catalog.py
└── vendors/
    └── <vendor-id>/
        ├── vendor.yaml
        ├── README.md        # optional vendor documentation and research guide
        └── products/
            └── <product-id>.yaml
```

## 供应商知识页

`vendor.yaml` 只保存机器可读的厂商名称、官网和文档入口。某个供应商具有稳定的中英文文档
结构、产品族命名、官方仓库或特殊检索方法时，把这些可复用的查询知识写入该厂商目录下的
`README.md`。具体型号的参数和来源仍写在 `products/<product-id>.yaml`，不要在供应商知识页
复制一套容易过期的规格表。

## 开发板、模块与配件放在哪里

- 带 MCU、可独立运行的开发板进入 `catalog/vendors/<vendor>/products/`，类别使用
  `development-board`；只有精确型号、修订和引脚经过核对后，才另外建立 `boards/<vendor-model>/`。
- Unit、HAT、Base、传感器、音频或通信扩展同样进入厂商的 `products/`，按实际功能使用
  `sensor-module`、`audio-module`、`positioning-module`、`accessory` 等类别，不进入 `boards/`。
- 某个项目采用配件时，在项目 BOM 或硬件文档中引用目录记录，不复制一份产品资料。
- 个人购买和盘点数据保留在本机电子物料库；产品目录只保存可公开核验的型号、规格和官方来源。

IDs and directory names use lowercase ASCII `kebab-case`. A vendor ID is stable even if its
marketing name changes. A product file may describe one exact model or a family only when every
recorded fact applies to the whole family. Put model- or revision-specific differences in separate
records.

YAML files may use normal YAML when PyYAML is installed, or JSON syntax (which is valid YAML) for a
zero-dependency workflow. The JSON Schemas are the public data contract. The repository validator
performs the important structural, source, cross-reference, and validation-level checks without
requiring `jsonschema`.

## Vendor record

```yaml
schema_version: 1
id: example-vendor
name: Example Vendor
website: https://example.com/
aliases: []
last_reviewed: 2026-09-20
```

## Product record

```yaml
schema_version: 1
id: example-board
vendor_id: example-vendor
name: Example Board
family: Example Family
model: EX-100
revision: "1.2"
category: development-board
lifecycle: active
summary: Short, factual description without marketing claims.
specifications:
  processor:
    manufacturer: Example Silicon
    model: EX32
    cores: 2
    source_refs: [product-page]
  memory:
    flash_bytes: 8388608
    source_refs: [product-page]
software:
  toolchains: [Example SDK]
  frameworks: []
  source_refs: [documentation]
sources:
  - id: product-page
    type: product-page
    title: Example Board product page
    publisher: Example Vendor
    url: https://example.com/products/ex-100
    accessed_on: 2026-09-20
    official: true
  - id: documentation
    type: documentation
    title: Example Board documentation
    publisher: Example Vendor
    url: https://docs.example.com/ex-100/
    accessed_on: 2026-09-20
    official: true
validation:
  level: cataloged
notes: []
```

All byte counts are integer bytes; frequencies are integer hertz; dimensions are millimetres;
voltage and current fields are numeric SI units. Do not convert an ambiguous marketing value into
false precision. Omit a field that the official source does not establish.

## Sources and copyright

- Cite official product pages, documentation, schematics, or manufacturer repositories using a
  stable HTTPS URL and the date accessed.
- `source_refs` contains IDs from the same record's `sources` list. Attach it to each specification
  group so future maintainers can audit changed facts.
- Link to third-party datasheets; do not copy them into this repository unless redistribution
  rights are explicit. A link is not a redistribution licence.
- Do not infer pin maps, connector orientation, electrical limits, or board revisions. Exact,
  verified board pin facts belong under `boards/` and should be linked from a product record with
  `board_profile`.
- Use `notes` to preserve genuine ambiguity. Never fill unknown fields with guesses.

## Validation levels

- `cataloged`: normalized facts were checked against the cited official sources. This says nothing
  about compilation or possession of the device.
- `build-verified`: a named target was built from a recorded commit. Add `checked_on`,
  `firmware_commit`, `build_target`, and repository-relative evidence.
- `hardware-verified`: a real, exact revision was exercised. Also record power arrangement, test
  duration, pass criteria, and repository-relative evidence. Compilation alone never qualifies.

The lifecycle (`active`, `not-recommended-for-new-designs`, `discontinued`, or `unknown`) is
independent of the validation level.

## 选型比较

- [PDM MEMS 与 MAX9814 模拟音频前端](../docs/research/pdm-vs-max9814-audio-frontends.md)：
  官方规格差异、语音对讲选型，以及 Canaan CanMV K230 V3.0 的软件依据与待验证接线边界。

## Validate

From the repository root:

```sh
python3 catalog/tools/validate_catalog.py
python3 -m unittest discover -s catalog/tools/tests -p 'test_*.py'
```

The validator scans `catalog/vendors`. It rejects duplicate IDs, mismatched vendor directories,
unknown source references, non-official/non-HTTPS source records, invalid numeric units, and
unsupported claims at elevated validation levels.
