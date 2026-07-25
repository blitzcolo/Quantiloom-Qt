# SDK 同步待办

自 0.1.0 版本号提升以来 libQuantiloom 的改动中,本仓库需要跟进的部分。
每条只写**问题 / 现状 / 期望目标**,不预设解决方案。

**本文中的提交号(`5181035` 等)均指 Quantiloom-dev 仓库**,文件路径除非另有说明
也指该仓库;本仓库内的路径写作 `src/...`、`assets/...`。dev 侧原有一份对应的待办
清单,已按其维护者的决定删除,因此本文不再交叉引用其编号 —— 下面每条都是自足的。

事实来源:dev 侧 `5181035..HEAD` 共 38 个提交,其中 4 个动了会被安装进 SDK 的公开
头文件(`core/Types.hpp`、
`postprocess/{GenericSensor,SensorModel,PostprocessConfig}.hpp`);本仓库引用了
17 个 SDK 头文件,最后一次提交是 2026-07-21。核对日期 2026-07-26。

---

## Q1. Qt 自带的光谱资产是修复前的旧副本

**问题** `assets/spectral/` 与 dev 已经对不上:

| 文件 | dev 现在 | Qt 副本 | |
|---|---|---|---|
| `quantiloom_basis_v3_usgs.qlbin` | 113,968 | 325,456 | 过期 |
| `quantiloom_materials_usgs.json` | 3,828,814 | 5,915,757 | 过期 |
| `material_summary_usgs.csv` | 263,221 | 275,595 | 过期 |
| `quantiloom_materials_rii.json` | 2,940,508 | 2,853,905 | 过期 |
| `quantiloom_basis_v3_rii.qlbin` | 325,456 | 325,456 | 一致 |
| `*_ecostress.*` | 有 | 无 | 缺失 |

**现状** Qt 用的 usgs 基仍是 5 波段版本,其中 MWIR 与 LWIR 是边缘钳位外推产生的
**伪数据** —— 这正是 dev 侧已经删掉的东西。也就是说,现在用 Qt 渲 MWIR/LWIR 且选
usgs 基,出来的图基于的是不存在的测量。usgs materials json 还少 8 个因重名被静默
丢弃的材质,两个 json 都没有 `coverage` 字段。

**期望目标** Qt 侧资产与 dev 一致,且 MWIR/LWIR 不再基于伪造数据。

## Q2. SDK 的 ABI 变了,Qt 必须重新构建

**问题** `SensorParams` 新增了 `u32 noiseSeed` 字段,`GenericSensor` 新增了
`m_Seeded` / `m_SeededWith` 两个成员 —— 两个类型的大小都变了,而 Qt 有 17 处
`SensorParams`、6 处 `GenericSensor` 的使用。

**现状** 源码层面兼容(新字段有默认值,构造方式不变),但二进制层面不兼容。旧的 Qt
可执行文件配上新的 `Quantiloom.dll` 属于未定义行为,而且不会有任何报错。
`build_wsl.sh` 每次都会重装 SDK,所以 dev 侧一改,Qt 的既有构建产物立刻就过期了。

**期望目标** 明确 SDK 更新后 Qt 必须重建,且这件事不依赖人记得。

## Q3. Qt 界面显示的波段范围与渲染器实际使用的不一致

**问题** `src/panels/SpectralConfigPanel.cpp`:

| 位置 | 界面显示 | 渲染器实际积分 |
|---|---|---|
| `:41`、`:377` | NIR **780-1400 nm** | **930-1200 nm** |
| `:43`、`:372` | SWIR **1000-2500 nm** | **1400-2400 nm** |

**现状** 同样的错误 dev 侧刚修掉,根因相同:`Types.hpp` 里
`WAVELENGTH_{MIN,MAX}_NIR`(780-1400)是 ISO 波段分类,`GetFusedBandInfo` 返回的
才是渲染器真正积分的范围,两者共用 "NIR" 这个名字。Qt 的下拉框写的是前者。
用户按界面文字理解结果,而结果不是那个范围算出来的。MWIR/LWIR 两处恰好相同,没有问题。

