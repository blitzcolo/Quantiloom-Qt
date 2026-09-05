# src/i18n/

`translations/quantiloom_en.ts` and `quantiloom_zh_CN.ts`, compiled to `.qm` by
`qt6_add_translation` during the build. There is no manual `lrelease` step.
`LanguageManager` swaps the installed translator at runtime, so changing language never
requires a restart.

## Regenerating after adding or changing a tr() string

From the repo root:

```bash
PATH="/mnt/c/Qt/6.10.1/msvc2022_64/bin:$PATH" \
  lupdate.exe src -locations none \
  -ts src/i18n/translations/quantiloom_zh_CN.ts src/i18n/translations/quantiloom_en.ts
```

**`-locations none` is not optional.** These files carry no `<location>` elements; a
plain `lupdate` adds one per message — hundreds of lines per file — burying the real
change under source-line coordinates that go stale on the next edit.

Hand-edit the text inside `<translation>`; leave the surrounding XML to `lupdate`.

## Zero backlog is the policy

`quantiloom_zh_CN.ts` has **no unfinished messages**. Adding or changing a user-visible
string means updating the `.ts` files and writing the Chinese **in the same commit**.
This repository has no CI, so the rule is kept by review; the check is one command:

```bash
grep -c 'type="unfinished"' src/i18n/translations/quantiloom_zh_CN.ts   # expect 0
```

`quantiloom_en.ts` is deliberately left unfinished throughout: the sources are English,
and Qt falls back to the source text for any message a translator does not cover. The
file exists so that selecting English installs a translator that overrides Chinese
rather than leaving it in place.

The `type="vanished"` entries are retained on purpose so their Chinese is not lost if a
string comes back. Do not delete them.

## Watch out for strings lupdate cannot see

`lupdate` reads literals at the call site. Two shapes silently produce no `.ts` entry:

- a helper that forwards a `const char*` to `QCoreApplication::translate()` — this is
  how every debug and spectral mode name was untranslatable while looking translated.
  `src/ui/ModeCatalog.cpp` uses a `Q_DECLARE_TR_FUNCTIONS(catalog)` class so the calls
  are plain `tr("literal")`;
- `tr(variable)` for text stored elsewhere — mark the literal with `QT_TR_NOOP` where it
  is written down, as `SensorPanel`'s form captions do.

After a `lupdate` run, check the message count moved by as much as you expected.

## Glossary

Translate consistently. Left column is the source term, right is the Chinese to use.

| English | 中文 |
|---|---|
| spectral | 光谱 |
| band | 波段 |
| wavelength | 波长 |
| samples per pixel / target samples | 采样数 / 目标采样数 |
| accumulation | 累积 |
| emissivity | 发射率 |
| transmittance | 透过率 |
| reflectance | 反射率 |
| albedo | 反照率 |
| roughness / metallic | 粗糙度 / 金属度 |
| viewport | 视口 |
| panel / dock | 面板 |
| workspace | 工作区 |
| layout | 布局 |
| node | 节点 |
| material | 材质 |
| preset | 预设 |
| sensor / detector | 传感器 / 探测器 |
| atmosphere | 大气 |
| resolution | 分辨率 |
| gizmo | 变换手柄 |
| display enhancement | 显示增强 |
| debug visualization | 调试可视化 |
| screenshot / export image | 截图 / 导出图像 |
| timeline | 时间轴 |
| tick | 时间刻 |
| keyframe | 关键帧 |
| playback | 播放 |
| scrub (the timeline) | 拖动时间轴 |
| transport (play/step controls) | 走带 |
| thermal geometry epoch | 热学几何分段 |
| rest pose | 静止姿态 |
| model (a `[[models]]` entry) | 模型 |
| trajectory / motion | 轨迹 / 运动 |

Temperatures always carry their qualifier: 大气温度, 地表温度, 物体温度, 探测器温度,
聚类温度. One bare "temperature" for five different quantities is what the qualifiers
replaced.

## Kept in Latin script by decision, not by omission

These are the conventional written form in Chinese technical usage, so they pass through
`tr()` and are translated as themselves — or are `QStringLiteral` with a comment saying
why:

- band acronyms: RGB, VIS, NIR, SWIR, MWIR, LWIR;
- symbols: n, k, R0, ε, τ, ρ, λ, Δλ, N·L, N·V;
- channel letters R:/G:/B:;
- unit suffixes: K, nm, μm, km, mm/h, hPa, W/m²/sr, px, spp, °;
- key names in the shortcut reference: Shift, Space, W/A/S/D, G, R, T, X/Y/Z;
- API and format names: BRDF, IBL, TLAS, BLAS, GGX, Smith, CLAHE, MODTRAN, EXR, PNG,
  TOML, glTF, USD, safetensors.

Language names in the preferences dialog are written in their own language (English,
中文), which is why they are not translated either.

## Runtime switching: what a new panel owes

Nothing user-visible may be set once in a constructor. Use `PanelBase::bindText()` or
override `retranslateUi()`. Two traps:

- text built from values — a formatted reading, a "%1 bands" label — has to be
  recomputed on a language change, not merely re-labelled;
- when refilling a combo box, restore the selection **by value**, never by index.
  Otherwise switching language quietly changes the user's spectral mode.

## Commits

**No Claude Code session link in a commit message.** No `Claude-Session:` trailer,
no `https://claude.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
