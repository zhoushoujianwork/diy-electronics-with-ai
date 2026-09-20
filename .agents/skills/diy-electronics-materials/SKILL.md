---
name: diy-electronics-materials
description: "查询淘宝、天猫和立创商城的个人电子物料购买记录，并记录用户确认的盘点数量。 Query personal electronics purchases from Taobao, Tmall, and LCSC, and record user-confirmed stocktake quantities."
---

# 电子物料库

此仓库只收录可复用的技能、脚本和数据契约，不收录个人库存数据库或订单证据。

默认数据库：`~/.local/share/electronic-materials/materials.sqlite3`。如果设置了
`ELECTRONIC_MATERIALS_HOME`，则使用该目录中的 `materials.sqlite3`；否则遵循
`XDG_DATA_HOME`，使用 `$XDG_DATA_HOME/electronic-materials/materials.sqlite3`。

下面的 `SKILL_DIR` 指本文件所在目录。技能可以从 `.agents/skills/diy-electronics-materials`
加载，也可以经兼容软链接加载；不要把仓库路径或用户名硬编码到脚本中。

## 查询

```sh
rtk python3 "$SKILL_DIR/scripts/materials.py" status
rtk python3 "$SKILL_DIR/scripts/materials.py" search 'ESP32'
rtk python3 "$SKILL_DIR/scripts/materials.py" search 'C2040' --source lcsc
rtk python3 "$SKILL_DIR/scripts/materials.py" search '' --category 电阻
```

查询先检查 `status`；零结果只表示已收录记录中未找到，覆盖未完成时不能声称用户没买过。返回型号、规格、购买数量及单位、状态、来源和盘点信息。购买数量不等于现存数量；同一订单的再次同步不能视为再次购买。不同封装、电压、阻值、变体或整包单位不得凭标题相似合并。只有验证型号和参数后才提供替代料建议。

## 获取 / 同步

按用户请求，用可用浏览器工具读取用户已登录的淘宝/天猫“已买到的宝贝”和立创商城“我的订单”。优先已有登录会话或官方订单导出。浏览器工具连接失败时尝试可用的原生浏览器 UI；登录、验证码或连接阻塞不能靠猜测订单填库。不要读取浏览器凭据文件或保存 Cookie、密码、地址、手机号。

第一次全量同步：逐平台检查所有可见年份、时间筛选、分页和订单详情，不仅检索几个关键词或读取最近订单。记录每个电子类订单行的原始标题、实际购买 SKU/规格、单位、数量、订单号、日期、状态和来源。排除无关日用品；不明确的电子配件保留为待分类。只收录实际购得或仍在途的物料。交易关闭、取消、未付款和全额退款的商品行不入库；导入器也会清除同一订单行的旧记录。部分退款无法推算退货颗数，应保留并明确状态。套件保留为套，除非来源明确给出组件数量。

将读取的订单行规范化成 JSON，按 [references/data.md](references/data.md) 导入。证据只保留订单商品相关文本，保存到仓库外的个人数据目录 `evidence/` 子目录；不保存无关个人信息。脚本只负责验证、去重、存储和查询，网页抓取由当前可用浏览器工具完成，不声称支持未经验证的商城 API。

淘宝官方订单导出和本 Skill 的立创页面证据可用 `scripts/normalize_orders.py RAW_DIR OUTPUT.json` 规范化；该脚本读取 `.xlsx` 时需要 `openpyxl`，优先使用 Codex 的 bundled workspace Python。规范化规则只自动收录明显的电子元器件、模块、开发板、电子配件和工具耗材，新增或含糊商品仍需人工抽查。

```sh
rtk python3 "$SKILL_DIR/scripts/materials.py" import /absolute/path/observed-orders.json
rtk python3 "$SKILL_DIR/scripts/materials.py" coverage lcsc --status partial --scope '已读2026年订单第1至3页' --note '下一步第4页'
```

每一批导入后更新 coverage。只有所有可见历史范围和详情都已核对，才设 complete，并记录年份、页数、行数核对依据；平台隐藏/删除的订单不能保证恢复。

增量同步先运行 `status`，读取 `latest_purchase` 中各平台最新的购买时间和订单号。进入平台最近订单，从最新一页向过去查询，直到至少与库中已有订单重叠一页；不要只按最新时间截断，因为同一时间附近可能有迟到、拆单或状态变化。记录新商品行，并复查近期已付款、已发货、退款和关闭订单。按 `source/account/order_id/line_id` 导入会更新或去重；取消、未付款和全额退款行会被导入器清除。完成后更新 coverage 的 scope、note 和时间。发生部分失败保存断点，不清空历史数据。

## 实物盘点

仅根据用户提供的盘点或实际领用信息记录实物数量。按订单行建立批次盘点，不把订单重复导入自动转成库存增加。`stocktake KEY QUANTITY --unit UNIT --note '用户确认…'` 记录指定批次的绝对剩余数，保留历史。跨批次总盘点先明确归属，不随意拆分。查询同时显示盘点时间；未记录的领用无法自动反映。

## 输出和边界

可用 `export /absolute/path/materials.json` 导出全部记录与覆盖信息。数据留在本地，不自动提交 Git 或上传。此 Skill 不包含下单、付款、确认收货、退货或联系商家的授权。收尾报告明确平台覆盖范围、已收录订单行数和待补齐部分。

不要向 Git 提交数据库、导出文件、订单证据、订单号、地址、账号资料或其他个人信息。项目所需的器件型号与公开参数应进入产品目录，而不是复制个人库存。
