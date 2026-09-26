# Tiny 完整卡扣外壳 Rev B

本版包含 **底壳 + 可拆卡扣上盖 + 6P 排针出口**，完整合盖外形暂定 **24 × 24 × 11.8 mm**。
壳内四角板托与上盖限位柱固定板件，两侧窗口用于按压释放卡扣。
排针侧的**底壳侧壁设 Ø3 mm 天线圆孔**，完全位于底壳内，上盖外表面和接缝完整。
当前已完成 CAD 几何检查，尚未打印和实物试装；配合尺寸仍可在源文件中调整。

这是 Tiny 独立保护壳的首轮设计。ATOM / StickS3 的主机固定、适配 PCB 与 Type-C 开孔在后续版本加入，
不能把 11.8 mm 视作包含适配板的最终底座高度。详细参数与设计理由见 [设计说明](tiny-case-design.md)。

## 选择与文件

| 排针方向 | 底壳打印件 | 共用上盖 | 合盖审查模型 |
| --- | --- | --- | --- |
| 朝下，穿出底部 | `tiny_case_base.stl`，出口 3.6 × 16.4 mm | `tiny_case_lid.stl` | `tiny_case.step` |
| 侧向，穿出右侧 | `tiny_case_side_base.stl`，出口 16.4 × 4.2 mm | `tiny_case_lid.stl` | `tiny_case_side_exit.step` |

每个版本只需打印一个底壳和一个上盖。**合盖 STEP 不能作为整体一次打印。**
`tiny_case_exploded.step` 是装配爆炸图，包含标有 `REFERENCE_ONLY` 的 PCB、模组和排针占位；
这些占位不是精确器件模型，不参与打印。

STEP 是主 CAD 文件；STL 为打印副产物。派生文件与预览图留在本地，由本目录 `.gitignore` 排除，
提交参数化源和验证脚本，方便重建与修改：

- [tiny_case.py](tiny_case.py)：共用参数、底壳、盖和默认装配；使用 `RigidJoint` 定义盖座配合。
- [tiny_case_base.py](tiny_case_base.py)、[tiny_case_side_base.py](tiny_case_side_base.py)：两种底壳。
- [tiny_case_lid.py](tiny_case_lid.py)：已将上盖外表面朝下，作为打印姿态。
- [tiny_case_side_exit.py](tiny_case_side_exit.py)、[tiny_case_exploded.py](tiny_case_exploded.py)：侧出针装配与爆炸图。
- [check_case.py](check_case.py)：导出文件有效性、装配干涉与开口检查。
- [历史配合试环](fit-ring-notes.md)：保留首轮间隙试片和原始验证记录。

## 已知尺寸与建模假设

| 项目 | 本版采用值 | 依据 |
| --- | --- | --- |
| 外轮廓 | 24 × 24 mm | 用户要求 |
| Tiny 板框 | 20 × 20 mm | 用户测量；商品图另标 20 × 19 × 6 mm，差异保留 |
| 排针 | 单排 6P、2.54 mm | 用户报告节距；首末针中心跨度 12.7 mm |
| 内腔、完整壳高 | 20.6 × 20.6 mm、11.8 mm | 首轮试印设计值 |
| PCB 厚度、板底高度 | 1.0 mm、距外壳底面 2.4 mm | 保留原模型假设，本次不改板件位置 |
| 天线圆孔 | Ø3 mm，侧壁中心 x=12、y=0、z=8 mm | 初始参数；需按插头穿孔包络调整 |
| 排针中心 x | +8.8 mm，y 方向居中 | 未测假设；不是生产封装坐标 |
| 板托和限位 | 四角 1.2 mm 方形接触区，板顶留 0.4 mm | 未核对实物器件避让 |
| 卡扣 | 臂厚 0.8 mm、长 6 mm，插入需挠曲约 0.25 mm | 几何设计值，未测材料强度与寿命 |

24 mm 壳体与 20 mm 板每侧只有 2 mm；常规 M3 通孔直径 3.4 mm，还需螺柱壁厚，
因此本版选卡扣。M3 方案需扩大外形或增加外置耳座。未复制 M5 的专有卡扣尺寸，也不带 M5 标识。

## 打印与装配

1. 按实际排针方向选择底壳，连同共用盖导入切片器；单位 mm、100% 比例。
2. 底壳底面朝下；上盖文件已翻面，外表面朝下、卡扣朝上。首轮建议 PETG、0.4 mm 喷嘴、0.2 mm 层高。
   检查 0.8 mm 弹臂和卡槽外侧薄壁的切片连续性，侧开口按桥接表现决定局部支撑。
