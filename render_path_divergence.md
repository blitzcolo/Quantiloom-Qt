# Studio（Qt）与 CLI（dev）渲染路径差异审计

日期：2026-07-30（审计）／2026-07-31（修复完成）
范围：`/mnt/d/Quantiloom-Qt`（Studio，经 `ExternalRenderContext` 驱动 SDK）对比
`/mnt/d/Quantiloom-dev`（CLI，经 `OfflineRenderer` 驱动同一核心库）。
目的：找出两条路径中**可能导致渲染结果错误**的分歧。

行号约定：dev 侧路径省略 `/mnt/d/Quantiloom-dev/` 前缀，Qt 侧省略
`/mnt/d/Quantiloom-Qt/` 前缀。dev 侧引用的实现与预编译 SDK 为同一份代码。

> ## 状态：本文所列分歧已修复
>
> 修法是 §5 提出的那一条：配置解释下沉为一份共享代码
> （`src/libQuantiloom/renderer/ConfigResolve.{hpp,cpp}`），CLI 直接调用，Studio 经
> 新导出的 `ExternalRenderContext::ApplyConfig` 调用。Studio 侧的
> `applySpectralConfig()` 与 `[[materials]]` 读取器随之删除。
>
> 因此**下文的行号与代码引用描述的是修复前的状态**，保留为记录：它说明了每一处
> 分歧的成因，以及为什么共享读取是必要的而不只是整洁。
>
> 逐项处置：
>
> | 分歧 | 处置 |
> |---|---|
> | §2.1 阴影射线默认值相反 | 共享读取缺省 true；`CreateDefaultLightingParams()` 一并翻转 |
> | §2.2 显示变换无 sRGB 编码 | Studio 请求 sRGB 交换链；核心对非 sRGB 目标告警；更正 Reinhard 假注释 |
> | §2.3 `solar_lut_normalise` 未实现 | 在共享读取中实现，两侧同一份 |
> | §2.4 IR 波长/温度缺省 | 波段中心规则与 `default_temperature_k` 回填均在共享读取中 |
> | §2.5 光谱模式解析器重复 | 删除自写解析器，改用 SDK `ParseSpectralMode` |
> | §2.6 NMF 基与复折射率 CLI 专属 | 移入共享读取，Studio 自动获得（`SpectralBasisLoader` 仍不导出） |
> | §2.7 两处状态污染 bug | 光照面板改从渲染器灌入；裸模型加载清除已存配置 |
> | §2.8 缺键策略 | `ConfigApplyOptions::MissingKeyPolicy`，CLI 严格、Studio 宽松并回报 |
> | §2.9 大气三处 | CWD 搜索候选补齐；显式几何键两侧同尊重；`[atmospheric]` 统一为忽略+告警 |
> | §3.1 spp / §3.6 FOV 缺省 | 对齐核心的 1 与 60 |
> | §3.4 分辨率 | 仍按视口渲染，但经 `ConfigApplyReport` 回报配置值而非静默丢弃 |
> | §3.2 传感器单位、§3.5 hyperspectral | 未改，属既有设计差异；后者现在有 Info 提示 |
>
> **验证**：CLI 的 8 个基线场景在整个重构过程中逐字节不变，furnace 六个腔体保持
> 0.0000%；核心测试套件 950 项（新增 34 项钉住上述规则）；同一 LWIR 配置下两侧的
> 配置读取日志 18 行中 16 行完全一致，其余两行是模型包路径写法与"大气几何冻结与否"
> 的设计差异（烘焙实际使用同一组公式与输入）。

---

## 0. 结论先行

两条路径**共享**：

- 场景几何加载：`rendercore::LoadSceneFromConfig`（`src/libQuantiloom/renderer/RenderCore.cpp:23-48`），
  读 `scene.usd` / `scene.gltf`，两侧同一调用。
- 全部着色器：光谱→XYZ→sRGB 转换、白平衡、逐样本累积均在
  `closesthit.rchit` / `raygen.rgen` 内，一份代码两侧共用。
- 渲染目标：`rendercore::CreateRenderTarget`，`R32G32B32A32_SFLOAT`
  （`RenderCore.cpp:902-920`），两侧同一调用。
- CIE CMF 表、BRDF LUT、环境立方体贴图、材质转换（`RenderCore.hpp:75-427`）。

