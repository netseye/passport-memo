[简体中文](AGENTS.zh_CN.md) · **English**

# Contributor instructions for agents

Read `docs/architecture.md` before code changes, `docs/build.md` before build/CI/partition changes, and the relevant BSP header before hardware changes. Check Git status first and preserve unrelated work.

- Target ESP32-C3, 8 MB, no PSRAM, ESP-IDF 5.5.3. Preserve the 3 MB app limit and `cardid` at `0x356000` / `0x4000`.
- Keep application logic in `main/` and reusable hardware support in `components/bsp/`.
- Button callbacks must not block. Non-LVGL callers must hold the BSP LVGL lock.
- Protocol/text logic belongs in host-testable modules. Preserve capture/TLS allocation ordering and the measured memory budget.
- `companion/ee04/` is a separately licensed GPL-3.0-only Arduino program. Do not blend it into the MIT Passport binary.
- Markdown uses English default files with linked `.zh_CN.md` peers. Keep both aligned. Record user-visible changes in `docs/CHANGELOG.md`.
- Never commit real credentials, setup codes, transcripts, private endpoints, logs or Flash backups. Documentation fixtures must be synthetic.
- Run focused checks during iteration and `./tools/validate.sh` before delivery. Report **Build / Host tests / Device tests / Unverified** separately; a preview or build is not a device test.
- Commit/push when requested. Use Conventional Commits. Tags and releases are separate actions; CI does not create them automatically.
