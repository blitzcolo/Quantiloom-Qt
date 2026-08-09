<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>AtmosphericPanel</name>
    <message>
        <source>Preset:</source>
        <translation>预设：</translation>
    </message>
    <message>
        <source>Disabled</source>
        <translation>禁用</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>晴空</translation>
    </message>
    <message>
        <source>Turbulent Clear</source>
        <translation>湍流晴空</translation>
    </message>
    <message>
        <source>Urban Haze</source>
        <translation>城市霾</translation>
    </message>
    <message>
        <source>Fog</source>
        <translation>雾</translation>
    </message>
    <message>
        <source>Light Rain</source>
        <translation>小雨</translation>
    </message>
    <message>
        <source>Heavy Rain</source>
        <translation>大雨</translation>
    </message>
    <message>
        <source>Snow</source>
        <translation>雪</translation>
    </message>
    <message>
        <source>Haze</source>
        <translation>霾</translation>
    </message>
    <message>
        <source>(auto-detect)</source>
        <translation>（自动检测）</translation>
    </message>
    <message>
        <source>Directory of &lt;band&gt;_&lt;geom&gt;_&lt;net&gt;.safetensors files.
Leave empty to auto-detect.</source>
        <translation>存放 &lt;band&gt;_&lt;geom&gt;_&lt;net&gt;.safetensors 文件的目录。
留空则自动检测。</translation>
    </message>
    <message>
        <source>...</source>
        <translation>...</translation>
    </message>
    <message>
        <source>Weather Parameters</source>
        <translation>天气参数</translation>
    </message>
    <message>
        <source>Mid-Latitude Summer (2)</source>
        <translation>中纬度夏季 (2)</translation>
    </message>
    <message>
        <source>Mid-Latitude Winter (3)</source>
        <translation>中纬度冬季 (3)</translation>
    </message>
    <message>
        <source>Rural (1)</source>
        <translation>乡村 (1)</translation>
    </message>
    <message>
        <source>Maritime (4)</source>
        <translation>海洋 (4)</translation>
    </message>
    <message>
        <source>Urban (5)</source>
        <translation>城市 (5)</translation>
    </message>
    <message>
        <source>Advection Fog (9)</source>
        <translation>平流雾 (9)</translation>
    </message>
    <message>
        <source>Radiation Fog (10)</source>
        <translation>辐射雾 (10)</translation>
    </message>
    <message>
        <source>Aerosol (IHAZE):</source>
        <translation>气溶胶 (IHAZE)：</translation>
    </message>
    <message>
        <source>Atmosphere</source>
        <translation>大气</translation>
    </message>
    <message>
        <source>Analytic terms (legacy)</source>
        <translation>解析式参数（旧版）</translation>
    </message>
    <message>
        <source>Transmittance:</source>
        <translation>透过率：</translation>
    </message>
    <message>
        <source>Atmosphere temperature:</source>
        <translation>大气温度：</translation>
    </message>
    <message>
        <source> K</source>
        <translation> K</translation>
    </message>
    <message>
        <source>Superseded: view-path transmittance comes from the network model. Kept because it is still part of the lighting parameters uploaded each frame.</source>
        <translation>已被取代：视线路径透过率现由神经网络模型给出。此处保留是因为它仍属于每帧上传的光照参数。</translation>
    </message>
    <message>
        <source>Thermal-sky fallback for infrared downwelling, used when the network model is off.</source>
        <translation>红外下行辐射的热天空回退值，仅在关闭神经网络模型时使用。</translation>
    </message>
    <message>
        <source>Both values live in the lighting parameters. The network model below supersedes the transmittance; the temperature remains the fallback sky for infrared when that model is disabled.</source>
        <translation>这两个值都属于光照参数。下方的神经网络模型会取代其中的透过率；关闭该模型时，温度仍作为红外的回退天空温度。</translation>
    </message>
    <message>
        <source>Neural network model (MODTRAN surrogate)</source>
        <translation>神经网络模型（MODTRAN 代理）</translation>
    </message>
    <message>
        <source>Use the network model</source>
        <translation>使用神经网络模型</translation>
    </message>
    <message>
        <source>Transmittance and path radiance come from the baked network lookup tables. A model pack must be available; without one the renderer falls back to the analytic terms above.</source>
        <translation>透过率与路径辐亮度取自预烘焙的网络查找表。需要提供模型包；没有模型包时渲染器会回退到上方的解析式参数。</translation>
    </message>
    <message>
        <source>Model pack:</source>
        <translation>模型包：</translation>
    </message>
    <message>
        <source>Atmosphere model:</source>
        <translation>大气模式：</translation>
    </message>
    <message>
        <source>None (0)</source>
        <translation>无 (0)</translation>
    </message>
    <message>
        <source>Rain Cloud (6)</source>
        <translation>雨云 (6)</translation>
    </message>
    <message>
        <source>Cirrus (18)</source>
        <translation>卷云 (18)</translation>
    </message>
    <message>
        <source> km</source>
        <translation> km</translation>
    </message>
    <message>
        <source>Rain rate:</source>
        <translation>降雨强度：</translation>
    </message>
    <message>
        <source> mm/h</source>
        <translation> mm/h</translation>
    </message>
    <message>
        <source>Ground temperature:</source>
        <translation>地表温度：</translation>
    </message>
    <message>
        <source>Relative humidity:</source>
        <translation>相对湿度：</translation>
    </message>
    <message>
        <source> hPa</source>
        <translation> hPa</translation>
    </message>
    <message>
        <source>H₂O scale:</source>
        <translation>水汽缩放系数：</translation>
    </message>
    <message>
        <source>Select the atmosphere model pack directory</source>
        <translation>选择大气模型包目录</translation>
    </message>
    <message>
        <source>Cloud (ICLD):</source>
        <translation>云 (ICLD)：</translation>
    </message>
    <message>
        <source>Visibility:</source>
        <translation>能见度：</translation>
    </message>
    <message>
        <source>Pressure:</source>
        <translation>气压：</translation>
    </message>
</context>
<context>
    <name>CameraPanel</name>
    <message>
        <source>Camera</source>
        <translation>相机</translation>
    </message>
    <message>
        <source>Pose</source>
        <translation>位姿</translation>
    </message>
    <message>
        <source>Position:</source>
        <translation>位置：</translation>
    </message>
    <message>
        <source>Look at:</source>
        <translation>目标点：</translation>
    </message>
    <message>
        <source>Distance:</source>
        <translation>距离：</translation>
    </message>
    <message>
        <source>Lens</source>
        <translation>镜头</translation>
    </message>
    <message>
        <source>Vertical field of view:</source>
        <translation>垂直视场角：</translation>
    </message>
    <message>
        <source>°</source>
        <translation>°</translation>
    </message>
    <message>
        <source>Views</source>
        <translation>视角</translation>
    </message>
    <message>
        <source>Reset View</source>
        <translation>重置视角</translation>
    </message>
    <message>
        <source>Front</source>
        <translation>前</translation>
    </message>
    <message>
        <source>Back</source>
        <translation>后</translation>
    </message>
    <message>
        <source>Right</source>
        <translation>右</translation>
    </message>
    <message>
        <source>Left</source>
        <translation>左</translation>
    </message>
    <message>
        <source>Top</source>
        <translation>顶</translation>
    </message>
    <message>
        <source>Bottom</source>
        <translation>底</translation>
    </message>
    <message>
        <source>Right-drag orbits, middle-drag pans, the wheel zooms. W/A/S/D fly the camera, Q/E move it down and up.</source>
        <translation>右键拖动环绕，中键拖动平移，滚轮缩放视距。W/A/S/D 自由飞行，Q/E 下降与上升。</translation>
    </message>
</context>
<context>
    <name>Commands</name>
    <message>
        <source>Transform Node</source>
        <translation>变换节点</translation>
    </message>
    <message numerus="yes">
        <source>Transform %n Node(s)</source>
        <translation>
            <numerusform>变换 %n 个节点</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Paste %n Object(s)</source>
        <translation>
            <numerusform>粘贴 %n 个物体</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Delete %n Object(s)</source>
        <translation>
            <numerusform>删除 %n 个物体</numerusform>
        </translation>
    </message>
    <message>
        <source>Modify Material</source>
        <translation>修改材质</translation>
    </message>
    <message>
        <source>Change Selection</source>
        <translation>更改选择</translation>
    </message>
</context>
<context>
    <name>ConfigManager</name>
    <message>
        <source>Cannot open file for writing: %1</source>
        <translation>无法写入文件: %1</translation>
    </message>
</context>
<context>
    <name>DebugVisualizationPanel</name>
    <message>
        <source>Debug</source>
        <translation>调试</translation>
    </message>
    <message>
        <source>Debug Mode</source>
        <translation>调试模式</translation>
    </message>
    <message>
        <source>Pixel Inspection</source>
        <translation>像素检查</translation>
    </message>
    <message>
        <source>Position:</source>
        <translation>位置：</translation>
    </message>
    <message>
        <source>Value:</source>
        <translation>数值：</translation>
    </message>
    <message>
        <source>Hover or click the viewport with a debug mode active. Coordinates are device pixels, matching the render target.
Help ▸ Reading Debug Output explains the colour encodings.</source>
        <translation>启用调试模式后，在视口中悬停或点击即可读数。坐标为设备像素，与渲染目标一致。
颜色编码的含义见「帮助 ▸ 调试输出判读」。</translation>
    </message>
    <message>
        <source>%1, %2 px</source>
        <translation>%1, %2 px</translation>
    </message>
    <message>
        <source>read failed</source>
        <translation>读取失败</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>常规</translation>
    </message>
    <message>
        <source>Material</source>
        <translation type="obsolete">材质</translation>
    </message>
    <message>
        <source>Lighting</source>
        <translation type="obsolete">光照</translation>
    </message>
    <message>
        <source>Spectral</source>
        <translation type="obsolete">光谱</translation>
    </message>
</context>
<context>
    <name>DisplayEnhancementPanel</name>
    <message>
        <source>CLAHE Settings</source>
        <translation>CLAHE 设置</translation>
    </message>
    <message>
        <source>Higher values allow more contrast enhancement.
1.0 = no clipping (full equalization)
2.0-4.0 = typical range for infrared</source>
        <translation>数值越大，允许的对比度增强越强。
1.0 表示不截断（完全均衡化）
红外常用范围为 2.0–4.0</translation>
    </message>
    <message>
        <source>4x4</source>
        <translation>4×4</translation>
    </message>
    <message>
        <source>Display Enhancement</source>
        <translation>显示增强</translation>
    </message>
    <message>
        <source>Enable display enhancement</source>
        <translation>启用显示增强</translation>
    </message>
    <message>
        <source>CLAHE lifts contrast in low-dynamic-range images such as infrared. It changes the viewport and saved screenshots; exported images keep their raw values.</source>
        <translation>CLAHE 可提升红外等低动态范围图像的对比度。它只改变视口与保存的截图；导出的图像保留原始数值。</translation>
    </message>
    <message>
        <source>Clip limit:</source>
        <translation>对比度截断：</translation>
    </message>
    <message>
        <source>Tile size:</source>
        <translation>分块大小：</translation>
    </message>
    <message>
        <source>8x8 (default)</source>
        <translation>8×8（默认）</translation>
    </message>
    <message>
        <source>16x16</source>
        <translation>16×16</translation>
    </message>
    <message>
        <source>32x32</source>
        <translation>32×32</translation>
    </message>
    <message>
        <source>Number of contextual tiles.
Smaller tiles = more local contrast.
Larger tiles = more global contrast.</source>
        <translation>上下文分块的数量。
分块越小，局部对比度越强。
分块越大，整体对比度越强。</translation>
    </message>
    <message>
        <source>Processing mode</source>
        <translation>处理方式</translation>
    </message>
    <message>
        <source>Luminance only (recommended)</source>
        <translation>仅亮度通道（推荐）</translation>
    </message>
    <message>
        <source>Apply CLAHE only to the luminance channel,
preserving colour information.</source>
        <translation>只对亮度通道应用 CLAHE，
保留色彩信息。</translation>
    </message>
    <message>
        <source>All channels</source>
        <translation>所有通道</translation>
    </message>
    <message>
        <source>Apply CLAHE independently to each RGB channel.
May cause colour shifts.</source>
        <translation>对 RGB 各通道分别应用 CLAHE。
可能引起色偏。</translation>
    </message>
</context>
<context>
    <name>HelpDialog</name>
    <message>
        <source>Quantiloom Help</source>
        <translation>Quantiloom 帮助</translation>
    </message>
    <message>
        <source>Keyboard Shortcuts</source>
        <translation>快捷键</translation>
    </message>
    <message>
        <source>Debug Output</source>
        <translation>调试输出</translation>
    </message>
    <message>
        <source>Menu and toolbar</source>
        <translation>菜单与工具栏</translation>
    </message>
    <message>
        <source>Viewport</source>
        <translation>视口</translation>
    </message>
    <message>
        <source>This page is generated from the shortcuts the application actually registers, so it cannot drift out of date.</source>
        <translation>本页由程序实际注册的快捷键生成，因此不会与实现脱节。</translation>
    </message>
</context>
<context>
    <name>HyperspectralExportDialog</name>
    <message>
        <source>ENVI BSQ (band sequential)</source>
        <translation>ENVI BSQ（按波段顺序）</translation>
    </message>
    <message>
        <source>ENVI BIL (band interleaved by line)</source>
        <translation>ENVI BIL（按行交织）</translation>
    </message>
    <message>
        <source>ENVI BIP (band interleaved by pixel)</source>
        <translation>ENVI BIP（按像素交织）</translation>
    </message>
    <message>
        <source>GeoTIFF</source>
        <translation>GeoTIFF</translation>
    </message>
    <message>
        <source>Render Hyperspectral Cube</source>
        <translation>渲染高光谱立方体</translation>
    </message>
    <message>
        <source>Traces every band to completion and streams the result to disk. This is an offline render on a device of its own — the viewport keeps working, and the two share the GPU.</source>
        <translation>把每个波段都渲染到收敛，并把结果流式写入磁盘。这是在独立设备上的离线渲染——视口仍可继续使用，两者共享 GPU。</translation>
    </message>
    <message>
        <source>Wavelength Range</source>
        <translation>波长范围</translation>
    </message>
    <message>
        <source> nm</source>
        <translation> nm</translation>
    </message>
    <message>
        <source>From:</source>
        <translation>起始：</translation>
    </message>
    <message>
        <source>To:</source>
        <translation>终止：</translation>
    </message>
    <message>
        <source>Step:</source>
        <translation>步长：</translation>
    </message>
    <message>
        <source>Output</source>
        <translation>输出</translation>
    </message>
    <message>
        <source>Samples per pixel, per band. Every band is traced to this count, so the total cost scales with the band count too.</source>
        <translation>每波段每像素的采样数。每个波段都渲染到该数值，因此总耗时还会随波段数增长。</translation>
    </message>
    <message>
        <source>Samples per band:</source>
        <translation>每波段采样数：</translation>
    </message>
    <message>
        <source>Format:</source>
        <translation>格式：</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>浏览……</translation>
    </message>
    <message>
        <source>Base name:</source>
        <translation>输出基名：</translation>
    </message>
    <message>
        <source>Also save each band as EXR</source>
        <translation>同时把每个波段另存为 EXR</translation>
    </message>
    <message>
        <source>Writes one image per band beside the cube, for inspecting a single wavelength. Multiplies the output size by the band count.</source>
        <translation>在立方体旁为每个波段各写一张图，便于查看单一波长。输出体积会乘以波段数。</translation>
    </message>
    <message>
        <source>Render</source>
        <translation>渲染</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
    <message numerus="yes">
        <source>%n band(s) — each traced to completion</source>
        <translation>
            <numerusform>%n 个波段 — 每个都渲染到收敛</numerusform>
        </translation>
    </message>
    <message>
        <source>Cube Output Base Name</source>
        <translation>立方体输出基名</translation>
    </message>
    <message>
        <source>All Files (*)</source>
        <translation>所有文件 (*)</translation>
    </message>
    <message>
        <source>Could not serialise the current document.</source>
        <translation>无法序列化当前文档。</translation>
    </message>
    <message>
        <source>Preparing — compiling shaders and loading the scene...</source>
        <translation>准备中 — 正在编译着色器并加载场景……</translation>
    </message>
    <message>
        <source>The document is not valid TOML: %1</source>
        <translation>文档不是有效的 TOML：%1</translation>
    </message>
    <message>
        <source>Band %1 of %2 — about %3 s remaining</source>
        <translation>第 %1 / %2 波段 — 约剩余 %3 秒</translation>
    </message>
    <message>
        <source>The render failed: %1</source>
        <translation>渲染失败：%1</translation>
    </message>
    <message>
        <source>Cube written.</source>
        <translation>立方体已写出。</translation>
    </message>
    <message>
        <source>Cube Render Failed</source>
        <translation>立方体渲染失败</translation>
    </message>