所以物理内核不是分歧来源。**根源是：SDK 没有导出"把 TOML 应用到渲染上下文"
的函数。** CLI 的约 50 个配置键全部在 `OfflineRenderer.cpp` 内部解释；
`ExternalRenderContext::LoadScene(config)` 只有五行、只读场景路径
（`src/libQuantiloom/renderer/ExternalRenderContext.cpp:683-691`）。Studio 因此在
`src/config/ConfigManager.cpp:62-260` + `src/MainWindow.cpp:2145-2274` 重新实现了
一遍配置解释，两份实现已明显漂移。`RenderCore.hpp:20-23` 自己把"导出统一的
apply-config"记为未完成的合并后半程。

---

## 1. 架构对照

| | `OfflineRenderer`（CLI） | `ExternalRenderContext`（Studio） |
|---|---|---|
| 入口 | `src/app/main.cpp`（纯宿主，438 行） | `src/vulkan/QuantiloomVulkanRenderer.cpp` |
| 设备 | 自建 `VulkanContext`，无 surface/swapchain（`OfflineRenderer.cpp:1198`） | 采用 Qt 的 instance/device/queue（`QuantiloomVulkanRenderer.cpp:96-104`），经 `VulkanContextAdapter` 包装 |
| 生命周期 | 一实例一渲（`OfflineRenderer.hpp:24-26`） | 长生命周期、可变 |
| 累积 | 批式：一次 `Render()` 跑满 `spp`，每次提交 2 样本防 TDR（`OfflineRenderer.cpp:1405-1470`） | 渐进：每 `RenderFrame` 一个样本，`accumulatedSamples` 只增不停（`ExternalRenderContext.cpp:1104`）；SPP 仅作进度条分母与抖动开关 |
| 输出 | `ReadbackImage` → EXR（原始线性）+ PNG 预览（百分位拉伸 + sRGB） | `vkCmdBlitImage` 到交换链图像（clamp 到 1.0） |
| 相机 | `Camera::FromConfig` 一次成型、不可变（`OfflineRenderer.cpp:1118`） | 实时 `SetCameraLookAt` / `SetCameraFOV`，每次重置累积 |
| 配置解释 | 全部键在 `OfflineRenderer` 内部 | Studio 自行解析后经 setter 推送；**从不调用** `LoadSceneFromConfig` / `LoadScene(const Config&)` |
| 管线缓存 | CWD 下 `pipeline_cache.bin`（`OfflineRenderer.hpp:130`） | `%LOCALAPPDATA%/Quantiloom/cache`（`ExternalRenderContext.cpp:66-122`） |

`RenderCore.hpp:14-24` 记录了历史上双编排器造成的三个真实 bug（环境贴图上下颠倒、
GUI 热响应与波长无关、GUI 缺传感器波段缩放）——本文列的即是尚存的同类问题。

---

## 2. 高危分歧（会直接导致错误画面，按严重程度排序）

### 2.1 阴影射线默认值相反

- CLI：`renderer.enable_shadow_rays` 缺省 **true**（`OfflineRenderer.cpp:1232`）。
- Studio：同键缺省 **false**（`src/config/ConfigManager.cpp:146`）；
  `CreateDefaultLightingParams()` 也是 `enableShadowRays = 0u`
  （`include/quantiloom/renderer/LightingParams.hpp:184`）。
- 现有全部配置文件都没写这个键 → **同一场景 CLI 有影子、Studio 全部无影。**

### 2.2 显示变换：clamp + 无 sRGB 编码 vs 百分位拉伸 + sRGB

- `ExternalRenderContext.hpp:226` 声称 "simple Reinhard tone mapping"，**实际不存在**：
  `RenderFrame` 只做一次 `vkCmdBlitImage`（`ExternalRenderContext.cpp:1048-1074`），
  即 clamp 到 1.0。树里唯一的 Reinhard 在调试可视化分支（`closesthit.rchit:1863`）。
- Studio 从不调用 `setPreferredColorFormats`（全仓 grep 无结果），Qt 6 默认给
  `VK_FORMAT_B8G8R8A8_UNORM` 交换链；SDK 存了 `targetColorFormat` 却**从不使用**
  （仅 `ExternalRenderContext.cpp:152` 声明、`:597` 赋值）→ 线性辐亮度未经
  sRGB 编码直接上屏，画面系统性偏暗。
- CLI 的 PNG：1–99 百分位拉伸到 [0,1]（`src/app/main.cpp:338-369`）再 IEC 61966-2-1
  sRGB 编码（`src/libQuantiloom/io/ImageIO.cpp:164-208`）；EXR 是原始线性物理量。
