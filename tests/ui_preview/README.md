[简体中文](README.zh_CN.md) · **English**

# UI previews

This target compiles the actual `memo_ui.c`, pixel theme and font against host LVGL. It uses synthetic notes, a sample IP and the demonstration code `12345678`; no device is contacted.

After fetching managed dependencies with a firmware build:

```sh
cmake -S tests/ui_preview -B build/ui-preview
cmake --build build/ui-preview
build/ui-preview/memo_preview 0 build/home.ppm
build/ui-preview/memo_preview 2 build/recording.ppm
build/ui-preview/memo_preview 4 build/review.ppm
build/ui-preview/memo_preview 6 build/settings.ppm
build/ui-preview/memo_preview 8 build/playback.ppm
build/ui-preview/memo_preview 9 build/actions.ppm
MEMO_PREVIEW_CONFIRM=1 build/ui-preview/memo_preview 9 build/delete-confirm.ppm
```

Phase numbers follow `memo_phase_t` in `main/memo_core.h`. Set `MEMO_PREVIEW_MAX_TEXT=1` for a 240-character memory stress fixture. The renderer reports LVGL heap usage.

The fixture clock is fixed at 09:41 Beijing time. Set `MEMO_PREVIEW_UNSYNCED=1` to show `--:--`, or `MEMO_PREVIEW_CLOCK_ROLLOVER=1` to assert a minute update while the view revision stays unchanged. Phase 9 renders actions; `MEMO_PREVIEW_CONFIRM=1` opens confirmation and `MEMO_PREVIEW_DELETE_SELECTED=1` selects deletion. Clock and menu bounds are asserted.

To regenerate documentation images, install Pillow and run `python3 tools/render_docs.py` after the preview build. The portal fixture is served by `python3 tools/preview_portal.py`; open its localhost URL and log in using `12345678`. It serves synthetic data only, refuses configuration/note writes, and does not connect to a board or cloud service. Capture the browser settings view with the legacy credentials section expanded.
