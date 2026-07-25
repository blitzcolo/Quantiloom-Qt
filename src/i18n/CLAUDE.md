# src/i18n/

`translations/quantiloom_en.ts` and `quantiloom_zh_CN.ts`, compiled to `.qm` by
`qt6_add_translation` during the build. There is no manual `lrelease` step.

## Regenerating after adding or changing a tr() string

From the repo root:

```bash
PATH="/mnt/c/Qt/6.10.1/msvc2022_64/bin:$PATH" \
  lupdate.exe src -locations none \
  -ts src/i18n/translations/quantiloom_zh_CN.ts src/i18n/translations/quantiloom_en.ts
```

**`-locations none` is not optional.** These files carry no `<location>` elements; a
plain `lupdate` adds one per message — 544 lines per file — burying the real change
under source-line coordinates that go stale on the next edit.

Hand-edit the text inside `<translation>`; leave the surrounding XML to `lupdate`.

## The backlog is pre-existing

277 of 544 messages are `type="unfinished"`, 262 of them with an empty translation, so
much of the Chinese UI falls back to English. That predates any current change — not a
regression to fix in passing. 56 `type="vanished"` entries are retained deliberately so
their Chinese text is not lost.