- 对 ASTM G-173 这类积分值达数百的光源（`main.cpp:330-334`），视口是一片白，
  CLI 预览正常。

### 2.3 `lighting.solar_lut_normalise` 未实现

- CLI：`"unit_luminance"` 把日/天两条光谱除以太阳亮度 Y
  （`OfflineRenderer.cpp:666, 692-704`）。
- Studio：`MainWindow::applySpectralConfig`（`src/MainWindow.cpp:2171-2216`）读了
  `solar_lut` / `solar_lut_columns` / `solar_lut_diffuse_is_global`，**不读**本键。
- 使用该键的配置在 Studio 里整体亮 Y 倍。

### 2.4 红外模式两处致命缺失

1. **波长默认值**：`spectral.wavelength_nm` 缺省时 CLI 取波段中心
   （`OfflineRenderer.cpp:171-181`，经 `GetFusedBandInfo`），Studio 一律 550 nm
   （`ConfigManager.cpp:81`）且总是推送（`MainWindow.cpp:2000`）→ 无显式波长的
   LWIR 场景在 Studio 按 550 nm 渲染。
2. **默认表面温度**：CLI 用 `scene.default_temperature_k`（缺省 300 K）给无温度材质
   回填（`OfflineRenderer.cpp:220-238`）；Studio 从不调用——尽管
   `ApplyDefaultIRTemperature` 是 inline 头文件函数
   （`include/quantiloom/scene/Material.hpp:440`），本可直接用。
   → glTF/USD 材质在 Studio 的 IR/Single 模式下 Planck 发射**静默为零**。

### 2.5 光谱模式解析器重复实现且行为不同

- Studio 自写 `parseSpectralMode`（`ConfigManager.cpp:233-263`），先转小写；
  SDK 现成的 `ParseSpectralMode`（`include/quantiloom/core/Types.hpp:353-375`）未用。
- 后果：CLI 接受的大写别名（`"VIS"`、`"MWIR"`、`"LWIR"`、`"SWIR"`、`"NIR"`、
  `"RGB"`）落空；**`"multispectral"` 完全没处理**——全部静默变成 RGB 模式。

### 2.6 两类光谱材质数据是 CLI 专属

- **NMF 光谱基**：`[spectral] basis_file / materials_json / band` 驱动
  `SpectralBasisLoader` 为携带 `quantiloom_*` 引用的材质重建实测曲线
  （`OfflineRenderer.cpp:395-478`）。该类在 `src/libQuantiloom/io/` 内部，
  **未进 SDK**（`windows_amd64/include/quantiloom/io/` 无此头）。
  → `cornell_box_vis.toml` 等场景在 Studio 里退化为 RGB 上采样。
- **复折射率**：`[refractive_index]` 段按材质名加载 RefractiveIndex.INFO YAML
  （`OfflineRenderer.cpp:521-554`）。Qt 仓 grep 无任何读取；
  `AddComplexRefractiveIndex` API 存在但从未被配置驱动。
  → 金属在 Studio 丢失物理 Fresnel。

### 2.7 Studio 侧两个状态污染 bug

1. **光照面板加载后被重置**：`updatePanelsFromScene()` 在 `sceneLoaded`
   （`MainWindow.cpp:1069`）时把面板设回 `CreateDefaultLightingParams()`
   （`:1973`）——晚于 `applyConfig` 灌入 TOML 值。渲染器里仍是 TOML 值，
   但用户**一碰任何光照滑块**，`onLightingChanged`（`:1873-1889`）就把面板默认值
   （sun `(1,1,1)`、sky `(0.1,0.15,0.2)`、shadow rays 关）整体覆盖上去。
2. **陈旧 config 重放**：`applySpectralConfig` 读 `getRawConfig()`
   （`MainWindow.cpp:2146`），后者仅在打开 `.toml` 时更新、从不清空
   （`ConfigManager.cpp:46`）。先开 TOML 再开裸 `.gltf`/`.usd`，上一个场景的
   `solar_lut` 与 `[spectral_curves]` 被套到新场景（触发点 `MainWindow.cpp:1070`）。

### 2.8 缺键策略：CLI 报错，Studio 静默默认

