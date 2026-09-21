# M5Stack supplier knowledge

本页记录查询 M5Stack 产品资料时可复用的供应商级知识。具体产品规格、来源引用和验证状态
仍以 [`products/`](products/) 下的记录为准；个人购买数量和盘点信息只保存在本机电子物料库。

## 官方入口

| 用途 | 英文 | 简体中文 |
| --- | --- | --- |
| 文档首页 | [English](https://docs.m5stack.com/en/start) | [简体中文](https://docs.m5stack.com/zh_CN/start) |
| 产品与商城 | [m5stack.com](https://m5stack.com/) | 同一官网 |
| 官方硬件仓库 | [M5Stack GitHub](https://github.com/m5stack) | 同一仓库 |
| 文档 URL 索引 | [sitemap.xml](https://docs.m5stack.com/sitemap.xml) | 同一索引包含各语言页面 |

产品文档通常使用以下结构：

```text
https://docs.m5stack.com/en/<family>/<product-slug>
https://docs.m5stack.com/zh_CN/<family>/<product-slug>
```

常见 `family` 包括 `core`、`atom`、`unit`、`hat`、`module` 和 `accessory`。路径大小写、空格
编码和产品 slug 并不总是一致，不能只替换语言段后就假定页面存在；先在官方站点或
`sitemap.xml` 中找到精确 URL，再检查中英文页面是否都返回有效内容。

## 查询流程

1. 先从板卡丝印、包装、官方 SKU 或电子物料库确认产品身份。商品标题只能作为检索线索，
   不能补出没有核实的型号或修订。
2. 在 [`products/`](products/) 中按厂商前缀名称、型号和 SKU 查找现有记录。
3. 没有记录时，从官方文档首页或 `sitemap.xml` 找到精确英文与简体中文产品页。
4. 用官方产品页核对处理器、存储、供电、逻辑电平、接口和兼容主机；原理图、引脚和结构件
   优先引用官方文档链接的 M5Stack GitHub 仓库。
5. 产品 YAML 中英文来源建议分别使用 `official-docs` 和 `official-docs-zh`，并让对应的
   `source_refs` 同时引用二者。只有单一语言页面时保留实际存在的来源，不伪造镜像链接。
6. 购买记录只说明买过或仍在途。实物型号、库存和硬件验证分别通过盘点、`boards/` 档案和
   项目验证记录确认。

## HAT PIR 示例

M5Stack Hat PIR 的精确型号为 `U054`，属于传感器模块而不是开发板：

- [英文文档](https://docs.m5stack.com/en/hat/hat-pir)
- [简体中文文档](https://docs.m5stack.com/zh_CN/hat/hat-pir)
- [仓库产品记录](products/hat-pir-u054.yaml)
- [内部 Senba AS312 传感器记录](../senba-sensing/products/as312.yaml)

它的产品记录同时引用中英文官方页；官方商城与原理图确认内部使用 AS312，原理图中没有状态指示 LED。
面向 StickC 的 GPIO36 接法不能自动外推到 StickS3、StickC Plus SE 或其他主机，必须按精确主机型号重新核对。

## 归档边界

- 可独立运行的 M5Stack Core、Stick、Atom 等开发板进入产品目录；只有经过精确板型和引脚
  核对后才另外进入仓库根目录的 `boards/`。
- Unit、HAT、Base 和 Module 按实际功能归为传感器、音频、定位、通信或通用配件，通常不建
  `boards/` 档案。
- 不把商城订单号、地址、个人证据、Cookie、库存数据库或整份第三方数据手册放进 Git。