3. 先清理毛边并空壳扣合，再断电装板。四角板托及限位柱必须接触空白板边，不能压器件或焊点。
4. 核对天线插头能通过圆孔后，先从壳外穿入尾线插头，再连接板上射频座；让六针通过排针出口并安放板件。
   上盖箭头朝向排针侧，确认线缆未受压后按合。若接头过不去，应调整孔径并重建，不强拉接头。
5. 拆盖时从两侧窗口向内压卡扣并抬盖；若卡住，应修正尺寸，避免撬压 PCB。
6. 记录打印机、材料、切片参数、壳体/板件实测、合盖力和反复开合后的白化/裂纹。

圆孔尺寸与接口需求见 [天线接口记录](../../docs/antenna-interface.md)。本版出线口不提供独立拉力保护；
换 SIM 需要打开上盖。排针塑胶座、排母插接高度和线缆弯曲空间
仍需试装检查。通电前另行核对模块供电和电平，闭壳温升与射频性能还未测试。

## 重建

已使用 Python 3.12.12、build123d 0.9.1、cadquery-ocp 7.8.1.1.post1、ocpsvg 0.4.0、VTK 9.3.1、
trimesh 5.1.0。兼容约束见 [requirements-cad.txt](requirements-cad.txt)。
从仓库根目录运行，`TINY_CAD_SKILL` 指向本机 CAD 技能安装目录；联网代理按本机规则配置：

```sh
TINY_CAD_SKILL=/path/to/cad
TINY_ENCLOSURE=projects/atom-lite-cellular-gnss-base/hardware/enclosure
tiny_cad_python() {
  uv run --python 3.12 \
    --with-requirements "$TINY_CAD_SKILL/requirements.txt" \
    --with-requirements "$TINY_ENCLOSURE/requirements-cad.txt" python "$@"
}
tiny_cad_python "$TINY_CAD_SKILL/scripts/step" \
  "$TINY_ENCLOSURE/tiny_case.py" "$TINY_ENCLOSURE/tiny_case_side_exit.py" \
  "$TINY_ENCLOSURE/tiny_case_exploded.py"
tiny_cad_python "$TINY_CAD_SKILL/scripts/step" "$TINY_ENCLOSURE/tiny_case_base.py" --stl tiny_case_base.stl
tiny_cad_python "$TINY_CAD_SKILL/scripts/step" "$TINY_ENCLOSURE/tiny_case_side_base.py" --stl tiny_case_side_base.stl
tiny_cad_python "$TINY_CAD_SKILL/scripts/step" "$TINY_ENCLOSURE/tiny_case_lid.py" --stl tiny_case_lid.stl
tiny_cad_python "$TINY_ENCLOSURE/check_case.py"
tiny_cad_python "$TINY_CAD_SKILL/scripts/inspect" refs \
  "$TINY_ENCLOSURE/tiny_case.step" --facts --planes --positioning
```

用 CAD Explorer 的 `dev:ensure --workspace-root … --file …` 打开指定 STEP，查看合盖和爆炸装配。
需要导出预览时用 CAD 技能的 `scripts/render view`；渲染依赖 Playwright 的 Chromium headless shell。

## 已执行检查与待验证项

2026-09-27，完整记录见 [项目验证记录](../../docs/validation.md)：

- 两种底壳和盖分别为一个有效实体；STL 水密、正体积，体积与 STEP 差小于 0.1%，打印面位于 z=0。
- 两个合盖模型各有两个有效实体，外形 24 × 24 × 11.8 mm，合盖静态体积干涉为 0。
- 假设 PCB/模组、六根排针均无碰撞；Ø2.8 mm 圆柱通过天线孔，Ø3.2 mm 圆柱接触孔壁。
  孔上下侧壁材料完整、孔顶低于接缝；这些检查只验证模型内的假设占位。
- 盖座 `inspect mate` 结果为零平移、无需旋转；刚性抬盖 0.6 mm 时出现 0.5472 mm³ 卡钩干涉，
  证明几何上有止退关系，不代表卡扣强度或疲劳寿命。
- 已在 CAD Explorer 审查爆炸视图，确认底壳、可拆盖、卡扣和占位板的关系。

Rev B 按用户“只改天线开口”的要求，保留 Rev A 朝下出针位置、板托、盖、卡扣及外形尺寸；
把原左侧矩形天线缺口改为排针侧底壳圆孔，仅对内部裙边作必要避让。默认审查 `tiny_case.step` / 爆炸图，
`tiny_case_side_exit.step` 保留为独立备选，不作为朝下出针版本的替换。
板背面新图显示 SIM 和射频座，但尚未测量其高度；本次只记录净空待核对，不据图改变排针或板件位置。

待验证：实物修订与含针尺寸、板托/限位区域、插接可达性、打印公差、开合力、循环寿命、温升和射频。
本版没有防水、跌落或量产可靠性结论；项目状态仍为 `idea`。不合适时修改源参数并重建，旧试环继续保留。