**期望目标** 界面显示的范围来自 `GetFusedBandInfo`,而不是各自硬编码。

## Q4. Qt 保存配置时会丢掉噪声种子

**问题** `ConfigManager.cpp:154` 读配置走的是
`PostprocessConfig::ParseSensorParams`,**会**读取新增的 `sensor.noise_seed`;
但写配置(`:339` 起)是逐字段手写 TOML,里面**没有** `noise_seed`。

**现状** 读写不对称:一个带 `noise_seed` 的场景配置在 Qt 里打开再保存,该键会消失,
下次加载静默回落到默认值。同一份配置在 CLI 和 Qt 之间来回过一遍就不再等价。
`SensorParams` 今后每加一个字段都会重复这个问题。

**期望目标** 读写覆盖同一组字段,新增字段不会被保存过程静默丢弃。

## Q5. Qt 的渲染结果仍然无法复现

**问题** `ExternalRenderContext.cpp:240` 是
`std::mt19937 rng{std::random_device{}()}` —— Qt 走的渲染路径,逐样本种子每次运行
都不同。

**现状** CLI 侧同类问题已修(`renderer.seed`,默认固定,填 0 才随机),验证过同一
场景两次渲染逐字节一致。Qt 走的是 `ExternalRenderContext`,不在那次修改范围内 ——
当时没改的原因是无法在 dev 环境运行 Qt 前端来验证。后果是 Qt 出的图既不能自我复现,
也不能与 CLI 的结果逐位比对。

**期望目标** Qt 与 CLI 采用同一套种子约定,或明确记录交互路径不保证可复现。

## Q6. Qt 的俯仰角限位是为绕开一个已修复的缺陷而设的

**问题** `QuantiloomVulkanRenderer.cpp:423` 把 `m_orbitPitch` 限制在
`±(π/2 - 0.1)`,即无法真正垂直俯视或仰视。

**现状** 这个限位的原因是 `Camera::UpdateVectors` 过去在 forward 与 up 平行时
`normalize(cross(...))` 会产生 NaN,污染每条主光线。该缺陷已在 dev 侧修复:两种退化
输入(position == lookAt、forward 平行 up)现在都有明确定义的结果,并有测试覆盖。
限位现在是一个不必要的功能限制,而不是防护。

**期望目标** 确认限位可以放开,或记录它还有别的理由(例如手感)。

## Q7. 光谱资产没有同步机制

**问题** SDK 只安装 `assets/atmos_models/`,不含 `assets/spectral/`。Qt 的光谱资产
是一份手工副本。

**现状** Q1 的过期正是这么来的:dev 侧重新烘焙后,没有任何东西提示 Qt 那份该更新,
也没有任何检查会发现两边不一致。dev 侧曾出现过同源问题:烘焙产物与实际加载路径
之间隔着一个无人记录的手工步骤,那次是靠逐个核对文件名才发现的。

**期望目标** 两侧资产要么同源,要么不一致时能被发现。

---

## 不需要 Qt 改动的部分

以下 dev 侧改动 Qt 会自动获益或不受影响,列出以免重复排查:

- **着色器波段常量统一**(`SPECTRAL_*_LAMBDA_*`):数值未变,且 SDK 安装的是编译好的
  `.spv`,Qt 重建后自动获得。
- **`Camera` 退化基修复**:纯行为修复,Qt 重建后自动获得(另见 Q6)。
- **materials json 新增 `coverage` 字段**:纯增量,SDK 的 JSON 解析器对未知键
  显式 `SkipValue`,旧代码读新文件不受影响。
- **`GetFusedBandInfo` 的取值**:未变,仅 `AtmosphereBaker` 改为复用它。
- **`renderer.seed` 配置键**:只在 CLI 的 `main.cpp`,不在 SDK 内(Qt 侧对应 Q5)。
