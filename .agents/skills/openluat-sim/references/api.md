# 桃源物联网卡 OpenAPI 契约摘要

来源：[供应商公开 API 文档](http://sim.taoyuan-tech.com/api/wechat/openapidoc)。本轮核对日期：2026-11-05；原文未列 API 版本号或文档更新日期。摘要为接入所需事实，不是原文镜像。

验证边界：本契约摘要不附带客户端或真实账号验证证据。实际查询、响应结构、多页分页、HTTPS 及短信、充值和回调均需在使用环境中按相应授权验证；文档事实不等于账号可用或服务调用成功，也不能据此断言所有环境的 HTTPS 支持情况。

## 公共约定

- 文档基址：`http://api.taoyuan-tech.com/api/open`。仅有 HTTP 记载，HTTPS 尚未核验；真实凭据的传输条件见 Skill。
- 主动接口均用 **POST，JSON body**，`Content-Type: application/json`。
- HTTP Basic：用户名 `<APPKEY>`，密码 `<APPSECRET>`，权限限对应账号的设备；不是网页登录账号密码。
- 公共响应 `code=0` 成功，非零失败，`msg` 提供说明；文档称失败 HTTP 状态非 200。必须分别检查 HTTP 和业务码。
- 普通时间字段采用秒级时间戳；日/月查询参数是日期字符串。流量单位沿用 `M`，十进制/二进制换算未说明；余额和价格单位为**分**。

错误映射：HTTP 500 / code 1 表示操作失败或设备异常；400 / 2 表示 JSON/参数错误；401 / 4 表示认证或权限错误；500 / 99 表示服务器错误。限流、部分成功、重试规则未说明。

## 标识选择

- `S`：`msisdn`、`iccid`、`imsi`、`imei` 四个字符串字段**有且仅有一个**。
- `B`：`msisdns`、`iccids`、`imsis`、`imeis` 四个字符串字段**有且仅有一个**；多卡以英文逗号分隔，不是 JSON 数组。
- 原文把每个标识单独列为非必填，但组合必须满足选择约束；不据此发送无标识请求。批量上限未说明。

## 八个查询接口

以下路径均接在基址后；POST 本身不代表写入。

| 目的 | 路径 | 请求字段 | 返回要点 |
| --- | --- | --- | --- |
| 指定日用量 | `/iotcard/usagelog` | `B`；`query_date` 字符串 `YYYYMMDD`，最近七日 | `data[]` 中 `data_usage` 为 M；无分页说明 |
| 月用量 | `/iotcard/monthly/usagelog` | `B`；`query_month` 字符串 `YYYYMM` | `data[]` 用量为 M；历史月份范围未说明 |
| 计费组 | `/iotcard/billing_group` | 空对象 `{}` | `data[]`：`carrier,bg_code,name,data_plan` |
| 计费组卡列表 | `/iotcard/cardlist` | 必填 `bg_code`（文档标 Integer）；可选 `page=1,psize=100`（Integer） | `data[]` 卡信息；顶层 `psize,num_pages,page,count` |
| 单卡详情 | `/iotcard/card` | `S` | `data` 对象：卡标识、套餐、状态、用量、日期、短信支持等 |
| 批量卡详情 | `/iotcard/cards` | `B` | `data.card_list[]` 和 `data.count` |
| 平台账户余额 | `/iotcard/account/fetch` | 空对象 `{}` | `balance` 为 Integer，单位分；容器形状有歧义 |
| 卡可充值套餐 | `/iotcard/product/fetch` | `S` | `products[]`：`product_id,added_month,product_name,price`；价格为分，容器有歧义 |

仅用于离线请求准备的合成例子（不是实际卡号）：

```json
{"iccids": "<ICCID>", "query_month": "202611"}
```

此例对应月用量；字段是 `query_month`，不是 `month`。卡列表需逐页处理实际 `num_pages`，不要假定第一页代表全部卡；最大分页大小、批量大小、数据刷新延迟及统计时区均未说明。

## 两个写入接口及回调

| 操作 | 路径 | 必填字段 | 影响 |
| --- | --- | --- | --- |
| 下发短信 | `/iotcard/sms_submit` | `msisdns` 字符串（英文逗号分隔）、`content` 字符串 | 发送短信，返回 `sms[]` 的 `msisdn,sms_id`；响应容器有歧义，短信费用及长度限制未说明 |
| 卡充值 | `/iotcard/make_order` | `S`、`product_id` Integer、`extra_no` String、`remark` String | 充值下单；先查询卡可用套餐和平台余额，返回 `code,msg`；没有幂等承诺 |

`make_order` 示例出现额外 `num`，参数表未列含义、类型或必填性，不能按示例推定数量规则或自行增加。`extra_no` 描述提及查询订单，但此文档未提供订单查询接口；超时后不能据此构造未知 endpoint 或自动重试。

短信状态通知由平台向用户配置的回调 URL 发 **POST JSON**，不是一个可主动调用的平台查询路径。字段：`msisdn` String、`status` Integer（0 发送中、1 成功、2 失败）、`sms_id` Integer。回调认证/签名、重试、接收方应答格式未说明；本 Skill 不配置真实回调。

## 必须保留的文档歧义

- 计费组响应 `bg_code` 标为 String，卡列表请求却标 Integer；接入时确认实际类型，不盲目转换或丢失前导零。
- `account/fetch`、`product/fetch`、`sms_submit` 参数表把 `data` 描述为对象，示例却是对象数组；没有真实响应前不固定猜测形状。
- `product_name` 在表中标 Integer，示例为文本；`added_month` 仅称“充值月份”，不补充未给出的业务语义。
- `data_plan`、`test_used_data_usage` 单位未明确；不能把所有数值统一当作 M。
- 月份历史范围、空结果规则、分页/批量上限、限流、流量超额政策、充值生效时间、幂等与网络重试均未给出。
- 部分代码/JSON 示例有格式问题，应按字段表构造合成请求，不照抄示例。文档未列停机、复机或自动续费接口。
