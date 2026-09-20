# 数据契约

`import` 接受 JSON 数组。每一项是一个订单商品行，字段如下。

必填字符串：`source`（taobao 或 lcsc；天猫归 taobao）、`account`（本地账号别名，不用手机号）、`order_id`、`line_id`、`title`、`status`、`evidence`、`observed_at`（带时区 ISO 时间）。`line_id` 优先平台订单行 ID；没有时为已核验订单内稳定行序号，不可用列表页面行号。`evidence` 为仓库外的本地证据文件路径或无凭据的订单链接。

必填 `quantity`：非负数，原订单购买量；必填 `unit`：件/颗/包/套等真实订单单位。未知数量填 null，unit 可用“未知”，不要填0或1冒充。

`status`：completed、shipped、paid、unpaid、cancelled、refunded、partial_refund、unknown。保留 `status_raw`。部分退款数量未知仍保留原购买量，不算净存量。

可选字符串：`purchased_at`（原始可确定日期）、`seller`、`model`、`lcsc_code`、`spec`（真实选中规格）、`package`、`category`、`product_url`、`notes`。可选数组 `aliases`：检索别名。

稳定主键由 source/account/order_id/line_id 构成。每次导入保存修订历史；相同主键更新当前订单行，不累加数量。不明确行身份先核对。缺失信息保留未知，不能通过商品标题补出未验证的型号或SKU。记录更新必须使用新读取的完整订单行；不要以不完整行覆盖原有已核实字段。

盘点与订单分离。盘点必须带与购买记录一致的单位；套件拆分时先建立清楚的组件来源，不直接把套改成颗。

`--db PATH` 可以指定临时数据库进行验证；日常不传此参数，使用个人数据目录中的默认全局库。SQLite 的 purchase_history 和 stocktakes 保留修订与盘点历史。coverage 为平台最近同步范围，不表示不存在范围外订单。