</context>
<context>
    <name>LightingPanel</name>
    <message>
        <source>Lighting</source>
        <translation>光照</translation>
    </message>
    <message>
        <source>No spectrum chosen</source>
        <translation>未选择光谱文件</translation>
    </message>
    <message>
        <source>This spectral mode needs an illuminant spectrum. Without one the render is black — the renderer will not substitute a standard spectrum, because a scene that acquired one silently would report radiance nobody asked for.</source>
        <translation>该光谱模式需要光源光谱。没有光源则渲染为全黑——渲染器不会自行代入标准光谱，因为悄悄获得光谱的场景会报出无人要求的辐亮度。</translation>
    </message>
    <message>
        <source>Choose an Illuminant Spectrum</source>
        <translation>选择光源光谱</translation>
    </message>
    <message>
        <source>Spectrum files (*.csv *.txt *.dat);;All Files (*)</source>
        <translation>光谱文件 (*.csv *.txt *.dat);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Sun Direction</source>
        <translation>太阳方向</translation>
    </message>
    <message>
        <source>Azimuth:</source>
        <translation>方位角:</translation>
    </message>
    <message>
        <source>Elevation:</source>
        <translation>仰角:</translation>
    </message>
    <message>
        <source>Illuminant</source>
        <translation>光源</translation>
    </message>
    <message>
        <source>None — RGB radiance only</source>
        <translation>无 — 仅用 RGB 辐亮度</translation>
    </message>
    <message>
        <source>Equal energy (CIE E)</source>
        <translation>等能光源（CIE E）</translation>
    </message>
    <message>
        <source>ASTM G-173 (bundled)</source>
        <translation>ASTM G-173（内置）</translation>
    </message>
    <message>
        <source>From file...</source>
        <translation>从文件选择……</translation>
    </message>
    <message>
        <source>The sun&apos;s spectrum, as distinct from the RGB radiance above. The quantitative spectral modes need one to render anything at all.</source>
        <translation>太阳的光谱，区别于上方的 RGB 辐亮度。量化光谱模式必须有它才能渲染出任何东西。</translation>
    </message>
    <message>
        <source>Choose Spectrum...</source>
        <translation>选择光谱……</translation>
    </message>
    <message>
        <source>Normalise to unit luminance</source>
        <translation>归一化到单位亮度</translation>
    </message>
    <message>
        <source>Published reference spectra are relative, so their absolute level is arbitrary. Both curves scale by the sun&apos;s luminance, which keeps the sun-to-sky ratio the measurement actually recorded.</source>
        <translation>已发布的参考光谱是相对值，绝对量级是任意的。两条曲线都按太阳的亮度缩放，从而保留测量实际记录下来的日天比。</translation>
    </message>
    <message>
        <source>Radiance</source>
        <translation>辐射度</translation>
    </message>
    <message>
        <source>Sun:</source>
        <translation>太阳:</translation>
    </message>
    <message>
        <source>Sky:</source>
        <translation>天空:</translation>
    </message>
    <message>
        <source> W/m²/sr</source>
        <translation> W/m²/sr</translation>
    </message>
    <message>
        <source>Environment Map (IBL)</source>
        <translation>环境贴图 (IBL)</translation>
    </message>
    <message>
        <source>Light the scene from the environment map</source>
        <translation>用环境贴图照亮场景</translation>
    </message>
    <message>
        <source>Off means the map contributes no light at all — not that it is replaced by another sky. What a ray sees when it misses the scene is the sky radiance above, either way.</source>
        <translation>关闭表示这张贴图完全不提供照明，而不是换成另一片天空。光线未命中场景时看到的始终是上方的天空辐亮度。</translation>
    </message>
    <message>
        <source>Browse…</source>
        <translation>浏览…</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>清除</translation>
    </message>
    <message>
        <source>%1°</source>
        <translation>%1°</translation>
    </message>
    <message>
        <source>No map — lighting from the built-in sky</source>
        <translation>未指定贴图 — 由内置天空照明</translation>
    </message>
    <message>
        <source>Preview only — not quantitative: an environment map and an analytic sun or sky are both lighting the scene, so the same illumination is counted twice. An HDRI sky already contains its own sun, and nothing aligns the two directions — expect two specular highlights in different places. Set sun and sky to 0 to light from the map alone, or turn the map off.</source>
        <translation>仅供预览 — 非定量结果：环境贴图与解析太阳／天空同时在照亮场景，同一份照明被重复计入。HDRI 天空本身就含有太阳，而两者的方向并不对齐 — 会在不同位置出现两个镜面高光。将太阳与天空设为 0 以仅用贴图照明，或关闭该贴图。</translation>
    </message>
    <message>
        <source>Choose Environment Map</source>
        <translation>选择环境贴图</translation>
    </message>
    <message>
        <source>Environment Maps (*.exr *.hdr *.png *.jpg *.jpeg);;All Files (*)</source>
        <translation>环境贴图 (*.exr *.hdr *.png *.jpg *.jpeg);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Atmosphere</source>
        <translation type="vanished">大气</translation>
    </message>
    <message>
        <source>Transmittance:</source>
        <translation type="vanished">透射率:</translation>
    </message>
    <message>
        <source>Temperature:</source>
        <translation type="vanished">温度:</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <source>Quantiloom - Spectral Renderer</source>
        <translation type="vanished">Quantiloom - 光谱渲染引擎</translation>
    </message>
    <message>
        <source>&amp;File</source>
        <translation>文件(&amp;F)</translation>
    </message>
    <message>
        <source>&amp;New Scene</source>
        <translation type="vanished">新建场景(&amp;N)</translation>
    </message>
    <message>
        <source>&amp;Open Scene...</source>
        <translation type="vanished">打开场景(&amp;O)...</translation>
    </message>
    <message>
        <source>&amp;Save Scene</source>
        <translation type="vanished">保存场景(&amp;S)</translation>
    </message>
    <message>
        <source>&amp;Import Config...</source>
        <translation type="vanished">导入配置(&amp;I)...</translation>
    </message>
    <message>
        <source>E&amp;xport Config...</source>
        <translation type="vanished">导出配置(&amp;X)...</translation>
    </message>
    <message>
        <source>Export &amp;Image...</source>
        <translation type="vanished">导出图像(&amp;I)...</translation>
    </message>
    <message>
        <source>E&amp;xit</source>
        <translation>退出(&amp;X)</translation>
    </message>
    <message>
        <source>&amp;Edit</source>
        <translation>编辑(&amp;E)</translation>
    </message>
    <message>
        <source>Nothing selected to copy</source>
        <translation>没有选中可复制的物体</translation>
    </message>
    <message numerus="yes">
        <source>Copied %n object(s)</source>
        <translation>
            <numerusform>已复制 %n 个物体</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Pasted %n object(s)</source>
        <translation>
            <numerusform>已粘贴 %n 个物体</numerusform>
        </translation>
    </message>
    <message>
        <source>Nothing to paste</source>
        <translation>剪贴板里没有可粘贴的物体</translation>
    </message>
    <message>
        <source>Clipboard objects no longer exist in this scene</source>
        <translation>剪贴板中的物体已不在当前场景中</translation>
    </message>
    <message>
        <source>Nothing selected to duplicate</source>
        <translation>没有选中可创建副本的物体</translation>
    </message>
    <message>
        <source>Nothing selected to delete</source>
        <translation>没有选中可删除的物体</translation>
    </message>
    <message>
        <source>Cannot delete every object in the scene</source>
        <translation>不能删除场景中的全部物体</translation>
    </message>
    <message numerus="yes">
        <source>Deleted %n object(s)</source>
        <translation>
            <numerusform>已删除 %n 个物体</numerusform>
        </translation>
    </message>
    <message>
        <source>Frame %1 ms wall clock, %2 ms on the GPU</source>
        <translation type="vanished">帧耗时 %1 ms（墙钟），GPU %2 ms</translation>
    </message>
    <message>
        <source>Frame %1 ms wall clock</source>
        <translation type="vanished">帧耗时 %1 ms（墙钟）</translation>
    </message>
    <message>
        <source>ETA %1</source>
        <translation>剩余 %1</translation>
    </message>
    <message>
        <source>Render complete — %1 samples in %2</source>
        <translation>渲染完成 — %1 采样，用时 %2</translation>
    </message>
    <message>
        <source>Select a material in the scene first</source>
        <translation>请先在场景中选择一个材质</translation>
    </message>
    <message>
        <source>Removed the measured spectrum from &apos;%1&apos;</source>
        <translation>已移除“%1”的实测光谱</translation>
    </message>
    <message>
        <source>Assign Failed</source>
        <translation>指派失败</translation>
    </message>
    <message>
        <source>The %1 spectral database was not found beside the application or in the working directory.</source>
        <translation>在应用程序目录和工作目录下都未找到 %1 光谱数据库。</translation>
    </message>
    <message>
        <source>Could not reconstruct &apos;%1&apos; in the %2 band:
%3</source>
        <translation>无法在 %2 波段重建 &apos;%1&apos;：