| 键 | CLI | Studio |
|---|---|---|
| `renderer.resolution` | 必填，硬错误（`OfflineRenderer.cpp:140-150`） | 只进标签（§3.4） |
| `lighting.sun_direction/sun_radiance/sky_radiance` | 必填（`:1124-1147`） | 静默落默认（`ConfigManager.cpp:64, 116-131`） |
| `material.albedo` | 必填（`:1185`） | 不读 |
| `camera.position/look_at` | 必填（`Camera.cpp:91-104`） | 数组不足 3 时静默保留旧值（`ConfigManager.cpp:93,100`） |
| `camera.fov_y` 缺省 | **60°**（`Camera.cpp:119`） | **45°**（`ConfigManager.cpp:113`） |
| 光谱模式 + 非零日/天光 + 无 `solar_lut` | 拒绝启动（`OfflineRenderer.cpp:639-645`） | 仅 warn（`MainWindow.cpp:2159-2169`），渲出全黑——着色器已无 RGB 回退（`closesthit.rchit:980-990, 1179-1189`），`ExternalRenderContext.hpp:614-619` 有明文警告 |

### 2.9 大气三处分歧

1. **模型包搜索路径**：CLI 依次试 `$QUANTILOOM_ATMOS_MODELS` → **CWD 相对
   `assets/atmos_models`** → exe 相对（`main.cpp:57-77`）；Studio 缺 CWD 候选
   （`src/vulkan/QuantiloomVulkanRenderer.cpp:805-820`）。找不到时 CLI 硬错误
   （`OfflineRenderer.cpp:807-811`），Studio 静默把 `enabled` 降为 false
   （`QuantiloomVulkanRenderer.cpp:822-852`）。
2. **显式几何键被忽略**：`atmosphere.sun_zenith_deg / sun_azimuth_deg / h1_km /
   lut_a_samples / lut_az_samples` CLI 尊重并固定（`OfflineRenderer.cpp:835-875`，
   置 `sunFromLighting = h1FromCamera = false`）；Studio 从不设置这些
   （`ConfigManager.cpp:170-196`），`AtmosphereNNConfig` 默认的实时推导生效
   （`AtmosphereNNConfig.hpp:40,47`；`ExternalRenderContext.cpp:837-850`）。
   Studio 转动相机会改 h1 并重烘焙；CLI 冻结在配置相机。
3. **旧版 `[atmospheric] preset`**：Studio 兼容映射（`ConfigManager.cpp:151-165`），
   CLI 弃用忽略（`OfflineRenderer.cpp:782-787`）——同一文件两边相反。

另注（`145f00d` 类 bug 的机理）：上下文销毁重建后 `Impl` 内**全部**配置态回到
构造默认——大气配置/烘焙键、太阳 LUT（归零 → 光谱模式无光）、光谱曲线、CRI、
环境贴图（回落天蓝）、光照参数（shadow rays 关）、模式/波长/spp/seed
（`ExternalRenderContext.cpp:200-234, 1917-1988`）。宿主漏重放任何一项都是静默错。

---

## 3. 数值级 / 行为级差异

### 3.1 SPP 默认值与亚像素抖动

CLI 缺省 `spp = 1`（`OfflineRenderer.cpp:151`），Studio 缺省 4
（`ConfigManager.cpp:72`；`QuantiloomVulkanRenderer.hpp:262`）。不止收敛快慢：
着色器在 `totalSamples > 1` 才开亚像素抖动（`raygen.rgen:89-91`）→ 无 `spp` 键的
配置 **CLI 有锯齿、Studio 没有**。Studio 每次场景加载后还会把存值重推
（`QuantiloomVulkanRenderer.cpp:286`）。

### 3.2 传感器链输出单位

CLI 在 CPU 链后把波段缩放除回，EXR 保持每 nm 平均量（`main.cpp:249-274`，另存
`*_rawdn.exr`）；GPU 链刻意保留波段积分值（`ExternalRenderContext.cpp:3352-3354`）。
LWIR 下 `CaptureDisplayImage()` 的数值比 CLI 的 EXR 大 `band.WidthNm()` = 6000 倍
（`include/quantiloom/core/Types.hpp:398-400`）。CLAHE 仅存在于 GUI 路径
（`ExecuteCLAHE`，`ExternalRenderContext.cpp:3493`），其 `normalizeOutput` 在
Studio 硬编码 true（`QuantiloomVulkanRenderer.cpp:955`），面板与 TOML 均不可控。

### 3.3 随机种子

同为 `std::mt19937`、同默认 `0x51ED`（`core/Types.hpp:556`）。CLI 每次运行播种一次
（`OfflineRenderer.cpp:1395-1404`，`0` = random_device）；GUI 每次
`ResetAccumulation()` 重播种（`ExternalRenderContext.cpp:240-242, 1258-1264`），
`frameIndex` 刻意排除在种子混合外以对齐 CLI（`:944-968`）。等价性设计正确，
但逐样本序列不可复现对齐。

