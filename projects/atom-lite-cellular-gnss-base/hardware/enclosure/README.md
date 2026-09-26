# Tiny 打印外壳：先确认配合尺寸

阶段：结构设计输入与平面配合试片。尚未取得含排针总高度、排针出线方向、SIM/天线座的精确位置，
因此完整保护壳的高度、开孔、板托和封盖配合尚未冻结。

## CAD brief

- 对象：先为 ML307R-DL Tiny 设计外壳，再设计 ATOM 与 StickS3 两款适配 PCB；这是用户本轮确定的顺序。
- 已有输入：用户测板框约 20 × 20 mm；商品图 20 × 19 × 6 mm；6P/2.54 mm。商品总高是否含针未知。
- 第一件：平面配合试环，24 × 24 × 3 mm，内窗 20.6 × 20.6 mm；验证 20 mm 板外轮廓与打印间隙。
- 假设：每侧 0.3 mm 间隙，壁厚 1.7 mm，外角 R1；均为首轮试印参数，不是实测公差或最终外壳厚度。
- 坐标：原点在外轮廓中心底面，XY 为板平面，+Z 向上；板件中心暂按试片内窗居中。
- 输出：`tiny-fit-ring.py` 为参数化源；STEP 为主几何；STL 为 FDM 试印副产物。
- 校验：一个闭合正体积实体，外包围盒 24 × 24 × 3 mm，通孔 20.6 mm，中心无底板，内壁间距正确。
- 边界：试环不固定电路板、不覆盖元件，不能当作最终保护壳、通电支架或主机适配结构。

选择先打印试环，是因为排针方向未知不影响平面间隙的验证；没有据此猜测排针孔或 SIM 开口。
完整壳体将根据用户补充的方向/高度，以及实物板托可接触区域再建模。独立 Tiny 壳与最终主机堆叠底座
是否复用同一件，在适配板高度和插针连接方式确定后再判断。

## 已生成的第一件

参数化源：[tiny-fit-ring.py](tiny-fit-ring.py)。同目录已生成 `tiny-fit-ring.step`、`tiny-fit-ring.stl`
及隐藏的 Explorer 文件。派生文件留在本地并由本目录 `.gitignore` 排除，可从源重新生成；此阶段只交付配合试片。

## 试印方法

1. STL 按 mm、100% 比例导入切片器，平放在 XY 面，不自动缩放。
2. 首轮建议 0.4 mm 喷嘴、0.2 mm 层高，选择能填满 1.7 mm 壁厚的墙线策略，无需支撑。
   打印材料仅用于配合试验；最终壳材料需结合温升和实际环境决定。
3. 只在断电状态比对 PCB 外轮廓；避开排针、天线座和器件，不能压着器件强推或用试环整形板子。
4. 记录打印机、材料、切片器、XY 补偿和实测内窗尺寸。如果边缘可自由通过但晃动过大，再调整间隙参数。

## 完整外壳下一步需要的尺寸

- 排针朝下/朝上/向侧面，以及焊好后整件最高点和最低点。
- 6P 孔中心到板边、塑胶座外廓、插接后的线材/排母占位。
- 天线座与出线方向、SIM 卡插拔路径；不要从缩放商品图推算坐标。
- PCB 两面哪些区域可以接触板托；底面器件和焊点净空。

## 重建与实际校验

已使用 Python 3.12.12、build123d 0.9.1、cadquery-ocp 7.8.1.1.post1、ocpsvg 0.4.0、VTK 9.3.1。
兼容约束见 [requirements-cad.txt](requirements-cad.txt)。初次使用较新 OCP/ocpsvg 时与 CAD 工具不兼容，
本轮在独立 uv 环境固定上述版本后生成成功，没有修改全局 Python 或 CAD 技能源码。

从仓库根目录运行，先将 `TINY_CAD_SKILL` 设置为本机 CAD 技能目录；需要代理的环境按本机规则加载：

```sh
TINY_CAD_SKILL=/path/to/cad
uv run --python 3.12 \
  --with-requirements "$TINY_CAD_SKILL/requirements.txt" \
  --with-requirements projects/atom-lite-cellular-gnss-base/hardware/enclosure/requirements-cad.txt \
  python "$TINY_CAD_SKILL/scripts/step" \
  projects/atom-lite-cellular-gnss-base/hardware/enclosure/tiny-fit-ring.py --stl tiny-fit-ring.stl

uv run --python 3.12 \
  --with-requirements "$TINY_CAD_SKILL/requirements.txt" \
  --with-requirements projects/atom-lite-cellular-gnss-base/hardware/enclosure/requirements-cad.txt \
  python "$TINY_CAD_SKILL/scripts/inspect" refs \
  projects/atom-lite-cellular-gnss-base/hardware/enclosure/tiny-fit-ring.step --facts --planes --positioning
```

2026-09-27 几何结果：

- STEP：一个有效闭合实体，包围盒 **24 × 24 × 3 mm**，体积 **452.3448 mm³**。
- `inspect refs --facts --planes --positioning` 通过；原点及上下表面分别为 z=0 / z=3。
- `inspect measure`：内窗 x（f14→f12）和 y（f11→f13）均为 **20.6 mm**；侧壁 x（f12→f10）为 **1.7 mm**。
  这些 face selector 属于本次生成结果，改模后需重新枚举。
- STL：闭合、水密，正体积 **452.3409 mm³**，与 STEP 体积差小于 0.001%；包围盒一致。
- CAD Explorer 已启动并提供模型入口；没有执行实体打印或实物试装，几何正确不证明装配公差已经合适。

后续记录打印件实测与壳体测量，方可冻结封盖、板托及排针开口。