%3</translation>
    </message>
    <message>
        <source>Could not upload the spectrum for &apos;%1&apos;.</source>
        <translation>无法上传“%1”的光谱。</translation>
    </message>
    <message>
        <source>Bound the spectrum, but the surface renders flat: %1</source>
        <translation>光谱已绑定，但表面渲染为均一反射率：%1</translation>
    </message>
    <message>
        <source>Assigned %1 (%2, %3 band)</source>
        <translation>已指派 %1（%2，%3 波段）</translation>
    </message>
    <message numerus="yes">
        <source>Mixed %n endmember(s) on &apos;%1&apos; (%2, %3 band)</source>
        <translation>
            <numerusform>已在“%1”上混合 %n 个端元（%2，%3 波段）</numerusform>
        </translation>
    </message>
    <message>
        <source>Node %1</source>
        <translation>节点 %1</translation>
    </message>
    <message>
        <source>&apos;%1&apos; selected</source>
        <translation>已选中“%1”</translation>
    </message>
    <message>
        <source>&amp;Undo</source>
        <translation>撤销(&amp;U)</translation>
    </message>
    <message>
        <source>&amp;Undo %1</source>
        <translation>撤销 %1(&amp;U)</translation>
    </message>
    <message>
        <source>&amp;Redo</source>
        <translation>重做(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Delete</source>
        <translation type="vanished">删除(&amp;D)</translation>
    </message>
    <message>
        <source>&amp;View</source>
        <translation>视图(&amp;V)</translation>
    </message>
    <message>
        <source>&amp;Reset Camera</source>
        <translation type="vanished">重置相机(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Parameter Panel</source>
        <translation type="vanished">参数面板(&amp;P)</translation>
    </message>
    <message>
        <source>&amp;Render</source>
        <translation>渲染(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Start Render</source>
        <translation>开始渲染(&amp;S)</translation>
    </message>
    <message>
        <source>S&amp;top Render</source>
        <translation>停止渲染(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Tools</source>
        <translation>工具(&amp;T)</translation>
    </message>
    <message>
        <source>Spectral Material &amp;Generator...</source>
        <translation type="vanished">光谱材质生成器(&amp;G)...</translation>
    </message>
    <message>
        <source>Spectral Gen</source>
        <translation type="vanished">光谱生成</translation>
    </message>
    <message>
        <source>&amp;Help</source>
        <translation>帮助(&amp;H)</translation>
    </message>
    <message>
        <source>&amp;About</source>
        <translation>关于(&amp;A)</translation>
    </message>
    <message>
        <source>About &amp;Qt</source>
        <translation>关于 Qt(&amp;Q)</translation>
    </message>
    <message>
        <source>Parameters</source>
        <translation type="vanished">参数</translation>
    </message>
    <message>
        <source>Scene</source>
        <translation type="vanished">场景</translation>
    </message>
    <message>
        <source>Material</source>
        <translation type="vanished">材质</translation>
    </message>
    <message>
        <source>Lighting</source>
        <translation>光照</translation>
    </message>
    <message>
        <source>Atmosphere</source>
        <translation>大气</translation>
    </message>
    <message>
        <source>Render</source>
        <translation type="vanished">渲染</translation>
    </message>
    <message>
        <source>Spectral</source>
        <translation type="vanished">光谱</translation>
    </message>
    <message>
        <source>Atmospheric preset: %1</source>
        <translation>大气预设：%1</translation>
    </message>
    <message>
        <source>Sensor simulation enabled</source>
        <translation>已启用传感器仿真</translation>
    </message>
    <message>
        <source>Sensor simulation disabled</source>
        <translation>已关闭传感器仿真</translation>
    </message>
    <message>
        <source>Ready</source>
        <translation>就绪</translation>
    </message>
    <message>
        <source>FPS: --</source>
        <translation type="vanished">帧率: --</translation>
    </message>
    <message>
        <source>Samples: 0</source>
        <translation type="vanished">采样数: 0</translation>
    </message>
    <message>
        <source>Unsaved Changes</source>
        <translation>未保存的更改</translation>
    </message>
    <message>
        <source>The scene has been modified. Do you want to save your changes?</source>
        <translation type="vanished">场景已被修改。是否保存更改？</translation>
    </message>
    <message>
        <source>New scene created</source>
        <translation type="vanished">已创建新场景</translation>
    </message>
    <message>
        <source>Open Scene</source>
        <translation type="vanished">打开场景</translation>
    </message>
    <message>
        <source>3D Scene Files (*.gltf *.glb *.usd *.usda *.usdc *.usdz);;glTF Files (*.gltf *.glb);;OpenUSD Files (*.usd *.usda *.usdc *.usdz);;TOML Config (*.toml);;All Files (*)</source>
        <translation type="vanished">3D 场景文件 (*.gltf *.glb *.usd *.usda *.usdc *.usdz);;glTF 文件 (*.gltf *.glb);;OpenUSD 文件 (*.usd *.usda *.usdc *.usdz);;TOML 配置 (*.toml);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Config loaded: %1</source>
        <translation type="vanished">配置已加载: %1</translation>
    </message>
    <message>
        <source>Load Failed</source>
        <translation type="vanished">加载失败</translation>
    </message>
    <message>
        <source>Failed to load config: %1</source>
        <translation type="vanished">配置加载失败: %1</translation>
    </message>
    <message>
        <source>Loading: %1</source>
        <translation type="vanished">加载中: %1</translation>
    </message>
    <message>
        <source>Save Scene</source>
        <translation type="vanished">保存场景</translation>
    </message>
    <message>
        <source>TOML Config (*.toml)</source>
        <translation type="vanished">TOML 配置 (*.toml)</translation>
    </message>
    <message>
        <source>Saved: %1</source>
        <translation type="vanished">已保存: %1</translation>
    </message>
    <message>
        <source>Export failed</source>
        <translation>导出失败</translation>
    </message>
    <message>
        <source>Debug mode: %1</source>
        <translation>调试模式：%1</translation>
    </message>
    <message>
        <source>Import Configuration</source>
        <translation type="vanished">导入配置</translation>
    </message>
    <message>
        <source>TOML Config (*.toml);;All Files (*)</source>
        <translation type="vanished">TOML 配置 (*.toml);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Config imported: %1</source>
        <translation type="vanished">配置已导入: %1</translation>
    </message>
    <message>
        <source>Import Failed</source>
        <translation type="vanished">导入失败</translation>
    </message>
    <message>
        <source>Failed to import config: %1</source>
        <translation type="vanished">配置导入失败: %1</translation>
    </message>
    <message>
        <source>Export Configuration</source>
        <translation type="vanished">导出配置</translation>
    </message>
    <message>
        <source>Config exported: %1</source>
        <translation type="vanished">配置已导出: %1</translation>
    </message>
    <message>
        <source>Export Failed</source>
        <translation>导出失败</translation>
    </message>
    <message>
        <source>Failed to export config: %1</source>
        <translation type="vanished">配置导出失败: %1</translation>
    </message>
    <message>
        <source>Export Image</source>
        <translation type="vanished">导出图像</translation>
    </message>
    <message>
        <source>EXR Image (*.exr);;PNG Image (*.png);;All Files (*)</source>
        <translation>EXR 图像 (*.exr);;PNG 图像 (*.png);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Exported: %1</source>
        <translation type="vanished">已导出: %1</translation>
    </message>
    <message>
        <source>Rendering...</source>
        <translation type="vanished">渲染中...</translation>
    </message>
    <message>
        <source>Render stopped</source>
        <translation type="vanished">渲染已停止</translation>
    </message>
    <message>
        <source>Camera reset</source>
        <translation>相机已重置</translation>
    </message>
    <message>
        <source>About Quantiloom</source>
        <translation>关于 Quantiloom</translation>
    </message>
    <message>
        <source>&lt;h3&gt;Quantiloom&lt;/h3&gt;&lt;p&gt;Version 0.1.8&lt;/p&gt;&lt;p&gt;A spectral renderer with hardware ray tracing support.&lt;/p&gt;&lt;p&gt;Features:&lt;/p&gt;&lt;ul&gt;&lt;li&gt;Hardware ray tracing&lt;/li&gt;&lt;li&gt;Spectral rendering&lt;/li&gt;&lt;li&gt;PBR materials with spectral extensions&lt;/li&gt;&lt;li&gt;Atmospheric scattering&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;Copyright (c) 2025-2026 blitzcolo&lt;/p&gt;</source>
        <translation type="vanished">&lt;h3&gt;Quantiloom&lt;/h3&gt;&lt;p&gt;版本 0.1.8&lt;/p&gt;&lt;p&gt;支持硬件光线追踪的光谱渲染器。&lt;/p&gt;&lt;p&gt;功能特性：&lt;/p&gt;&lt;ul&gt;&lt;li&gt;硬件光线追踪&lt;/li&gt;&lt;li&gt;光谱渲染&lt;/li&gt;&lt;li&gt;支持光谱扩展的PBR材质&lt;/li&gt;&lt;li&gt;大气散射&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;版权所有 (c) 2025-2026 blitzcolo&lt;/p&gt;</translation>
    </message>
    <message>
        <source>FPS: %1</source>
        <translation type="vanished">帧率: %1</translation>
    </message>
    <message>
        <source>Samples: %1</source>
        <translation>采样数: %1</translation>
    </message>
    <message>
        <source>Node %1 selected</source>
        <translation type="vanished">已选中节点 %1</translation>
    </message>
    <message>
        <source>Material &apos;%1&apos; selected</source>
        <translation>已选中材质 &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Material modified</source>
        <translation>材质已修改</translation>
    </message>
    <message>
        <source>-- spp/s</source>
        <translation>-- spp/s</translation>
    </message>
    <message>
        <source>%1 spp/s</source>
        <translation>%1 spp/s</translation>
    </message>
    <message>
        <source>Samples accumulated per second of wall clock</source>
        <translation>每墙钟秒累积的采样数</translation>
    </message>
    <message>
        <source>Samples per second of wall clock. %1 frames/s, %2 ms per frame, %3 ms of it on the GPU</source>
        <translation>每墙钟秒累积的采样数。%1 帧/秒，每帧 %2 ms，其中 GPU %3 ms</translation>
    </message>
    <message>
        <source>Samples per second of wall clock. %1 frames/s, %2 ms per frame</source>
        <translation>每墙钟秒累积的采样数。%1 帧/秒，每帧 %2 ms</translation>
    </message>
    <message>
        <source>Lighting updated</source>
        <translation>光照已更新</translation>
    </message>
    <message>
        <source>SPP set to %1</source>
        <translation type="vanished">SPP 已设为 %1</translation>
    </message>
    <message>
        <source>Spectral mode: %1</source>
        <translation>光谱模式: %1</translation>
    </message>
    <message>
        <source>Target samples: infinite</source>
        <translation>目标采样数：无限</translation>
    </message>
    <message>
        <source>Target samples: %1</source>
        <translation>目标采样数：%1</translation>
    </message>
    <message>
        <source>Illuminant: RGB radiance only</source>
        <translation>光源：仅用 RGB 辐亮度</translation>
    </message>
    <message>
        <source>Illuminant Not Found</source>
        <translation>未找到光源文件</translation>
    </message>
    <message>
        <source>assets/luts/astmg173.csv was not found beside the application or in the working directory.</source>
        <translation>在应用程序目录和工作目录下都未找到 assets/luts/astmg173.csv。</translation>
    </message>
    <message>
        <source>Illuminant Failed</source>
        <translation>光源加载失败</translation>
    </message>
    <message>
        <source>Could not load the illuminant:
%1</source>
        <translation>无法加载光源：
%1</translation>
    </message>
    <message>
        <source>Illuminant loaded</source>
        <translation>光源已加载</translation>
    </message>
    <message>
        <source>Sensor parameters updated</source>
        <translation>传感器参数已更新</translation>
    </message>
    <message>
        <source>Display enhancement on (CLAHE: clip %1, %2x%2 tiles)</source>
        <translation>已启用显示增强（CLAHE：截断 %1，%2×%2 分块）</translation>
    </message>
    <message>
        <source>Display enhancement off</source>
        <translation>已关闭显示增强</translation>
    </message>
    <message>
        <source>MCP server stopped</source>
        <translation>MCP 服务已停止</translation>
    </message>
    <message>
        <source>MCP Server</source>
        <translation>MCP 服务</translation>
    </message>
    <message>
        <source>Could not start the MCP server.

%1</source>
        <translation>无法启动 MCP 服务。

%1</translation>
    </message>
    <message>
        <source>MCP server on 127.0.0.1:%1</source>
        <translation>MCP 服务运行于 127.0.0.1:%1</translation>
    </message>
    <message>
        <source>MCP :%1</source>
        <translation>MCP :%1</translation>
    </message>
    <message>
        <source>Spectral mode</source>
        <translation>光谱模式</translation>
    </message>
    <message>
        <source>Wavelength</source>
        <translation>波长</translation>
    </message>
    <message>
        <source>Sensor simulation</source>
        <translation>传感器仿真</translation>
    </message>
    <message>
        <source>Sensor parameters</source>
        <translation>传感器参数</translation>
    </message>
    <message>
        <source>Workspace: %1</source>
        <translation>工作区：%1</translation>
    </message>
    <message>
        <source>The renderer failed to start</source>
        <translation>渲染器启动失败</translation>
    </message>
    <message>
        <source>Renderer Unavailable</source>
        <translation>渲染器不可用</translation>
    </message>
    <message>
        <source>Local space</source>
        <translation>局部坐标系</translation>
    </message>
    <message>
        <source>World space</source>
        <translation>世界坐标系</translation>
    </message>
    <message>
        <source>&amp;Open...</source>
        <translation>打开(&amp;O)...</translation>
    </message>
    <message>
        <source>Open &amp;Recent</source>
        <translation>打开最近使用的场景(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Save</source>
        <translation>保存(&amp;S)</translation>
    </message>
    <message>
        <source>Save &amp;As...</source>
        <translation>另存为(&amp;A)...</translation>
    </message>
    <message>
        <source>Export &amp;Image (raw render)...</source>
        <translation>导出图像（原始渲染结果）(&amp;I)...</translation>
    </message>
    <message>
        <source>Render Hyperspectral &amp;Cube...</source>
        <translation>渲染高光谱立方体(&amp;C)……</translation>
    </message>
    <message>
        <source>Trace every band to completion and stream the cube to disk</source>
        <translation>把每个波段渲染到收敛并将立方体流式写入磁盘</translation>
    </message>
    <message>
        <source>Write the accumulated render without display enhancement.</source>
        <translation>写出累积得到的渲染结果，不含显示增强。</translation>
    </message>
    <message>
        <source>Save Screensho&amp;t (as displayed)</source>
        <translation>保存截图（所见即所得）(&amp;T)</translation>
    </message>
    <message>
        <source>Write what the viewport shows, display enhancement included.</source>
        <translation>写出视口当前显示的画面，含显示增强。</translation>
    </message>
    <message>
        <source>&amp;Transform</source>
        <translation>变换(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Translate</source>
        <translation>移动(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Rotate</source>
        <translation>旋转(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Scale</source>
        <translation>缩放(&amp;S)</translation>
    </message>
    <message>
        <source>Constrain to &amp;X</source>
        <translation type="vanished">约束到 X 轴(&amp;X)</translation>
    </message>
    <message>
        <source>Constrain to &amp;Y</source>
        <translation type="vanished">约束到 Y 轴(&amp;Y)</translation>
    </message>
    <message>
        <source>Constrain to &amp;Z</source>
        <translation type="vanished">约束到 Z 轴(&amp;Z)</translation>
    </message>
    <message>
        <source>&amp;Local Space</source>
        <translation>局部坐标系(&amp;L)</translation>
    </message>
    <message>
        <source>Transform along the object&apos;s own axes instead of the world&apos;s</source>
        <translation>沿物体自身的坐标轴变换，而非世界坐标轴</translation>
    </message>
    <message>
        <source>Select &amp;All</source>
        <translation>全选(&amp;A)</translation>
    </message>
    <message>
        <source>&amp;Invert Selection</source>
        <translation>反选(&amp;I)</translation>
    </message>
    <message>
        <source>&amp;Copy</source>
        <translation>复制(&amp;C)</translation>
    </message>
    <message>
        <source>&amp;Paste</source>
        <translation>粘贴(&amp;P)</translation>
    </message>
    <message>
        <source>Paste as instances: geometry and materials stay shared with the source.</source>
        <translation>以实例方式粘贴：几何体与材质与源物体保持共享。</translation>
    </message>
    <message>
        <source>D&amp;uplicate</source>
        <translation>创建副本(&amp;U)</translation>
    </message>
    <message>
        <source>De&amp;lete</source>
        <translation>删除(&amp;L)</translation>
    </message>
    <message>
        <source>&amp;Preferences...</source>
        <translation>首选项(&amp;P)...</translation>
    </message>
    <message>
        <source>&amp;Workspace</source>
        <translation>工作区(&amp;W)</translation>
    </message>
    <message>
        <source>&amp;Panels</source>
        <translation>面板(&amp;P)</translation>
    </message>
    <message>
        <source>&amp;Reset Layout</source>
        <translation>重置布局(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Theme</source>
        <translation>主题(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Camera</source>
        <translation>相机(&amp;C)</translation>
    </message>
    <message>
        <source>&amp;Reset View</source>
        <translation>重置视角(&amp;R)</translation>
    </message>
    <message>
        <source>&amp;Frame Selected</source>
        <translation>聚焦选中(&amp;F)</translation>
    </message>
    <message>
        <source>Orbit around the selection and pull back to fit it</source>
        <translation>以选中对象为轴心，并拉远至完整可见</translation>
    </message>
    <message>
        <source>Frame &amp;All</source>
        <translation>聚焦全部(&amp;A)</translation>
    </message>
    <message>
        <source>&amp;Orthographic</source>
        <translation>正交投影(&amp;O)</translation>
    </message>
    <message>
        <source>Parallel projection: edges stay parallel and two things the same size measure the same at any depth</source>
        <translation>平行投影：边保持平行，同样大小的物体在任何深度上量出来都一样</translation>
    </message>
    <message>
        <source>&amp;Front</source>
        <translation>前视图(&amp;F)</translation>
    </message>
    <message>
        <source>&amp;Back</source>
        <translation>后视图(&amp;B)</translation>
    </message>
    <message>
        <source>Ri&amp;ght</source>
        <translation>右视图(&amp;G)</translation>
    </message>
    <message>
        <source>&amp;Left</source>
        <translation>左视图(&amp;L)</translation>
    </message>
    <message>
        <source>&amp;Top</source>
        <translation>顶视图(&amp;T)</translation>
    </message>
    <message>
        <source>Botto&amp;m</source>
        <translation>底视图(&amp;M)</translation>
    </message>
    <message>
        <source>&amp;Debug Visualization</source>
        <translation>调试可视化(&amp;D)</translation>
    </message>
    <message>
        <source>Display &amp;Enhancement (CLAHE)</source>
        <translation>显示增强（CLAHE）(&amp;E)</translation>
    </message>
    <message>
        <source>Affects the viewport and screenshots only; exported images keep their raw values.</source>
        <translation>只影响视口与截图；导出的图像保留原始数值。</translation>
    </message>
    <message>
        <source>Show &amp;Grid</source>
        <translation>显示网格(&amp;G)</translation>
    </message>
    <message>
        <source>Ground grid overlay in the viewport; does not affect renders or accumulation.</source>
        <translation>在视口中叠加地面网格；不影响渲染结果与累积。</translation>
    </message>
    <message>
        <source>Discard the accumulated samples and render from scratch</source>
        <translation>丢弃已累积的采样，从头开始渲染</translation>
    </message>
    <message>
        <source>&amp;Resume Render</source>
        <translation>继续渲染(&amp;R)</translation>
    </message>
    <message>
        <source>Carry on from the samples already accumulated</source>
        <translation>从已累积的采样继续</translation>
    </message>
    <message>
        <source>Reset &amp;Accumulation</source>
        <translation>重置累积(&amp;A)</translation>
    </message>
    <message>
        <source>&amp;Quality</source>
        <translation>质量(&amp;Q)</translation>
    </message>
    <message>
        <source>&amp;Spectral Mode</source>
        <translation>光谱模式(&amp;S)</translation>
    </message>
    <message>
        <source>Main Toolbar</source>
        <translation>主工具栏</translation>
    </message>
    <message>
        <source> Spectral: </source>
        <translation> 光谱：</translation>
    </message>
    <message>
        <source> Debug: </source>
        <translation> 调试：</translation>
    </message>
    <message>
        <source>&amp;Assign Measured Spectrum</source>
        <translation>指派实测光谱(&amp;A)</translation>
    </message>
    <message>
        <source>Bind the material selected in the scene to the spectrum highlighted in the Spectral Library, replacing anything already bound</source>
        <translation>把场景中选中的材质绑定到光谱库里高亮的那条光谱，并替换已绑定的全部端元</translation>
    </message>
    <message>
        <source>Add Spectral &amp;Endmember</source>
        <translation>添加光谱端元(&amp;E)</translation>
    </message>
    <message>
        <source>Add the highlighted spectrum alongside the ones already bound, so the surface renders as a mixture of measured materials</source>
        <translation>在已绑定的光谱之外再加上高亮的这条，使表面渲染为多种实测材质的混合</translation>
    </message>
    <message>
        <source>Spectral Material &amp;Generator</source>
        <translation>光谱材质生成器(&amp;G)</translation>
    </message>
    <message>
        <source>&amp;MCP Server</source>
        <translation>MCP 服务(&amp;M)</translation>
    </message>
    <message>
        <source>Let an agent drive Studio over a local connection</source>
        <translation>让 Agent 通过本机连接操作 Studio</translation>
    </message>
    <message>
        <source>&amp;Keyboard Shortcuts</source>
        <translation>快捷键参考(&amp;K)</translation>
    </message>
    <message>
        <source>Reading &amp;Debug Output</source>
        <translation>调试输出判读(&amp;D)</translation>
    </message>
    <message>
        <source>Hover the viewport to inspect</source>
        <translation>把光标悬停在视口上以查看数值</translation>
    </message>
    <message>
        <source>Untitled</source>
        <translation>未命名</translation>
    </message>
    <message>
        <source>%1[*] — Quantiloom Studio</source>
        <translation>%1[*] — Quantiloom Studio</translation>
    </message>
    <message>
        <source>The scene configuration has been modified. Save your changes?</source>
        <translation>场景配置已修改，是否保存？</translation>
    </message>
    <message>
        <source>Open Failed</source>
        <translation>打开失败</translation>
    </message>
    <message>
        <source>This file no longer exists:
%1</source>
        <translation>该文件已不存在：
%1</translation>
    </message>
    <message>
        <source>(none)</source>
        <translation>（无）</translation>
    </message>
    <message>
        <source>Clear List</source>
        <translation>清空列表</translation>
    </message>
    <message>
        <source>Open Scene or Configuration</source>
        <translation>打开场景或配置</translation>
    </message>
    <message>
        <source>Scenes and Configurations (*.toml *.gltf *.glb *.usd *.usda *.usdc *.usdz);;TOML Configuration (*.toml);;glTF Files (*.gltf *.glb);;OpenUSD Files (*.usd *.usda *.usdc *.usdz);;All Files (*)</source>
        <translation>场景与配置文件 (*.toml *.gltf *.glb *.usd *.usda *.usdc *.usdz);;TOML 配置 (*.toml);;glTF 文件 (*.gltf *.glb);;OpenUSD 文件 (*.usd *.usda *.usdc *.usdz);;所有文件 (*)</translation>
    </message>
    <message>
        <source>No such file: %1</source>
        <translation>找不到文件：%1</translation>
    </message>
    <message>
        <source>Failed to load configuration: %1</source>
        <translation>加载配置失败：%1</translation>
    </message>
    <message>
        <source>%1 is not a scene configuration: it names no scene.gltf or scene.usd.</source>
        <translation>%1 不是场景配置：其中没有指定 scene.gltf 或 scene.usd。</translation>
    </message>
    <message>
        <source>Loaded configuration: %1</source>
        <translation>已加载配置：%1</translation>
    </message>
    <message>
        <source>Loading %1...</source>
        <translation>正在加载 %1...</translation>
    </message>
    <message>
        <source>Save Configuration As</source>
        <translation>配置另存为</translation>
    </message>
    <message>
        <source>TOML Configuration (*.toml)</source>
        <translation>TOML 配置 (*.toml)</translation>
    </message>
    <message>
        <source>Save Failed</source>
        <translation>保存失败</translation>
    </message>
    <message>
        <source>Failed to write configuration: %1</source>
        <translation>写入配置失败：%1</translation>
    </message>
    <message>
        <source>Save failed</source>
        <translation>保存失败</translation>
    </message>
    <message>
        <source>Saved %1</source>
        <translation>已保存 %1</translation>
    </message>
    <message>
        <source>No Scene</source>
        <translation>没有场景</translation>
    </message>
    <message>
        <source>Open a scene before rendering a cube.</source>
        <translation>请先打开场景再渲染立方体。</translation>
    </message>
    <message>
        <source>Export Image (raw render)</source>
        <translation>导出图像（原始渲染结果）</translation>
    </message>
    <message>
        <source>Failed to capture the image. Make sure a scene is loaded.</source>
        <translation>抓取图像失败，请确认已加载场景。</translation>
    </message>
    <message>
        <source>Exported %1</source>
        <translation>已导出 %1</translation>
    </message>
    <message>
        <source>Failed to save the image:
%1</source>
        <translation>保存图像失败：
%1</translation>
    </message>
    <message>
        <source>Rendering (infinite)</source>
        <translation>正在渲染（无限）</translation>
    </message>
    <message>
        <source>Rendering from scratch to %1 samples</source>
        <translation>正在从头渲染，目标 %1 个采样</translation>
    </message>
    <message>
        <source>Resuming from %1 samples (infinite)</source>
        <translation>从 %1 采样继续（无限）</translation>
    </message>
    <message>
        <source>Resuming from %1 samples to %2</source>
        <translation>从 %1 采样继续至 %2</translation>
    </message>
    <message>
        <source>Rendering stopped at %1 samples</source>
        <translation>渲染已在第 %1 个采样处停止</translation>
    </message>
    <message>
        <source>Render complete, but the image could not be captured</source>
        <translation>渲染完成，但无法捕获图像</translation>
    </message>
    <message>
        <source>Render complete, but %1 could not be created</source>
        <translation>渲染完成，但无法创建 %1</translation>
    </message>
    <message>
        <source>Render complete — %1 samples, exported to %2</source>
        <translation>渲染完成 — %1 采样，已导出至 %2</translation>
    </message>
    <message>
        <source>Render complete, but the export to %1 failed</source>
        <translation>渲染完成，但导出至 %1 失败</translation>
    </message>
    <message>
        <source>Orthographic projection</source>
        <translation>正交投影</translation>
    </message>
    <message>
        <source>Perspective projection</source>
        <translation>透视投影</translation>
    </message>
    <message numerus="yes">
        <source>Framed %n object(s)</source>
        <translation>
            <numerusform>已聚焦 %n 个对象</numerusform>
        </translation>
    </message>
    <message>
        <source>Framed the scene</source>
        <translation>已聚焦整个场景</translation>
    </message>
    <message>
        <source>Layout reset for %1</source>
        <translation>已重置“%1”工作区的布局</translation>
    </message>
    <message>
        <source>Failed to capture the screenshot. Make sure a scene is loaded.</source>
        <translation>抓取截图失败，请确认已加载场景。</translation>
    </message>
    <message>
        <source>Failed to create the screenshot directory:
%1</source>
        <translation>创建截图目录失败：
%1</translation>
    </message>
    <message>
        <source>Failed to save the EXR file:
%1</source>
        <translation>保存 EXR 文件失败：
%1</translation>
    </message>
    <message>
        <source>The EXR was saved but the PNG failed:
%1</source>
        <translation>EXR 已保存，但 PNG 保存失败：
%1</translation>
    </message>
    <message>
        <source>&lt;h3&gt;Quantiloom&lt;/h3&gt;&lt;p&gt;Version %1&lt;/p&gt;&lt;p&gt;A spectral renderer with hardware ray tracing support.&lt;/p&gt;&lt;p&gt;Features:&lt;/p&gt;&lt;ul&gt;&lt;li&gt;Hardware ray tracing&lt;/li&gt;&lt;li&gt;Spectral rendering&lt;/li&gt;&lt;li&gt;PBR materials with spectral extensions&lt;/li&gt;&lt;li&gt;Atmospheric scattering&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;Copyright (c) 2025-2026 blitzcolo&lt;/p&gt;</source>
        <translation>&lt;h3&gt;Quantiloom&lt;/h3&gt;&lt;p&gt;版本 %1&lt;/p&gt;&lt;p&gt;支持硬件光线追踪的光谱渲染器。&lt;/p&gt;&lt;p&gt;功能特性：&lt;/p&gt;&lt;ul&gt;&lt;li&gt;硬件光线追踪&lt;/li&gt;&lt;li&gt;光谱渲染&lt;/li&gt;&lt;li&gt;支持光谱扩展的PBR材质&lt;/li&gt;&lt;li&gt;大气散射&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;版权所有 (c) 2025-2026 blitzcolo&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Right-drag</source>
        <translation>右键拖动</translation>
    </message>
    <message>
        <source>Orbit the camera</source>
        <translation>环绕相机</translation>
    </message>
    <message>
        <source>Middle-drag</source>
        <translation>中键拖动</translation>
    </message>
    <message>
        <source>Pan the camera</source>
        <translation>平移相机</translation>
    </message>
    <message>
        <source>Wheel</source>
        <translation>滚轮</translation>
    </message>
    <message>
        <source>Zoom</source>
        <translation>缩放视距</translation>
    </message>
    <message>
        <source>W / A / S / D</source>
        <translation>W / A / S / D</translation>
    </message>
    <message>
        <source>Fly the camera</source>
        <translation>自由飞行</translation>
    </message>
    <message>
        <source>Q / E</source>
        <translation>Q / E</translation>
    </message>
    <message>
        <source>Move the camera down / up</source>
        <translation>相机下降／上升</translation>
    </message>
    <message>
        <source>Shift</source>
        <translation>Shift</translation>
    </message>
    <message>
        <source>Fine control while dragging</source>
        <translation>拖动时进行精细调整</translation>
    </message>
    <message>
        <source>G</source>
        <translation>G</translation>
    </message>
    <message>
        <source>Translate mode</source>
        <translation>移动模式</translation>
    </message>
    <message>
        <source>R</source>
        <translation>R</translation>
    </message>
    <message>
        <source>Rotate mode</source>
        <translation>旋转模式</translation>
    </message>
    <message>
        <source>T</source>
        <translation>T</translation>
    </message>
    <message>
        <source>Scale mode</source>
        <translation>缩放模式</translation>
    </message>
    <message>
        <source>X / Y / Z</source>
        <translation>X / Y / Z</translation>
    </message>
    <message>
        <source>Constrain the transform to an axis</source>
        <translation>把变换约束到某一坐标轴</translation>
    </message>
    <message>
        <source>Space</source>
        <translation>Space</translation>
    </message>
    <message>
        <source>Toggle world / local space</source>
        <translation>切换世界／局部坐标系</translation>
    </message>
    <message>
        <source>Escape</source>
        <translation>Esc</translation>
    </message>
    <message>
        <source>Cancel the drag, then clear the selection</source>
        <translation>取消拖动，再清除选择</translation>
    </message>
    <message>
        <source>Left-drag</source>
        <translation>左键拖动</translation>
    </message>
    <message>
        <source>Transform the selection</source>
        <translation>变换所选对象</translation>
    </message>
    <message>
        <source>Hover</source>
        <translation>悬停</translation>
    </message>
    <message>
        <source>Read the pixel under the cursor in debug modes</source>
        <translation>在调试模式下读取光标处的像素值</translation>
    </message>
    <message>
        <source>Preferences saved</source>
        <translation>首选项已保存</translation>
    </message>
    <message>
        <source>Failed to load environment map: %1</source>
        <translation>环境贴图加载失败：%1</translation>
    </message>
    <message>
        <source>Environment map cleared</source>
        <translation>已清除环境贴图</translation>
    </message>
    <message>
        <source>Lighting from %1</source>
        <translation>正在使用 %1 照明</translation>
    </message>
    <message>
        <source>Environment map off — it contributes no light</source>
        <translation>环境贴图已关闭 — 不提供任何照明</translation>
    </message>
    <message>
        <source>Wavelength: %1 nm</source>
        <translation>波长: %1 nm</translation>
    </message>
    <message>
        <source>Accumulation reset</source>
        <translation>累积已重置</translation>
    </message>
    <message>
        <source>Loaded %1 spectral curve(s)</source>
        <translation type="vanished">已加载 %1 条光谱曲线</translation>
    </message>
    <message>
        <source>&amp;Redo %1</source>
        <translation>重做 %1(&amp;R)</translation>
    </message>
    <message>
        <source>Select a debug mode to inspect pixels</source>
        <translation>先选择一种调试模式才能查看像素值</translation>
    </message>
    <message>
        <source>%1 %2</source>
        <translation>%1 %2</translation>
    </message>
    <message>
        <source>(%1,%2) %3</source>
        <translation>(%1,%2) %3</translation>
    </message>
    <message>
        <source>(%1,%2) read failed</source>
        <translation>(%1,%2) 读取失败</translation>
    </message>
    <message>
        <source>Scene Load Failed</source>
        <translation>场景加载失败</translation>
    </message>
    <message>
        <source>Failed to load scene</source>
        <translation>场景加载失败</translation>
    </message>
    <message>
        <source>Click in Scene panel to select objects</source>
        <translation type="vanished">点击场景面板选择对象</translation>
    </message>
    <message>
        <source>Selection cleared</source>
        <translation type="vanished">选择已清除</translation>
    </message>
    <message>
        <source>%1 objects selected</source>
        <translation>已选中 %1 个对象</translation>
    </message>
    <message>
        <source>Selection cleared - click a node in Scene panel to select</source>
        <translation type="vanished">选择已清除 - 点击场景面板中的节点进行选择</translation>
    </message>
    <message>
        <source>&apos;%1&apos; selected - Left-drag in viewport to transform</source>
        <translation type="vanished">已选中 &apos;%1&apos; - 在视口中左键拖拽进行变换</translation>
    </message>
    <message>
        <source>%1 objects selected - Left-drag in viewport to transform</source>
        <translation type="vanished">已选中 %1 个对象 - 在视口中左键拖拽进行变换</translation>
    </message>
    <message>
        <source>Scene loaded - Click a node in Scene panel to select, use G/R/T keys to change transform mode</source>
        <translation type="vanished">场景已加载 - 点击场景面板中的节点进行选择，使用 G/R/T 键切换变换模式</translation>
    </message>
    <message>
        <source>[G] Translate</source>
        <translation>[G] 移动</translation>
    </message>
    <message>
        <source>[R] Rotate</source>
        <translation>[R] 旋转</translation>
    </message>
    <message>
        <source>[T] Scale</source>
        <translation>[T] 缩放</translation>
    </message>
    <message>
        <source>Mode: %1</source>
        <translation type="vanished">模式: %1</translation>
    </message>
    <message>
        <source>&amp;Settings</source>
        <translation type="vanished">设置(&amp;S)</translation>
    </message>
    <message>
        <source>&amp;Language</source>
        <translation type="vanished">语言(&amp;L)</translation>
    </message>
    <message>
        <source>Language Changed</source>
        <translation type="vanished">语言已更改</translation>
    </message>
    <message>
        <source>The language setting has been changed.
Please restart the application for the changes to take effect.</source>
        <translation type="vanished">语言设置已更改。
请重新启动应用程序以使更改生效。</translation>
    </message>
    <message>
        <source>Take &amp;Screenshot</source>
        <translation type="vanished">截图(&amp;S)</translation>
    </message>
    <message>
        <source>&amp;Properties...</source>
        <translation type="vanished">属性(&amp;P)...</translation>
    </message>
    <message>
        <source>Screenshot Failed</source>
        <translation>截图失败</translation>
    </message>
    <message>
        <source>Failed to capture screenshot. Make sure a scene is loaded.</source>
        <translation type="vanished">截图失败。请确保已加载场景。</translation>
    </message>
    <message>
        <source>Screenshot failed</source>
        <translation>截图失败</translation>
    </message>
    <message>
        <source>Failed to create screenshot directory:
%1</source>
        <translation type="vanished">无法创建截图目录：
%1</translation>
    </message>
    <message>
        <source>Failed to save EXR file:
%1</source>
        <translation type="vanished">无法保存 EXR 文件：
%1</translation>
    </message>
    <message>
        <source>Screenshot failed (EXR)</source>
        <translation>截图失败 (EXR)</translation>
    </message>
    <message>
        <source>Screenshot Warning</source>
        <translation>截图警告</translation>
    </message>
    <message>
        <source>EXR saved successfully, but PNG save failed:
%1</source>
        <translation type="vanished">EXR 保存成功，但 PNG 保存失败：
%1</translation>
    </message>
    <message>
        <source>Screenshot saved (EXR only): %1</source>
        <translation>截图已保存（仅 EXR）：%1</translation>
    </message>
    <message>
        <source>Screenshot saved: %1.{exr,png}</source>
        <translation>截图已保存：%1.{exr,png}</translation>
    </message>
    <message>
        <source>Screenshot Saved</source>
        <translation type="vanished">截图已保存</translation>
    </message>
    <message>
        <source>Screenshot saved successfully:

EXR: %1
PNG: %2</source>
        <translation type="vanished">截图保存成功：

EXR：%1
PNG：%2</translation>
    </message>
    <message>
        <source>Settings saved</source>
        <translation type="vanished">设置已保存</translation>
    </message>
</context>
<context>
    <name>MaterialEditorPanel</name>
    <message>
        <source>Material Properties</source>
        <translation type="vanished">材质属性</translation>
    </message>
    <message>
        <source>Material</source>
        <translation>材质</translation>
    </message>
    <message>
        <source>No material selected</source>
        <translation>未选中材质</translation>
    </message>
    <message>
        <source>Base Color</source>
        <translation>基础颜色</translation>
    </message>
    <message>
        <source>PBR Properties</source>
        <translation>PBR 属性</translation>
    </message>
    <message>
        <source>Emissive</source>
        <translation>自发光</translation>
    </message>
    <message>
        <source>IR Properties (Thermal)</source>
        <translation>红外属性（热辐射）</translation>
    </message>
    <message>
        <source>Fraction of blackbody radiation emitted (0 = reflective, 1 = perfect emitter)</source>
        <translation>相对黑体辐射的发射比例（0 为完全反射，1 为理想发射体）</translation>
    </message>
    <message>
        <source>Fraction of radiation transmitted through the material (0 = opaque)</source>
        <translation>透过材质的辐射比例（0 为不透明）</translation>
    </message>
    <message>
        <source>Object temperature:</source>
        <translation>物体温度：</translation>
    </message>
    <message>
        <source> K</source>
        <translation> K</translation>
    </message>
    <message>
        <source>Surface temperature of this material. 0 uses the scene ambient; roughly 293 K is room temperature and 310 K is human skin.</source>
        <translation>该材质的表面温度。填 0 表示使用场景环境温度；293 K 约为室温，310 K 约为人体皮肤温度。</translation>
    </message>
    <message>
        <source>Transmission and Volume</source>
        <translation>透射与体积</translation>
    </message>
    <message>
        <source>Transmission:</source>
        <translation>透射率：</translation>
    </message>
    <message>
        <source>How much light passes through rather than reflecting. 0 is opaque.</source>
        <translation>有多少光穿透而非反射。0 表示不透明。</translation>
    </message>
    <message>
        <source>Index of refraction: 1.0 air, 1.33 water, 1.5 glass, 2.4 diamond.</source>
        <translation>折射率：空气 1.0、水 1.33、玻璃 1.5、金刚石 2.4。</translation>
    </message>
    <message>
        <source>Dispersion:</source>
        <translation>色散：</translation>
    </message>
    <message>
        <source>Reciprocal Abbe number — how much the index varies with wavelength. 0 is no dispersion; this is what splits white light in a prism, and it is only visible in the spectral modes.</source>
        <translation>阿贝数的倒数——折射率随波长变化的程度。0 表示无色散；棱镜分解白光靠的就是它，且只在光谱模式下可见。</translation>
    </message>
    <message>
        <source>Attenuation distance:</source>
        <translation>衰减距离：</translation>
    </message>
    <message>
        <source>Distance inside the medium at which light reaches the attenuation colour (Beer-Lambert). 0 means no absorption at all.</source>
        <translation>光在介质内走到衰减颜色所需的距离（比尔-朗伯定律）。0 表示完全不吸收。</translation>
    </message>
    <message>
        <source>Attenuation colour:</source>
        <translation>衰减颜色：</translation>
    </message>
    <message>
        <source>Material %1</source>
        <translation>材质 %1</translation>
    </message>
    <message>
        <source>This material carries full spectral IR curves from the material generator. The three values below are a two-point summary of them; editing one replaces the curves.</source>
        <translation>该材质带有材质生成器产生的完整红外光谱曲线。下面三个数值只是这些曲线的两点概括；修改其中任意一项都会替换掉曲线。</translation>
    </message>
    <message>
        <source>Replace spectral curves?</source>
        <translation>是否替换光谱曲线？</translation>
    </message>
    <message>
        <source>This material&apos;s infrared response is stored as full spectral curves. Editing this value replaces them with two constant sample points.

Replace the curves?</source>
        <translation>该材质的红外响应以完整光谱曲线的形式保存。修改此数值会把它们替换为两个常数采样点。

是否替换这些曲线？</translation>
    </message>
    <message>
        <source>Warning: ε + τ &gt; 1 (violates energy conservation)</source>
        <translation>警告：ε + τ &gt; 1（违反能量守恒）</translation>
    </message>
    <message>
        <source>Reflectance ρ = %1</source>
        <translation>反射率 ρ = %1</translation>
    </message>
    <message>
        <source>Attenuation Colour</source>
        <translation>衰减颜色</translation>
    </message>
    <message>
        <source>Emissivity:</source>
        <translation>发射率：</translation>
    </message>
    <message>
        <source>Transmittance:</source>
        <translation>透过率：</translation>
    </message>
    <message>
        <source>Temperature:</source>
        <translation type="obsolete">温度:</translation>
    </message>
    <message>
        <source>Select Base Color</source>
        <translation>选择基础色</translation>
    </message>
    <message>
        <source>Set IR properties for thermal rendering</source>
        <translation>设置红外属性以进行热辐射渲染</translation>
    </message>
    <message>
        <source>Albedo:</source>
        <translation type="vanished">反照率:</translation>
    </message>
    <message>
        <source>PBR Parameters</source>
        <translation type="vanished">PBR 参数</translation>
    </message>
    <message>
        <source>Metallic:</source>
        <translation>金属度:</translation>
    </message>
    <message>
        <source>Roughness:</source>
        <translation>粗糙度:</translation>
    </message>
    <message>
        <source>IOR:</source>
        <translation>折射率:</translation>
    </message>
    <message>
        <source>Emission</source>
        <translation type="vanished">自发光</translation>
    </message>
    <message>
        <source>Color:</source>
        <translation type="vanished">颜色:</translation>
    </message>
    <message>
        <source>Strength:</source>
        <translation type="vanished">强度:</translation>
    </message>
    <message>
        <source>Spectral</source>
        <translation type="vanished">光谱</translation>
    </message>
    <message>
        <source>Spectral Curve:</source>
        <translation type="vanished">光谱曲线:</translation>
    </message>
    <message>
        <source>None</source>
        <translation type="vanished">无</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation type="vanished">重置</translation>
    </message>
</context>
<context>
    <name>PreferencesDialog</name>
    <message>
        <source>Preferences</source>
        <translation>首选项</translation>
    </message>
    <message>
        <source>Appearance</source>
        <translation>外观</translation>
    </message>
    <message>
        <source>Theme:</source>
        <translation>主题：</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <source>Interface language:</source>
        <translation>界面语言：</translation>
    </message>
    <message>
        <source>Screenshots and image export</source>
        <translation>截图与图像导出</translation>
    </message>
    <message>
        <source>Save location:</source>
        <translation>保存位置：</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>浏览...</translation>
    </message>
    <message>
        <source>Restore Defaults</source>
        <translation>恢复默认值</translation>
    </message>
    <message>
        <source>Screenshots are written as a matching pair, named YYYY-MM-DD_HH-MM-SS-mmm:
• EXR — the displayed image at full precision, including display enhancement
• PNG — an 8-bit sRGB preview
File → Export Image writes the unenhanced render instead.</source>
        <translation>截图成对写出，文件名格式为 YYYY-MM-DD_HH-MM-SS-mmm：
• EXR —— 视口所显示画面的全精度版本，含显示增强
• PNG —— 8 位 sRGB 预览图
「文件 → 导出图像」写出的则是未经增强的渲染结果。</translation>
    </message>
    <message>
        <source>Select screenshot save location</source>
        <translation>选择截图保存位置</translation>
    </message>
</context>
<context>
    <name>PropertiesPanel</name>
    <message>
        <source>Properties</source>
        <translation>属性</translation>
    </message>
    <message>
        <source>Nothing selected.
Pick a node or a material in the scene tree.</source>
        <translation>当前没有选中任何对象。
请在场景树中选择一个节点或材质。</translation>
    </message>
    <message>
        <source>Transform</source>
        <translation>变换</translation>
    </message>
    <message>
        <source>Position:</source>
        <translation>位置：</translation>
    </message>
    <message>
        <source>Rotation (°):</source>
        <translation>旋转（°）：</translation>
    </message>
    <message>
        <source>Scale:</source>
        <translation>缩放：</translation>
    </message>
    <message>
        <source>Edit Material</source>
        <translation>编辑材质</translation>
    </message>
    <message numerus="yes">
        <source>%n nodes selected. Drag in the viewport to transform them together.</source>
        <translation>
            <numerusform>已选中 %n 个节点。在视口中拖动可一并变换。</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>Quantiloom SDK mismatch</source>
        <translation>Quantiloom SDK 不匹配</translation>
    </message>
    <message>
        <source>Qt could not create a Vulkan device. The selected GPU is most likely missing the ray tracing extensions this renderer needs. On a laptop with both an integrated and a discrete GPU, check that Quantiloom is running on the discrete one.</source>
        <translation>Qt 无法创建 Vulkan 设备。所选 GPU 很可能不支持本渲染器所需的光线追踪扩展。若使用同时具备核显与独显的笔记本，请确认 Quantiloom 运行在独立显卡上。</translation>
    </message>
    <message>
        <source>The renderer could not start:

%1</source>
        <translation>渲染器无法启动：

%1</translation>
    </message>
    <message>
        <source>Render context not initialized</source>
        <translation>渲染上下文尚未初始化</translation>
    </message>
    <message>
        <source>Compiling shaders — first run may take a few minutes</source>
        <translation>正在编译着色器 —— 首次运行可能需要几分钟</translation>
    </message>
    <message>
        <source>Scene loaded successfully</source>
        <translation>场景加载成功</translation>
    </message>
    <message>
        <source>Failed to load scene: %1</source>
        <translation>场景加载失败：%1</translation>
    </message>
    <message>
        <source>Failed to load configuration: %1</source>
        <translation>加载配置失败：%1</translation>
    </message>
    <message>
        <source>The renderer is not ready yet.</source>
        <translation>渲染器尚未就绪。</translation>
    </message>
</context>
<context>
    <name>QuantiloomVulkanRenderer</name>
    <message>
        <source>Render context not initialized</source>
        <translation type="vanished">渲染上下文未初始化</translation>
    </message>
    <message>
        <source>Scene loaded successfully</source>
        <translation type="vanished">场景加载成功</translation>
    </message>
    <message>
        <source>Failed to load scene: %1</source>
        <translation type="vanished">场景加载失败: %1</translation>
    </message>
    <message>
        <source>Compiling Shaders</source>
        <translation type="vanished">编译着色器</translation>
    </message>
    <message>
        <source>First-time shader compilation in progress...
This may take a few minutes.</source>
        <translation type="vanished">首次着色器编译中...
这可能需要几分钟时间。</translation>
    </message>
    <message>
        <source>scale %1, bias %2</source>
        <translation>缩放 %1，偏移 %2</translation>
    </message>
    <message>
        <source>fractional part only — original value not recoverable</source>
        <translation>仅保留小数部分 —— 无法还原原始数值</translation>
    </message>
    <message>
        <source>hashed — original value not recoverable</source>
        <translation>已哈希 —— 无法还原原始数值</translation>
    </message>
    <message>
        <source>colour-mapped — read against the colour bar</source>
        <translation>已伪彩映射 —— 请对照色标读取</translation>
    </message>
</context>
<context>
    <name>RenderSettingsPanel</name>
    <message>
        <source>Status</source>
        <translation>状态</translation>
    </message>
    <message>
        <source>Quality</source>
        <translation>质量</translation>
    </message>
    <message>
        <source>Custom...</source>
        <translation>自定义...</translation>
    </message>
    <message>
        <source>Accumulate samples over multiple frames</source>
        <translation type="vanished">跨多帧累积采样</translation>
    </message>
    <message>
        <source>Preset:</source>
        <translation type="vanished">预设:</translation>
    </message>
    <message>
        <source>Render</source>
        <translation>渲染</translation>
    </message>
    <message>
        <source>Accumulated samples:</source>
        <translation>已累积采样数：</translation>
    </message>
    <message>
        <source>Target samples:</source>
        <translation>目标采样数：</translation>
    </message>
    <message>
        <source>Custom samples:</source>
        <translation>自定义采样数：</translation>
    </message>
    <message>
        <source>Progressive rendering</source>
        <translation type="vanished">渐进式渲染</translation>
    </message>
    <message>
        <source>Render resolution:</source>
        <translation>渲染分辨率：</translation>
    </message>
    <message>
        <source>Follows the viewport size.</source>
        <translation>跟随视口尺寸。</translation>
    </message>
    <message>
        <source>Display Enhancement</source>
        <translation>显示增强</translation>
    </message>
    <message>
        <source>Actions</source>
        <translation>操作</translation>
    </message>
    <message>
        <source>Reset Accumulation</source>
        <translation>重置累积</translation>
    </message>
    <message>
        <source>Clear accumulated samples and restart rendering</source>
        <translation>清空已累积的采样并重新开始渲染</translation>
    </message>
    <message>
        <source>Export Image...</source>
        <translation>导出图像...</translation>
    </message>
    <message>
        <source>Save the raw render, without display enhancement</source>
        <translation>保存原始渲染结果，不含显示增强</translation>
    </message>
    <message>
        <source>Export EXR when the target is reached</source>
        <translation>达到目标采样数时导出 EXR</translation>
    </message>
    <message>
        <source>Writes beside the open configuration, named after it and the sample count. Has no effect in Infinite mode, which has no target to reach.</source>
        <translation>写入到已打开配置的同目录，以配置名和采样数命名。无限模式没有目标采样数，此项不生效。</translation>
    </message>
    <message>
        <source>%1 x %2</source>
        <translation>%1 × %2</translation>
    </message>
    <message>
        <source>Export Image</source>
        <translation type="obsolete">导出图像</translation>
    </message>
    <message>
        <source>EXR Image (*.exr);;PNG Image (*.png);;All Files (*)</source>
        <translation type="obsolete">EXR 图像 (*.exr);;PNG 图像 (*.png);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Preview (4 SPP)</source>
        <translation type="vanished">预览 (4 SPP)</translation>
    </message>
    <message>
        <source>Draft (16 SPP)</source>
        <translation type="vanished">草稿 (16 SPP)</translation>
    </message>
    <message>
        <source>Medium (64 SPP)</source>
        <translation type="vanished">中等 (64 SPP)</translation>
    </message>
    <message>
        <source>High (256 SPP)</source>
        <translation type="vanished">高质量 (256 SPP)</translation>
    </message>
    <message>
        <source>Ultra (1024 SPP)</source>
        <translation type="vanished">极高 (1024 SPP)</translation>
    </message>
    <message>
        <source>Custom</source>
        <translation type="vanished">自定义</translation>
    </message>
    <message>
        <source>SPP:</source>
        <translation type="vanished">SPP:</translation>
    </message>
    <message>
        <source>Current:</source>
        <translation type="vanished">当前:</translation>
    </message>
    <message>
        <source>%1 / %2</source>
        <translation type="vanished">%1 / %2</translation>
    </message>
    <message>
        <source>Resolution</source>
        <translation>分辨率</translation>
    </message>
    <message>
        <source>720p (1280×720)</source>
        <translation type="vanished">720p (1280×720)</translation>
    </message>
    <message>
        <source>1080p (1920×1080)</source>
        <translation type="vanished">1080p (1920×1080)</translation>
    </message>
    <message>
        <source>1440p (2560×1440)</source>
        <translation type="vanished">1440p (2560×1440)</translation>
    </message>
    <message>
        <source>4K (3840×2160)</source>
        <translation type="vanished">4K (3840×2160)</translation>
    </message>
    <message>
        <source>Output</source>
        <translation type="vanished">输出</translation>
    </message>
    <message>
        <source>Export...</source>
        <translation type="vanished">导出...</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation type="vanished">重置</translation>
    </message>
    <message>
        <source>Progressive</source>
        <translation type="vanished">渐进式</translation>
    </message>
</context>
<context>
    <name>SceneTreePanel</name>
    <message>
        <source>Scene Hierarchy</source>
        <translation type="vanished">场景层级</translation>
    </message>
    <message>
        <source>No scene loaded</source>
        <translation type="vanished">未加载场景</translation>
    </message>
    <message>
        <source>Scene: %1</source>
        <translation type="vanished">场景: %1</translation>
    </message>
    <message>
        <source>Meshes</source>
        <translation>网格</translation>
    </message>
    <message>
        <source>Materials</source>
        <translation type="vanished">材质</translation>
    </message>
    <message>
        <source>Nodes</source>
        <translation type="vanished">节点</translation>
    </message>
    <message>
        <source>%1 (%2 triangles)</source>
        <translation type="vanished">%1 (%2 三角形)</translation>
    </message>
    <message>
        <source>Controls</source>
        <translation type="vanished">操作说明</translation>
    </message>
    <message>
        <source>&lt;b&gt;Selection:&lt;/b&gt; Click node above&lt;br&gt;&lt;b&gt;Transform:&lt;/b&gt; Select node, then Left-drag in viewport&lt;br&gt;&lt;b&gt;Mode:&lt;/b&gt; G=Move, R=Rotate, T=Scale&lt;br&gt;&lt;b&gt;Axis:&lt;/b&gt; X/Y/Z to constrain&lt;br&gt;&lt;b&gt;Camera:&lt;/b&gt; Right-drag=Orbit, Middle-drag=Pan, Wheel=Zoom&lt;br&gt;&lt;b&gt;Undo:&lt;/b&gt; Ctrl+Z / Ctrl+Y</source>
        <translation type="vanished">&lt;b&gt;选择:&lt;/b&gt; 点击上方节点&lt;br&gt;&lt;b&gt;变换:&lt;/b&gt; 选中节点后，在视口中左键拖拽&lt;br&gt;&lt;b&gt;模式:&lt;/b&gt; G=移动, R=旋转, T=缩放&lt;br&gt;&lt;b&gt;轴约束:&lt;/b&gt; X/Y/Z 限制轴向&lt;br&gt;&lt;b&gt;相机:&lt;/b&gt; 右键拖拽=环绕, 中键拖拽=平移, 滚轮=缩放&lt;br&gt;&lt;b&gt;撤销:&lt;/b&gt; Ctrl+Z / Ctrl+Y</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Scene</source>
        <translation>场景</translation>
    </message>
    <message>
        <source>Group</source>
        <translation>组</translation>
    </message>
    <message>
        <source>Nodes (%1)</source>
        <translation>节点 (%1)</translation>
    </message>
    <message>
        <source>Materials (%1)</source>
        <translation>材质 (%1)</translation>
    </message>
    <message>
        <source>Textures (%1)</source>
        <translation>纹理 (%1)</translation>
    </message>
    <message>
        <source>Node</source>
        <translation>节点</translation>
    </message>
    <message>
        <source>Filter by name...</source>
        <translation>按名称过滤……</translation>
    </message>
    <message>
        <source>Open a scene to see its contents.</source>
        <translation>打开场景后可在此查看其内容。</translation>
    </message>
    <message>
        <source>Node %1</source>
        <translation>节点 %1</translation>
    </message>
    <message>
        <source>Material %1</source>
        <translation>材质 %1</translation>
    </message>
    <message>
        <source>Material</source>
        <translation>材质</translation>
    </message>
    <message>
        <source>Texture %1</source>
        <translation>纹理 %1</translation>
    </message>
    <message>
        <source>Statistics</source>
        <translation>统计</translation>
    </message>
    <message>
        <source>Info</source>
        <translation>信息</translation>
    </message>
    <message>
        <source>Triangles</source>
        <translation>三角形</translation>
    </message>
    <message>
        <source>Vertices</source>
        <translation>顶点</translation>
    </message>
</context>
<context>
    <name>SdkGuard</name>
    <message>
        <source>%1 next to the executable is not the library this build was linked against.

Loaded:       %2
Built against: %3

The SDK&apos;s public types are source-compatible but not binary-compatible across updates, so continuing would be undefined behaviour with no visible error. Rebuild:

    cd /mnt/d/Quantiloom-Qt &amp;&amp; ./build_wsl.sh</source>
        <translation>可执行文件旁的 %1 并不是本次构建所链接的那个库。

已加载：      %2
构建时使用：  %3

SDK 的公开类型在版本之间保持源码兼容，但并不保持二进制兼容，继续运行属于未定义行为，且不会给出任何可见错误。请重新构建：

    cd /mnt/d/Quantiloom-Qt &amp;&amp; ./build_wsl.sh</translation>
    </message>
    <message>
        <source>The Quantiloom SDK at %1 has been reinstalled since this executable was built. This run uses the older library and will not reflect the current core.

Rebuild with:
    cd /mnt/d/Quantiloom-Qt &amp;&amp; ./build_wsl.sh</source>
        <translation>位于 %1 的 Quantiloom SDK 在本可执行文件构建之后被重新安装过。本次运行使用的是较旧的库，不会反映当前的核心实现。

请重新构建：
    cd /mnt/d/Quantiloom-Qt &amp;&amp; ./build_wsl.sh</translation>
    </message>
</context>
<context>
    <name>SensorPanel</name>
    <message>
        <source>Enable Sensor Simulation</source>
        <translation>启用传感器仿真</translation>
    </message>
    <message>
        <source>Optics</source>
        <translation>光学系统</translation>
    </message>
    <message>
        <source>Focal Length:</source>
        <translation>焦距:</translation>
    </message>
    <message>
        <source>Aperture:</source>
        <translation>光圈:</translation>
    </message>
    <message>
        <source>Detector</source>
        <translation>探测器</translation>
    </message>
    <message>
        <source>Sensor</source>
        <translation>传感器</translation>
    </message>
    <message>
        <source>Lens focal length. With the pixel pitch it sets the angular size of a pixel, and so how much of the scene one pixel averages.</source>
        <translation>镜头焦距。与像元间距共同决定单个像素的角尺寸，也就决定了一个像素平均了多大范围的场景。</translation>
    </message>
    <message>
        <source>f-number, focal length divided by entrance pupil diameter. Lower collects more light: irradiance on the detector goes as 1/f-number squared.</source>
        <translation>f 数，即焦距除以入瞳直径。数值越小进光越多：探测器上的辐照度与 f 数的平方成反比。</translation>
    </message>
    <message>
        <source>Pixel Pitch:</source>
        <translation>像元尺寸:</translation>
    </message>
    <message>
        <source>Centre-to-centre spacing of the detector elements. Sets how much area collects photons for one pixel.</source>
        <translation>探测器单元的中心间距。决定单个像素用多大面积收集光子。</translation>
    </message>
    <message>
        <source>Quantum Efficiency:</source>
        <translation>量子效率:</translation>
    </message>
    <message>
        <source>Fraction of arriving photons that become signal electrons. 1.0 would convert every photon.</source>
        <translation>入射光子转化为信号电子的比例。1.0 表示每个光子都被转换。</translation>
    </message>
    <message>
        <source>Well Capacity:</source>
        <translation>满阱容量:</translation>
    </message>
    <message>
        <source>Electrons a pixel can hold before it saturates. Anything brighter clips to white.</source>
        <translation>像素饱和前可容纳的电子数。更亮的部分将被截断为白色。</translation>
    </message>
    <message>
        <source>Bit Depth:</source>
        <translation>位深:</translation>
    </message>
    <message>
        <source>Bits per pixel out of the converter. Sets how finely the electron count is quantised.</source>
        <translation>模数转换输出的每像素位数。决定电子数被量化的精细程度。</translation>
    </message>
    <message>
        <source>Integration Time:</source>
        <translation>积分时间:</translation>
    </message>
    <message>
        <source>Temperature of the detector itself. Drives dark current, and for thermal bands the self-emission the optics see.</source>
        <translation>探测器自身的温度。决定暗电流；在热成像波段还决定光学系统看到的自发辐射。</translation>
    </message>
    <message>
        <source>ADC</source>
        <translation>模数转换</translation>
    </message>
    <message>
        <source>Gain:</source>
        <translation>增益:</translation>
    </message>
    <message>
        <source>Noise Model</source>
        <translation>噪声模型</translation>
    </message>
    <message>
        <source>How long the detector collects per frame. Longer gathers more signal and more dark current with it.</source>
        <translation>每帧的积分时长。时间越长收集的信号越多，暗电流也随之增加。</translation>
    </message>
    <message>
        <source>Electrons per digital number. Lower means finer steps, at the cost of clipping sooner.</source>
        <translation>每个数字量化单位对应的电子数。数值越小量化步长越细，但也更早截断。</translation>
    </message>
    <message>
        <source>Read Noise:</source>
        <translation>读出噪声:</translation>
    </message>
    <message>
        <source>Noise the readout electronics add per pixel, in electrons RMS. Independent of exposure -- it is what limits the darkest tones.</source>
        <translation>读出电路为每个像素引入的噪声，单位为电子均方根。与曝光无关，是暗部细节的极限所在。</translation>
    </message>
    <message>
        <source>Dark Current:</source>
        <translation>暗电流:</translation>
    </message>
    <message>
        <source>Photon Shot Noise (Poisson)</source>
        <translation>光子散粒噪声 (泊松)</translation>
    </message>
    <message>
        <source>Enable Read Noise</source>
        <translation>启用读出噪声</translation>
    </message>
    <message>
        <source>Enable Dark Current</source>
        <translation>启用暗电流</translation>
    </message>
    <message>
        <source>Fixed Pattern Noise (FPN)</source>
        <translation>固定图案噪声 (FPN)</translation>
    </message>
    <message>
        <source>FPN Parameters</source>
        <translation>FPN 参数</translation>
    </message>
    <message>
        <source>Electrons generated thermally per second with no light at all. Multiplied by the integration time, and roughly doubles every 7 K.</source>
        <translation>完全无光时每秒热激发产生的电子数。与积分时间相乘，温度每升高约 7 K 翻一倍。</translation>
    </message>
    <message>
        <source>PRNU Sigma:</source>
        <translation>PRNU 标准差:</translation>
    </message>
    <message>
        <source>Photo-Response Non-Uniformity: pixel-to-pixel spread in sensitivity, as a fraction. A fixed multiplicative pattern, visible in bright areas.</source>
        <translation>光响应非均匀性：像素间灵敏度的相对离散度。属于固定的乘性图案，在亮区可见。</translation>
    </message>
    <message>
        <source>DSNU Sigma:</source>
        <translation>DSNU 标准差:</translation>
    </message>
    <message>
        <source>Enable NUC</source>
        <translation>启用非均匀性校正</translation>
    </message>
    <message>
        <source>Dark Signal Non-Uniformity: pixel-to-pixel spread in dark current, in electrons. A fixed additive pattern, visible in dark areas.</source>
        <translation>暗信号非均匀性：像素间暗电流的离散度，单位为电子。属于固定的加性图案，在暗区可见。</translation>
    </message>
    <message>
        <source>NUC Efficiency:</source>
        <translation>NUC 校正效率:</translation>
    </message>
    <message>
        <source>IR Detector</source>
        <translation>红外探测器</translation>
    </message>
    <message>
        <source>How much of the fixed pattern the Non-Uniformity Correction removes. 1.0 removes all of it, which no real calibration does.</source>
        <translation>非均匀性校正能消除的固定图案比例。1.0 表示完全消除，实际标定无法做到。</translation>
    </message>
    <message>
        <source>Detector Temperature:</source>
        <translation>探测器温度:</translation>
    </message>
</context>
<context>
    <name>SettingsDialog</name>
    <message>
        <source>Settings</source>
        <translation type="vanished">设置</translation>
    </message>
    <message>
        <source>Screenshot Settings</source>
        <translation type="vanished">截图设置</translation>
    </message>
    <message>
        <source>Save Location:</source>
        <translation type="vanished">保存位置：</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation type="vanished">浏览...</translation>
    </message>
    <message>
        <source>Screenshots are saved as:
• EXR format (HDR, full precision)
• PNG format (8-bit, sRGB preview)
Filename: YYYY-MM-DD_HH-MM-SS-mmm</source>
        <translation type="vanished">截图保存格式：
• EXR 格式（HDR，全精度）
• PNG 格式（8位，sRGB 预览）
文件名：YYYY-MM-DD_HH-MM-SS-mmm</translation>
    </message>
    <message>
        <source>Restore Defaults</source>
        <translation type="vanished">恢复默认</translation>
    </message>
    <message>
        <source>Select Screenshot Save Location</source>
        <translation type="vanished">选择截图保存位置</translation>
    </message>
</context>
<context>
    <name>SpectralConfigPanel</name>
    <message>
        <source>Spectral Mode</source>
        <translation>光谱模式</translation>
    </message>
    <message>
        <source>Wavelength:</source>
        <translation>波长：</translation>
    </message>
    <message>
        <source>Spectral</source>
        <translation>光谱</translation>
    </message>
    <message>
        <source> nm</source>
        <translation> nm</translation>
    </message>
    <message>
        <source>Hyperspectral Range</source>
        <translation>高光谱范围</translation>
    </message>
    <message>
        <source>Min λ:</source>
        <translation>最小 λ：</translation>
    </message>
    <message>
        <source>Max λ:</source>
        <translation>最大 λ：</translation>
    </message>
    <message>
        <source>Δλ:</source>
        <translation>Δλ：</translation>
    </message>
    <message>
        <source>Bands:</source>
        <translation>波段数：</translation>
    </message>
    <message>
        <source>Preview mode: rendering from RGB-averaged spectral albedo.
Not suitable for quantitative analysis. Load measured spectral materials for accurate infrared work.</source>
        <translation>预览模式：使用 RGB 平均得到的光谱反照率渲染。
不适合定量分析。若需准确的红外结果，请载入实测光谱材质。</translation>
    </message>
    <message numerus="yes">
        <source>%n band(s)</source>
        <translation>
            <numerusform>%n 个波段</numerusform>
        </translation>
    </message>
    <message>
        <source>Mode:</source>
        <translation type="vanished">模式:</translation>
    </message>
    <message>
        <source>RGB Fused</source>
        <translation type="vanished">RGB 融合</translation>
    </message>
    <message>
        <source>Single Wavelength</source>
        <translation type="vanished">单波长</translation>
    </message>
    <message>
        <source>SWIR Fused</source>
        <translation type="vanished">SWIR 融合</translation>
    </message>
    <message>
        <source>MWIR Fused</source>
        <translation type="vanished">MWIR 融合</translation>
    </message>
    <message>
        <source>LWIR Fused</source>
        <translation type="vanished">LWIR 融合</translation>
    </message>
    <message>
        <source>Standard visible light rendering with spectral-to-RGB conversion.</source>
        <translation type="vanished">标准可见光渲染，光谱转 RGB。</translation>
    </message>
    <message>
        <source>Render at a single wavelength for monochromatic analysis.</source>
        <translation type="vanished">在单一波长下渲染，用于单色分析。</translation>
    </message>
    <message>
        <source>Short-wave infrared band (1.0-2.5 μm).</source>
        <translation type="vanished">短波红外波段 (1.0-2.5 μm)。</translation>
    </message>
    <message>
        <source>Mid-wave infrared band (3-5 μm).</source>
        <translation type="vanished">中波红外波段 (3-5 μm)。</translation>
    </message>
    <message>
        <source>Long-wave infrared band (8-14 μm).</source>
        <translation type="vanished">长波红外波段 (8-14 μm)。</translation>
    </message>
    <message>
        <source>Wavelength</source>
        <translation type="vanished">波长</translation>
    </message>
    <message>
        <source>Wavelength Range</source>
        <translation type="vanished">波长范围</translation>
    </message>
    <message>
        <source>Min (nm):</source>
        <translation type="vanished">最小 (nm):</translation>
    </message>
    <message>
        <source>Max (nm):</source>
        <translation type="vanished">最大 (nm):</translation>
    </message>
    <message>
        <source>Step (nm):</source>
        <translation type="vanished">步长 (nm):</translation>
    </message>
    <message>
        <source>Bands: %1</source>
        <translation type="vanished">波段数: %1</translation>
    </message>
</context>
<context>
    <name>SpectralLibraryPanel</name>
    <message>
        <source>%1
Database: %2
Coverage — VIS %3%, NIR %4%, SWIR %5%</source>
        <translation>%1
数据库：%2
覆盖度 — VIS %3%，NIR %4%，SWIR %5%</translation>
    </message>
    <message>
        <source>Material</source>
        <translation>材质</translation>
    </message>
    <message>
        <source>Database</source>
        <translation>数据库</translation>
    </message>
    <message>
        <source>Category</source>
        <translation>类别</translation>
    </message>
    <message>
        <source>Coverage</source>
        <translation>覆盖度</translation>
    </message>
    <message>
        <source>The spectral databases are installed but contain no entries.</source>
        <translation>光谱数据库已安装，但其中没有条目。</translation>
    </message>
    <message>
        <source>Could not read the spectral databases: %1</source>
        <translation>无法读取光谱数据库：%1</translation>
    </message>
    <message>
        <source>Spectral Library</source>
        <translation>光谱库</translation>
    </message>
    <message numerus="yes">
        <source>Search %n material(s)...</source>
        <translation>
            <numerusform>搜索 %n 种材质……</numerusform>
        </translation>
    </message>
    <message>
        <source>All databases</source>
        <translation>全部数据库</translation>
    </message>
    <message>
        <source>Reflectance preview</source>
        <translation>反射率预览</translation>
    </message>
    <message>
        <source>Band:</source>
        <translation>波段：</translation>
    </message>
    <message>
        <source>Reflectance</source>
        <translation>反射率</translation>
    </message>
    <message>
        <source>Select a material to preview its measured spectrum.</source>
        <translation>选择一个材质以预览其实测光谱。</translation>
    </message>
    <message>
        <source>Assign to Material</source>
        <translation>指派到材质</translation>
    </message>
    <message>
        <source>Replaces the material&apos;s colour with this measured spectrum, and any endmembers already bound. Undoable, and written to the configuration on save.</source>
        <translation>用这条实测光谱替换材质的颜色，以及已绑定的全部端元。可撤销，并在保存时写入配置。</translation>
    </message>
    <message>
        <source>Endmembers</source>
        <translation>端元</translation>
    </message>
    <message>
        <source>Add Endmember</source>
        <translation>添加端元</translation>
    </message>
    <message>
        <source>Adds this spectrum alongside the ones already bound. The surface then renders as a mixture of them, in proportions read out of its base-colour texture -- which is how a measured material keeps its texture instead of rendering as one flat reflectance.</source>
        <translation>在已绑定的光谱之外再加上这一条。表面随后渲染为它们的混合，各自的比例由基色贴图逐像素解算得出——实测材质正是这样保留住自己的纹理，而不是渲染成一片均一的反射率。</translation>
    </message>
    <message>
        <source>Remove Endmember</source>
        <translation>移除端元</translation>
    </message>
    <message>
        <source>%1. %2</source>
        <translation>%1. %2</translation>
    </message>
    <message>
        <source>None -- this material renders from its colour</source>
        <translation>无——该材质按自身颜色渲染</translation>
    </message>
    <message>
        <source>Replaces the material&apos;s colour with this measured spectrum. Undoable, and written to the configuration on save.</source>
        <translation type="vanished">用这条实测光谱替换该材质的颜色。可撤销，并在保存时写入配置。</translation>
    </message>
    <message>
        <source>Select a material in the scene to assign to.</source>
        <translation>请在场景中选择要指派的材质。</translation>
    </message>
    <message numerus="yes">
        <source>Assigning to: %1 (at the limit of %n endmember(s))</source>
        <translation>
            <numerusform>指派到：%1（已达 %n 个端元的上限）</numerusform>
        </translation>
    </message>
    <message>
        <source>Assigning to: %1</source>
        <translation>指派目标：%1</translation>
    </message>
    <message numerus="yes">
        <source>%n material(s) shown</source>
        <translation>
            <numerusform>显示 %n 种材质</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>SpectralMaterialGenPanel</name>
    <message>
        <source>Material Type</source>
        <translation>材质类型</translation>
    </message>
    <message>
        <source>Type:</source>
        <translation>类型:</translation>
    </message>
    <message>
        <source>Conductor</source>
        <translation>导体</translation>
    </message>
    <message>
        <source>Spectral Material Generator</source>
        <translation>光谱材质生成器</translation>
    </message>
    <message>
        <source>Auto IR Generation (SpectraForge)</source>
        <translation>自动红外生成（SpectraForge）</translation>
    </message>
    <message>
        <source>Current Material</source>
        <translation>当前材质</translation>
    </message>
    <message>
        <source>All Materials</source>
        <translation>全部材质</translation>
    </message>
    <message>
        <source>Mode:</source>
        <translation>方式：</translation>
    </message>
    <message>
        <source>K-means cluster count for texture color analysis</source>
        <translation>纹理颜色分析所用的 K-means 聚类数</translation>
    </message>
    <message>
        <source>Clusters (K):</source>
        <translation>聚类数 (K)：</translation>
    </message>
    <message>
        <source>Temperature:</source>
        <translation type="obsolete">温度:</translation>
    </message>
    <message>
        <source>Overwrite existing IR data</source>
        <translation>覆盖已有的红外数据</translation>
    </message>
    <message>
        <source>Generate IR Materials</source>
        <translation>生成红外材质</translation>
    </message>
    <message>
        <source>Results will appear here...</source>
        <translation>结果将显示在这里...</translation>
    </message>
    <message>
        <source>Dielectric</source>
        <translation>介质</translation>
    </message>
    <message>
        <source>Semiconductor</source>
        <translation>半导体</translation>
    </message>
    <message>
        <source>Roughness:</source>
        <translation>粗糙度:</translation>
    </message>
    <message>
        <source>Wavelength Range</source>
        <translation>波长范围</translation>
    </message>
    <message>
        <source>Start (nm):</source>
        <translation type="vanished">起始 (nm):</translation>
    </message>
    <message>
        <source>End (nm):</source>
        <translation type="vanished">终止 (nm):</translation>
    </message>
    <message>
        <source>Output Steps:</source>
        <translation>输出步数:</translation>
    </message>
    <message>
        <source> nm</source>
        <translation> nm</translation>
    </message>
    <message>
        <source>Start must be below End, and Output Steps at least 2, for a curve to be generated.</source>
        <translation>起始波长须小于终止波长，且输出步数不少于 2，才能生成曲线。</translation>
    </message>
    <message>
        <source>Anchor Points</source>
        <translation>锚点</translation>
    </message>
    <message>
        <source>Add Point</source>
        <translation>添加锚点</translation>
    </message>
    <message>
        <source>Remove Point</source>
        <translation>删除锚点</translation>
    </message>
    <message>
        <source>Load CSV</source>
        <translation>加载 CSV</translation>
    </message>
    <message>
        <source>Load YAML</source>
        <translation>加载 YAML</translation>
    </message>
    <message>
        <source>Interpolation:</source>
        <translation>插值方法:</translation>
    </message>
    <message>
        <source>Linear</source>
        <translation>线性</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>预览</translation>
    </message>
    <message>
        <source>Spectral Curves</source>
        <translation>光谱曲线</translation>
    </message>
    <message>
        <source>No scene loaded or no materials in scene.</source>
        <translation>尚未加载场景，或场景中没有材质。</translation>
    </message>
    <message>
        <source>No material selected. Select a material in the Scene Tree first.</source>
        <translation>尚未选择材质。请先在场景树中选择一种材质。</translation>
    </message>
    <message>
        <source>Material[%1] &apos;%2&apos; already has IR data, skipped.</source>
        <translation>材质[%1]“%2”已有红外数据，已跳过。</translation>
    </message>
    <message>
        <source>Material[%1] &apos;%2&apos; -&gt; IR generated (T=%3 K)</source>
        <translation>材质[%1]“%2” → 已生成红外数据（T=%3 K）</translation>
    </message>
    <message>
        <source>Failed to process material[%1]</source>
        <translation>处理材质[%1] 失败</translation>
    </message>
    <message>
        <source>Processed: %1/%2, Skipped: %3</source>
        <translation>已处理：%1/%2，已跳过：%3</translation>
    </message>
    <message>
        <source>Wavelength (nm)</source>
        <translation>波长 (nm)</translation>
    </message>
    <message>
        <source>Actions</source>
        <translation>操作</translation>
    </message>
    <message>
        <source> K</source>
        <translation> K</translation>
    </message>
    <message>
        <source>Start:</source>
        <translation>起始：</translation>
    </message>
    <message>
        <source>End:</source>
        <translation>终止：</translation>
    </message>
    <message>
        <source>Save CSV</source>
        <translation>保存 CSV</translation>
    </message>
    <message>
        <source>Target Material Index:</source>
        <translation>目标材质索引:</translation>
    </message>
    <message>
        <source>Apply to Material</source>
        <translation>应用到材质</translation>
    </message>
    <message>
        <source>Load Spectral CSV</source>
        <translation>加载光谱 CSV</translation>
    </message>
    <message>
        <source>CSV Files (*.csv);;All Files (*)</source>
        <translation>CSV 文件 (*.csv);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Cannot open file: %1</source>
        <translation>无法打开文件：%1</translation>
    </message>
    <message>
        <source>Save Spectral CSV</source>
        <translation>保存光谱 CSV</translation>
    </message>
    <message>
        <source>Cannot write file: %1</source>
        <translation>无法写入文件：%1</translation>
    </message>
    <message>
        <source>Load RefractiveIndex.info YAML</source>
        <translation>加载 RefractiveIndex.info YAML</translation>
    </message>
    <message>
        <source>YAML Files (*.yml *.yaml);;All Files (*)</source>
        <translation>YAML 文件 (*.yml *.yaml);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <source>Surface temperature assigned to the materials generated from each cluster</source>
        <translation>赋给各聚类所生成材质的表面温度</translation>
    </message>
    <message>
        <source>Cluster temperature:</source>
        <translation>聚类温度：</translation>
    </message>
    <message>
        <source>Info</source>
        <translation>提示</translation>
    </message>
    <message>
        <source>No data to save. Add anchor points first.</source>
        <translation>没有数据可保存，请先添加锚点。</translation>
    </message>
    <message>
        <source>Failed to load YAML: %1</source>
        <translation>加载 YAML 失败：%1</translation>
    </message>
    <message>
        <source>YAML file contains no spectral data.</source>
        <translation>该 YAML 文件不含光谱数据。</translation>
    </message>
    <message>
        <source>No interpolated data. Add anchor points first.</source>
        <translation>没有插值数据，请先添加锚点。</translation>
    </message>
</context>
<context>
    <name>ThemeManager</name>
    <message>
        <source>Blender Dark</source>
        <translation>Blender 深色</translation>
    </message>
    <message>
        <source>Classic</source>
        <translation>经典</translation>
    </message>
    <message>
        <source>Windows 11</source>
        <translation>Windows 11</translation>
    </message>
    <message>
        <source>Windows XP</source>
        <translation>Windows XP</translation>
    </message>
    <message>
        <source>Windows 7</source>
        <translation>Windows 7</translation>
    </message>
    <message>
        <source>Neutral Grey</source>
        <translation>中性灰</translation>
    </message>
    <message>
        <source>High Contrast</source>
        <translation>高对比度</translation>
    </message>
    <message>
        <source>Solarized Light</source>
        <translation>Solarized 浅色</translation>
    </message>
    <message>
        <source>Green Phosphor</source>
        <translation>绿色荧光</translation>
    </message>
    <message>
        <source>Print Friendly</source>
        <translation>打印友好</translation>
    </message>
</context>
<context>
    <name>TitleBar</name>
    <message>
        <source>Minimise</source>
        <translation>最小化</translation>
    </message>
    <message>
        <source>Restore Down</source>
        <translation>向下还原</translation>
    </message>
    <message>
        <source>Maximise</source>
        <translation>最大化</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
</context>
<context>
    <name>UndoStack</name>
    <message>
        <source>Undo</source>
        <translation>撤销</translation>
    </message>
    <message>
        <source>Undo %1</source>
        <translation>撤销 %1</translation>
    </message>
    <message>
        <source>Redo</source>
        <translation>重做</translation>
    </message>
    <message>
        <source>Redo %1</source>
        <translation>重做 %1</translation>
    </message>
</context>
<context>
    <name>ViewportFrame</name>
    <message>
        <source>Spectral: %1</source>
        <translation>光谱：%1</translation>
    </message>
    <message>
        <source>Debug: %1</source>
        <translation>调试：%1</translation>
    </message>
    <message>
        <source>Preview only — not quantitative</source>
        <translation>仅供预览 —— 非定量结果</translation>
    </message>
    <message>
        <source>This band renders from RGB-averaged spectral albedo. Load measured spectral materials for quantitative work.</source>
        <translation>该波段使用 RGB 平均得到的光谱反照率渲染。如需定量分析，请载入实测光谱材质。</translation>
    </message>
    <message>
        <source>No scene loaded</source>
        <translation>尚未加载场景</translation>
    </message>
    <message>
        <source>Open a glTF, USD or TOML scene to start rendering.
Right-drag orbits the camera, middle-drag pans, the wheel zooms; G/R/T switch transform mode once a node is selected.</source>
        <translation>打开 glTF、USD 或 TOML 场景即可开始渲染。
右键拖动环绕相机，中键拖动平移，滚轮缩放视距；选中节点后按 G/R/T 切换变换模式。</translation>
    </message>
    <message>
        <source>Open Scene...</source>
        <translation>打开场景...</translation>
    </message>
    <message>
        <source>Recent scenes</source>
        <translation>最近使用的场景</translation>
    </message>
</context>
<context>
    <name>WorkspaceManager</name>
    <message>
        <source>Layout</source>
        <translation>布景</translation>
    </message>
    <message>
        <source>Environment &amp;&amp; Spectral</source>
        <translation>环境与光谱</translation>
    </message>
    <message>
        <source>Material Prep</source>
        <translation>材质制备</translation>
    </message>
    <message>
        <source>Debug</source>
        <translation>调试</translation>
    </message>
</context>
<context>
    <name>catalog</name>
    <message>
        <source>Geometry</source>
        <translation>几何</translation>
    </message>
    <message>
        <source>Material</source>
        <translation>材质</translation>
    </message>
    <message>
        <source>Lighting</source>
        <translation>光照</translation>
    </message>
    <message>
        <source>BRDF</source>
        <translation>BRDF</translation>
    </message>
    <message>
        <source>IBL</source>
        <translation>IBL</translation>
    </message>
    <message>
        <source>Spectral</source>
        <translation>光谱</translation>
    </message>
    <message>
        <source>Infrared</source>
        <translation>红外</translation>
    </message>
    <message>
        <source>Geometry Diagnostics</source>
        <translation>几何诊断</translation>
    </message>
    <message>
        <source>None (Normal Rendering)</source>
        <translation>无（正常渲染）</translation>
    </message>
    <message>
        <source>World Position</source>
        <translation>世界坐标位置</translation>
    </message>
    <message>
        <source>Geometric Normal</source>
        <translation>几何法线</translation>
    </message>
    <message>
        <source>Shaded Normal</source>
        <translation>着色法线</translation>
    </message>
    <message>
        <source>Tangent</source>
        <translation>切线</translation>
    </message>
    <message>
        <source>UV Coordinates</source>
        <translation>UV 坐标</translation>
    </message>
    <message>
        <source>Material ID</source>
        <translation>材质 ID</translation>
    </message>
    <message>
        <source>Triangle ID</source>
        <translation>三角形 ID</translation>
    </message>
    <message>
        <source>Barycentric Coords</source>
        <translation>重心坐标</translation>
    </message>
    <message>
        <source>Base Color (Albedo)</source>
        <translation>基础色（反照率）</translation>
    </message>
    <message>
        <source>Metallic</source>
        <translation>金属度</translation>
    </message>
    <message>
        <source>Roughness</source>
        <translation>粗糙度</translation>
    </message>
    <message>
        <source>Normal Map Delta</source>
        <translation>法线贴图偏移</translation>
    </message>
    <message>
        <source>Emissive</source>
        <translation>自发光</translation>
    </message>
    <message>
        <source>Alpha</source>
        <translation>不透明度</translation>
    </message>
    <message>
        <source>N dot L</source>
        <translation>N·L</translation>
    </message>
    <message>
        <source>N dot V</source>
        <translation>N·V</translation>
    </message>
    <message>
        <source>Direct Sun</source>
        <translation>直射太阳光</translation>
    </message>
    <message>
        <source>Diffuse</source>
        <translation>漫反射</translation>
    </message>
    <message>
        <source>Atmospheric Transmittance</source>
        <translation>大气透过率</translation>
    </message>
    <message>
        <source>Fresnel F0</source>
        <translation>菲涅尔 F0</translation>
    </message>
    <message>
        <source>Fresnel</source>
        <translation>菲涅尔</translation>
    </message>
    <message>
        <source>Full BRDF</source>
        <translation>完整 BRDF</translation>
    </message>
    <message>
        <source>Specular D (GGX)</source>
        <translation>高光 D 项（GGX）</translation>
    </message>
    <message>
        <source>Specular G (Smith)</source>
        <translation>高光 G 项（Smith）</translation>
    </message>
    <message>
        <source>Reflection Direction</source>
        <translation>反射方向</translation>
    </message>
    <message>
        <source>Prefiltered Environment</source>
        <translation>预滤波环境贴图</translation>
    </message>
    <message>
        <source>BRDF LUT</source>
        <translation>BRDF 查找表</translation>
    </message>
    <message>
        <source>IBL Specular</source>
        <translation>IBL 高光</translation>
    </message>
    <message>
        <source>Sky Ambient</source>
        <translation>天空环境光</translation>
    </message>
    <message>
        <source>XYZ Tristimulus</source>
        <translation>XYZ 三刺激值</translation>
    </message>
    <message>
        <source>Before Chroma Correction</source>
        <translation>色度校正前</translation>
    </message>
    <message>
        <source>Spectral Reflectance @550nm</source>
        <translation>550 nm 处光谱反射率</translation>
    </message>
    <message>
        <source>Surface Temperature</source>
        <translation>表面温度</translation>
    </message>
    <message>
        <source>IR Emissivity</source>
        <translation>红外发射率</translation>
    </message>
    <message>
        <source>IR Emission</source>
        <translation>红外自发辐射</translation>
    </message>
    <message>
        <source>IR Reflection</source>
        <translation>红外反射</translation>
    </message>
    <message>
        <source>Vertex Positions (Hash)</source>
        <translation>顶点位置（哈希）</translation>
    </message>
    <message>
        <source>Index Values</source>
        <translation>索引值</translation>
    </message>
    <message>
        <source>Instance ID</source>
        <translation>实例 ID</translation>
    </message>
    <message>
        <source>Primitive ID</source>
        <translation>图元 ID</translation>
    </message>
    <message>
        <source>Index Buffer Position</source>
        <translation>索引缓冲区位置</translation>
    </message>
    <message>
        <source>V0 Position</source>
        <translation>V0 位置</translation>
    </message>
    <message>
        <source>Raw idx0</source>
        <translation>原始 idx0</translation>
    </message>
    <message>
        <source>V0 Raw (clamped)</source>
        <translation>V0 原始值（钳制）</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <source>Standard rendering output. No debug visualization.</source>
        <translation>常规渲染输出，不做调试可视化。</translation>
    </message>
    <message>
        <source>World-space hit position. RGB = fractional XYZ coordinates.</source>
        <translation>世界空间命中点位置。RGB 对应 XYZ 坐标的小数部分。</translation>
    </message>
    <message>
        <source>Raw geometric normal from triangle vertices (before normal mapping).</source>
        <translation>由三角形顶点得到的原始几何法线（未叠加法线贴图）。</translation>
    </message>
    <message>
        <source>Final shading normal after interpolation and normal map application.</source>
        <translation>插值并叠加法线贴图之后的最终着色法线。</translation>
    </message>
    <message>
        <source>Tangent vector for normal mapping. Used for TBN matrix construction.</source>
        <translation>用于法线贴图的切线向量，构建 TBN 矩阵时使用。</translation>
    </message>
    <message>
        <source>Texture coordinates. RG = fractional UV, useful for texture mapping debug.</source>
        <translation>纹理坐标。RG 为 UV 的小数部分，便于排查贴图映射问题。</translation>
    </message>
    <message>
        <source>Material index visualized as distinct colors. Each material gets unique color.</source>
        <translation>把材质索引映射为不同颜色，每种材质对应一种颜色。</translation>
    </message>
    <message>
        <source>Primitive (triangle) index. Useful for mesh topology inspection.</source>
        <translation>图元（三角形）索引，便于检查网格拓扑。</translation>
    </message>
    <message>
        <source>Barycentric coordinates within triangle. RGB = weights at 3 vertices.</source>
        <translation>三角形内的重心坐标，RGB 对应三个顶点的权重。</translation>
    </message>
    <message>
        <source>Albedo/base color from texture or material parameters.</source>
        <translation>来自贴图或材质参数的反照率／基础色。</translation>
    </message>
    <message>
        <source>Metallic parameter. 0 = dielectric, 1 = metal.</source>
        <translation>金属度参数。0 为电介质，1 为金属。</translation>
    </message>
    <message>
        <source>Roughness parameter. 0 = mirror smooth, 1 = fully rough.</source>
        <translation>粗糙度参数。0 为镜面光滑，1 为完全粗糙。</translation>
    </message>
    <message>
        <source>Normal map perturbation from surface normal.</source>
        <translation>法线贴图相对表面法线的扰动量。</translation>
    </message>
    <message>
        <source>Emissive color/intensity. Self-illumination without external lighting.</source>
        <translation>自发光颜色／强度，不依赖外部光照。</translation>
    </message>
    <message>
        <source>Alpha/opacity value. 1 = opaque, 0 = transparent.</source>
        <translation>不透明度值。1 为不透明，0 为全透明。</translation>
    </message>
    <message>
        <source>Dot product of normal and light direction. Basic diffuse term.</source>
        <translation>法线与光照方向的点积，即基础漫反射项。</translation>
    </message>
    <message>
        <source>Dot product of normal and view direction. Affects Fresnel and specular.</source>
        <translation>法线与视线方向的点积，影响菲涅尔与高光。</translation>
    </message>
    <message>
        <source>Direct sunlight contribution after shadowing and attenuation.</source>
        <translation>经过阴影与衰减之后的太阳直射光贡献。</translation>
    </message>
    <message>
        <source>Diffuse lighting term: kD * albedo * NdotL.</source>
        <translation>漫反射光照项：kD × 反照率 × N·L。</translation>
    </message>
    <message>
        <source>Atmospheric transmittance factor from scattering/absorption LUT.</source>
        <translation>取自散射／吸收查找表的大气透过率因子。</translation>
    </message>
    <message>
        <source>Base reflectivity at normal incidence. Depends on metallic and IOR.</source>
        <translation>垂直入射时的基础反射率，取决于金属度与折射率。</translation>
    </message>
    <message>
        <source>Fresnel reflectance at current viewing angle (Schlick approximation).</source>
        <translation>当前视角下的菲涅尔反射率（Schlick 近似）。</translation>
    </message>
    <message>
        <source>Complete Cook-Torrance BRDF evaluation: D * G * F / (4 * NdotL * NdotV).</source>
        <translation>完整的 Cook-Torrance BRDF 求值：D × G × F ÷ (4 × N·L × N·V)。</translation>
    </message>
    <message>
        <source>GGX/Trowbridge-Reitz normal distribution function.</source>
        <translation>GGX／Trowbridge-Reitz 法线分布函数。</translation>
    </message>
    <message>
        <source>Smith geometry/masking-shadowing function.</source>
        <translation>Smith 几何遮蔽－阴影函数。</translation>
    </message>
    <message>
        <source>Mirror reflection direction for environment map sampling.</source>
        <translation>用于采样环境贴图的镜面反射方向。</translation>
    </message>
    <message>
        <source>Pre-filtered environment map sample at current roughness level.</source>
        <translation>当前粗糙度下的预滤波环境贴图采样值。</translation>
    </message>
    <message>
        <source>BRDF integration LUT sample. RG = scale and bias for split-sum.</source>
        <translation>BRDF 积分查找表采样值，RG 为分裂求和法的缩放与偏移。</translation>
    </message>
    <message>
        <source>Final IBL specular contribution: prefiltered * (F * scale + bias).</source>
        <translation>最终的 IBL 高光贡献：预滤波值 × (F × 缩放 + 偏移)。</translation>
    </message>
    <message>
        <source>Ambient sky lighting contribution (diffuse IBL).</source>
        <translation>天空环境光贡献（漫反射 IBL）。</translation>
    </message>
    <message>
        <source>CIE XYZ tristimulus values from spectral integration. Before RGB conversion.</source>
        <translation>光谱积分得到的 CIE XYZ 三刺激值，尚未转换为 RGB。</translation>
    </message>
    <message>
        <source>Linear RGB before chromaticity correction. May show color shifts.</source>
        <translation>色度校正之前的线性 RGB，可能存在色偏。</translation>
    </message>
    <message>
        <source>Material spectral reflectance sampled at 550 nm (green reference).</source>
        <translation>材质在 550 nm（绿光参考波长）处的光谱反射率。</translation>
    </message>
    <message>
        <source>Surface temperature in Kelvin. Blue = cold, red = hot (colormap).</source>
        <translation>以开尔文为单位的表面温度。蓝色偏冷，红色偏热（伪彩映射）。</translation>
    </message>
    <message>
        <source>IR emissivity factor. 1 = perfect blackbody, 0 = perfect reflector.</source>
        <translation>红外发射率。1 为理想黑体，0 为理想反射体。</translation>
    </message>
    <message>
        <source>Thermal emission contribution: emissivity * Planck(T, lambda).</source>
        <translation>热辐射贡献：发射率 × 普朗克函数 Planck(T, λ)。</translation>
    </message>
    <message>
        <source>IR reflection of ambient thermal radiation.</source>
        <translation>对环境热辐射的红外反射。</translation>
    </message>
    <message>
        <source>Hash of 3 vertex positions. The same face should show similar colors. Different colors on one face mean index corruption.</source>
        <translation>三个顶点位置的哈希值。同一面应显示相近的颜色；同一面出现不同颜色说明索引已损坏。</translation>
    </message>
    <message>
        <source>Triangle vertex indices as RGB (normalized by 32). For a cube: idx 0-23.</source>
        <translation>以 RGB 表示的三角形顶点索引（除以 32 归一化）。立方体为 idx 0–23。</translation>
    </message>
    <message>
        <source>TLAS instance index. Verifies instance-to-geometry mapping.</source>
        <translation>TLAS 实例索引，用于验证实例到几何体的映射。</translation>
    </message>
    <message>
        <source>PrimitiveIndex() value. R = id/12 (gradient), G = alternating, B = even/odd. For a cube: 12 distinct triangles with a smooth R gradient.</source>
        <translation>PrimitiveIndex() 的取值。R = id/12（渐变），G 交替，B 表示奇偶。立方体应显示 12 个三角形，R 通道平滑渐变。</translation>
    </message>
    <message>
        <source>Index buffer read position. R = basePos/36, G = offset/36, B = primID/12. For a single BLAS, G should be 0.</source>
        <translation>索引缓冲区读取位置。R = basePos/36，G = offset/36，B = primID/12。只有一个 BLAS 时 G 应为 0。</translation>
    </message>
    <message>
        <source>First vertex (v0) position mapped to 0-1 using frac(). For a ±1 cube: 0 for both +1 and -1, 0.5 for 0.</source>
        <translation>用 frac() 把首顶点 v0 的位置映射到 0–1。±1 立方体上 +1 与 −1 都显示为 0，0 显示为 0.5。</translation>
    </message>
    <message>
        <source>Raw idx0 value. R = idx0/32, G = readAddr/32, B = offset/32. For a cube: R should be 0-0.72 (idx 0-23). G = R when offset is 0.</source>
        <translation>原始 idx0 取值。R = idx0/32，G = readAddr/32，B = offset/32。立方体上 R 应在 0–0.72（idx 0–23）之间；offset 为 0 时 G 等于 R。</translation>
    </message>
    <message>
        <source>v0 position clamped (not frac). -1 maps to 0, 0 to 0.5, +1 to 1. For a cube: only 0 or 1, never 0.5.</source>
        <translation>对 v0 位置做钳制（而非取小数）。−1 映射为 0，0 映射为 0.5，+1 映射为 1。立方体上只应出现 0 或 1，不应出现 0.5。</translation>
    </message>
    <message>
        <source>Unknown debug mode.</source>
        <translation>未知的调试模式。</translation>
    </message>
    <message>
        <source>&lt;h3&gt;Reading a debug image&lt;/h3&gt;&lt;p&gt;&lt;b&gt;Color encoding&lt;/b&gt;&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;Vectors&lt;/b&gt; — (V+1)/2 maps the [-1,1] range onto [0,1] RGB.&lt;/li&gt;&lt;li&gt;&lt;b&gt;Scalars&lt;/b&gt; — grayscale intensity.&lt;/li&gt;&lt;li&gt;&lt;b&gt;Identifiers&lt;/b&gt; — hashed to distinct colors.&lt;/li&gt;&lt;li&gt;&lt;b&gt;Temperature&lt;/b&gt; — blue (cold) through red (hot).&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;&lt;b&gt;Where to start&lt;/b&gt;&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;Shaded Normal&lt;/b&gt; — check that normal mapping is applied.&lt;/li&gt;&lt;li&gt;&lt;b&gt;Material ID&lt;/b&gt; — verify which material each surface resolved to.&lt;/li&gt;&lt;li&gt;&lt;b&gt;XYZ Tristimulus&lt;/b&gt; — debug spectral integration before RGB conversion.&lt;/li&gt;&lt;li&gt;&lt;b&gt;Geometry Diagnostics&lt;/b&gt; — only useful when a mesh renders as noise; they expose index and vertex buffer addressing directly.&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;Hover the viewport with a debug mode active to read the raw value under the cursor in the status bar.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;如何解读调试图像&lt;/h3&gt;&lt;p&gt;&lt;b&gt;颜色编码&lt;/b&gt;&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;向量&lt;/b&gt;——按 (V+1)/2 把 [−1,1] 映射到 [0,1] 的 RGB。&lt;/li&gt;&lt;li&gt;&lt;b&gt;标量&lt;/b&gt;——灰度亮度。&lt;/li&gt;&lt;li&gt;&lt;b&gt;标识符&lt;/b&gt;——哈希为互不相同的颜色。&lt;/li&gt;&lt;li&gt;&lt;b&gt;温度&lt;/b&gt;——由蓝（冷）过渡到红（热）。&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;&lt;b&gt;从哪里入手&lt;/b&gt;&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;着色法线&lt;/b&gt;——确认法线贴图是否生效。&lt;/li&gt;&lt;li&gt;&lt;b&gt;材质 ID&lt;/b&gt;——确认每个表面实际解析到哪种材质。&lt;/li&gt;&lt;li&gt;&lt;b&gt;XYZ 三刺激值&lt;/b&gt;——在转换为 RGB 之前排查光谱积分。&lt;/li&gt;&lt;li&gt;&lt;b&gt;几何诊断&lt;/b&gt;——只有当网格渲染成噪点时才有用，它们直接暴露索引与顶点缓冲区的寻址情况。&lt;/li&gt;&lt;/ul&gt;&lt;p&gt;启用调试模式后把光标悬停在视口上，状态栏会显示光标处的原始数值。&lt;/p&gt;</translation>
    </message>
    <message>
        <source>%1-%2 μm</source>
        <translation>%1–%2 μm</translation>
    </message>
    <message>
        <source>%1-%2 nm</source>
        <translation>%1–%2 nm</translation>
    </message>
    <message>
        <source>Single Wavelength</source>
        <translation>单波长</translation>
    </message>
    <message>
        <source>RGB (Default)</source>
        <translation>RGB（默认）</translation>
    </message>
    <message>
        <source>VIS Fused (32-band Spectral)</source>
        <translation>可见光融合（32 波段光谱）</translation>
    </message>
    <message>
        <source>%1 (%2)</source>
        <translation>%1（%2）</translation>
    </message>
    <message>
        <source>Fast RGB rendering, no spectral integration. Best for real-time preview.</source>
        <translation>快速 RGB 渲染，不做光谱积分，适合实时预览。</translation>
    </message>
    <message>
        <source>32-wavelength spectral integration with CIE XYZ color matching. Physically accurate but slower.</source>
        <translation>32 个波长的光谱积分，配合 CIE XYZ 配色函数。物理更准确，但更慢。</translation>
    </message>
    <message>
        <source>Monochromatic rendering at a single wavelength. Useful for spectral analysis and wavelength-specific effects.</source>
        <translation>在单一波长下做单色渲染，适合光谱分析与波长相关效应。</translation>
    </message>
    <message>
        <source>Near-Infrared (%1). Reflected solar radiation, vegetation analysis, and night vision.</source>
        <translation>近红外（%1）。用于反射太阳辐射、植被分析与夜视。</translation>
    </message>
    <message>
        <source>Short-Wave Infrared (%1). Moisture detection, material identification, and imaging through haze.</source>
        <translation>短波红外（%1）。用于水分探测、材料识别与透雾成像。</translation>
    </message>
    <message>
        <source>Mid-Wave Infrared (%1). Thermal imaging for hot objects, engine exhaust, and fire detection.</source>
        <translation>中波红外（%1）。用于高温目标、发动机尾焰与火情探测的热成像。</translation>
    </message>
    <message>
        <source>Long-Wave Infrared (%1). Thermal imaging for room-temperature objects, people, and buildings.</source>
        <translation>长波红外（%1）。用于常温物体、人体与建筑的热成像。</translation>
    </message>
    <message>
        <source>Unknown spectral mode.</source>
        <translation>未知的光谱模式。</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>预览</translation>
    </message>
    <message>
        <source>Draft</source>
        <translation>草稿</translation>
    </message>
    <message>
        <source>Medium</source>
        <translation>中等</translation>
    </message>
    <message>
        <source>High</source>
        <translation>高</translation>
    </message>
    <message>
        <source>Very High</source>
        <translation>很高</translation>
    </message>
    <message>
        <source>Production</source>
        <translation>成品</translation>
    </message>
    <message>
        <source>Infinite (∞)</source>
        <translation>无限 (∞)</translation>
    </message>
    <message>
        <source>%1 (%2 spp)</source>
        <translation>%1（%2 采样／像素）</translation>
    </message>
</context>
<context>
    <name>uiplot::SpectrumPlotWidget</name>
    <message>
        <source>Wavelength (nm)</source>
        <translation>波长 (nm)</translation>
    </message>
</context>
</TS>