### 3.4 分辨率

`renderer.resolution` 在 Studio 只进标签（`MainWindow.cpp:1990` →
`RenderSettingsPanel.cpp:193-197`，标签自述 "Follows the viewport size"）。
实际按 `swapChainImageSize()` 设备像素渲染（`QuantiloomVulkanRenderer.cpp:103-104`）；
`PassThrough` DPI 舍入（`src/main.cpp:32-33`）下 150% 显示器多渲 1.5 倍像素。

### 3.5 其余 CLI 专属键

`renderer.debug_mode`（`OfflineRenderer.cpp:1064`；Studio 侧按设计属会话状态，见
`src/config/CLAUDE.md`）、`quality.fail_on_srgb_upsample` / `log_material_sources`
（`:247-248`）、全部 `hyperspectral.*`（`:1291-1318`）。
反向地，`[[materials]]` 的 IR 键是 **Studio 发明**（`MainWindow.cpp:2276-2326`）：
硬编码采样点 4000/10000 nm（`:2294-2295`）、反射率合成为
`1 - emissivity - transmittance`（`:2310`）；CLI 没有 `[[materials]]` 读取器。
`spectral.lambda_min/max/delta_lambda` Studio 读入后只进面板，从不推给 SDK
（`ConfigManager.cpp:82-84`，`wavelengthRangeChanged` 无接收者）。

### 3.6 裸模型加载后的相机 FOV 漂移

`AdoptScene` 采用场景相机，但场景加载后的重推块
（`QuantiloomVulkanRenderer.cpp:280-294`）**不含相机**；只有 TOML 路径事后调
`setCamera`（`MainWindow.cpp:2059`）。裸 glTF 打开后 GUI 显示 45° 而 SDK 用场景值。

---

## 4. 核心侧已知顺序约束（改动时勿破坏）

- CLI 装配顺序是依赖图：太阳 LUT 必须先于大气（大气覆写
  `lightingParams.atmosphereTemperature_K`，`OfflineRenderer.cpp:920-925`）；
  材质缓冲必须后于光谱曲线（`:960-982`）。
- `UploadLightingParams()` 是光照缓冲唯一合法写者
  （`ExternalRenderContext.cpp:795-802`）：烘焙期间以 `tGroundK` 替代
  `atmosphereTemperature_K`，绕过它直接写会扭曲 IR 天际线。
- 规格常量每帧在 `RenderFrame` 重同步（`:935-942`）——场景重载会用默认变体重建管线。
- `SetWavelength` 会整体重建材质缓冲（`:1329-1347`），须先于任何材质回读。
- GUI 侧 `contextAdapter` 必须是首个成员且 `Cleanup()` 不得重置（`:482-494`，
  VMA 分配器生命周期）。

---

## 5. 结构性建议（已实施）

> 已落地为 `rendercore::ResolveRenderConfig` / `ResolveMaterialSpectra`
> （内部）与导出的 `ExternalRenderContext::ApplyConfig`。ABI 只增一个符号。
> 下文为当时的建议原文。

分歧几乎全部源于配置解释被写了两遍。修法不在 Qt 侧逐个打补丁，而是按
`RenderCore.hpp:20-23` 已规划的方向，在 **dev 仓库**导出统一的
`ApplyConfigToContext(const Config&, ExternalRenderContext&)`：

- 覆盖 §2/§3 列出的全部键，顺带把 `SpectralBasisLoader` 与
  `ApplyDefaultIRTemperature` 的调用收进去；
- Studio 的 `ConfigManager` / `applySpectralConfig` 退化为薄封装；
- 符合 SRS CON-03（前端只经导出符号触达核心）与 CON-02（物理归核心）。

在此之前，Studio 侧可独立先修、收益最大的三组：

1. **阴影射线默认值**改为 true 对齐 CLI（`ConfigManager.cpp:146`）。
2. **交换链 sRGB**：`setPreferredColorFormats({VK_FORMAT_B8G8R8A8_SRGB})`，
   并向 dev 仓报告 `targetColorFormat` 未被使用 + 头文件 Reinhard 谎言。
3. **光谱正确性组**：实现 `solar_lut_normalise`、IR 波长缺省取波段中心、
   接入 SDK 的 `ParseSpectralMode`、修复 §2.7 的两个状态污染 bug。
