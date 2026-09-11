#!/usr/bin/env python3
"""Generate documentation illustrations from actual host LVGL frames.

Requires Pillow and build/ui-preview/memo_preview. No board is accessed.
"""

from pathlib import Path
import os
import subprocess

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/images"
BUILD = ROOT / "build/ui-preview"
INK = "#17202a"
PAPER = "#f4f4ea"
BLUE = "#1689e8"


def font(size):
    # Pillow's bundled font keeps labels reproducible without OS fonts.
    return ImageFont.load_default(size=size)


def device(canvas, screen, x, y, scale=1):
    draw = ImageDraw.Draw(canvas)
    w, h = 240 * scale, 320 * scale
    draw.rounded_rectangle((x + 6, y + 7, x + w + 30, y + h + 56), 18, fill=INK)
    draw.rounded_rectangle((x, y, x + w + 24, y + h + 49), 18, fill="#ffffff", outline=INK, width=3)
    canvas.paste(screen.resize((w, h), Image.Resampling.NEAREST), (x + 12, y + 12))
    for offset in (w // 2 - 34, w // 2, w // 2 + 34):
        draw.ellipse((x + offset + 7, y + h + 25, x + offset + 17, y + h + 35), fill=INK)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    screens = {}
    preview_env = {k: v for k, v in os.environ.items() if not k.startswith("MEMO_PREVIEW_")}
    for name, phase in (("home", 0), ("recording", 2), ("review", 4), ("history", 5),
                        ("settings", 6), ("playback", 8), ("actions", 9), ("delete-confirm", 9)):
        ppm = BUILD / f"{name}.ppm"
        env = dict(preview_env)
        if name == "delete-confirm":
            env["MEMO_PREVIEW_CONFIRM"] = "1"
        subprocess.run([str(BUILD / "memo_preview"), str(phase), str(ppm)], env=env, check=True)
        screens[name] = Image.open(ppm).convert("RGB")
        screens[name].save(OUT / f"{name}.png")

    cover = Image.new("RGB", (1200, 650), PAPER)
    draw = ImageDraw.Draw(cover)
    draw.rectangle((0, 0, 1200, 12), fill=BLUE)
    draw.text((48, 42), "PASSPORT MEMO", font=font(46), fill=INK)
    draw.text((49, 102), "Press. Speak. Keep the idea.", font=font(23), fill=INK)
    for (name, label), x in zip(
        (("home", "01  READY"), ("recording", "02  LISTENING"), ("review", "03  REVIEW & SAVE")),
        (76, 465, 854),
    ):
        draw.text((x, 167), label, font=font(18), fill=INK)
        device(cover, screens[name], x, 202)
    draw.text((48, 610), "ESP32-C3   /   ON-DEVICE OPUS   /   DIRECT ASR", font=font(18), fill=INK)
    cover.save(OUT / "overview.png")

    flow = Image.new("RGB", (930, 445), PAPER)
    draw = ImageDraw.Draw(flow)
    for name, label, x in (("home", "PRESS OK", 22), ("recording", "SPEAK / STOP", 333), ("review", "REVIEW / SAVE", 644)):
        draw.text((x, 12), label, font=font(17), fill=INK)
        device(flow, screens[name], x, 49)
    flow.save(OUT / "recording-flow.png")
    replay = Image.new("RGB", (930, 445), PAPER)
    draw = ImageDraw.Draw(replay)
    for name, label, x in (("review", "DOUBLE OK: REPLAY", 22), ("playback", "UP / DOWN: VOLUME", 333), ("review", "OK: STOP, THEN SAVE", 644)):
        draw.text((x, 12), label, font=font(17), fill=INK)
        device(replay, screens[name], x, 49)
    replay.save(OUT / "playback-flow.png")
    deletion = Image.new("RGB", (930, 445), PAPER)
    draw = ImageDraw.Draw(deletion)
    for name, label, x in (("history", "HOLD DOWN: ACTIONS", 22),
                           ("actions", "OK: DELETE MENU", 333),
                           ("delete-confirm", "DOWN + OK: DELETE", 644)):
        draw.text((x, 12), label, font=font(17), fill=INK)
        device(deletion, screens[name], x, 49)
    deletion.save(OUT / "delete-flow.png")
    print(f"Documentation images: {OUT}")


if __name__ == "__main__":
    main()
