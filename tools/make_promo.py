#!/usr/bin/env python3
"""
ReplicantEdgy - cyberpunk promo generator.

Builds the full soundtrack (the plugin's own 74 s demo + a synthesized techno
bed) and renders a 1920x1080 / 30 fps promo using the real panel screenshots.

Usage:
    python3 tools/make_promo.py [--audio-only] [--frames-only]

Outputs:
    assets/replicantedgy_promo_1080p.mp4
    assets/replicantedgy_poster.png
"""

import math
import os
import subprocess
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")

W, H = 1920, 1080
FPS = 30
DURATION = 74.0
SAMPLE_RATE = 48000

MAGENTA = (255, 47, 142)
MAGENTA_HOT = (255, 170, 210)
CYAN = (47, 230, 255)
LIME = (182, 255, 47)
YELLOW = (255, 210, 63)
WHITE = (238, 232, 244)
MUTED = (160, 146, 172)
DARK = (6, 4, 9)

FONT_TITLE = "/usr/share/fonts/TTF/RobotoCondensed-Bold.ttf"
FONT_MONO = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Bold.ttf"
FONT_MONO_REG = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf"

_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _cache:
        _cache[key] = ImageFont.truetype(path, size)
    return _cache[key]


# ---------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------
def load_stem(path, n):
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8)

    if sw == 3:
        b = raw.reshape(-1, 3)
        v = (b[:, 0].astype(np.int32)
             | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        v = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float32) / 8388608.0
    elif sw == 2:
        v = raw.view(np.int16).astype(np.float32) / 32768.0
    else:
        v = raw.astype(np.float32) / 127.0 - 1.0

    v = v.reshape(-1, ch)

    if ch == 1:
        v = np.repeat(v, 2, axis=1)

    if len(v) < n:
        v = np.pad(v, ((0, n - len(v)), (0, 0)))

    return v[:n]


def env_ramp(n, points):
    times = np.array([p[0] for p in points], dtype=np.float64)
    gains = np.array([p[1] for p in points], dtype=np.float64)
    t = np.arange(n) / SAMPLE_RATE
    return np.interp(t, times, gains).astype(np.float32)


def make_kick(rng):
    n = int(0.55 * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    f = 48.0 + 160.0 * np.exp(-t * 36.0)
    phase = 2.0 * np.pi * np.cumsum(f) / SAMPLE_RATE
    body = np.sin(phase) * np.exp(-t * 6.0)
    click = rng.standard_normal(n) * np.exp(-t * 120.0) * 0.32
    x = np.tanh((body + click) * 1.8)
    return (x / max(np.abs(x).max(), 1e-9) * 0.92).astype(np.float32)


def make_hat(rng, dur, decay, hp=7200.0):
    n = int(dur * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    noise = signal.sosfilt(signal.butter(2, hp, "hp", fs=SAMPLE_RATE, output="sos"),
                           rng.standard_normal(n))
    x = noise * np.exp(-t * decay)
    return (x / max(np.abs(x).max(), 1e-9) * 0.8).astype(np.float32)


def make_clap(rng):
    n = int(0.34 * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    noise = signal.sosfilt(signal.butter(2, [900.0, 3400.0], "bp", fs=SAMPLE_RATE,
                                         output="sos"),
                           rng.standard_normal(n))
    env = np.exp(-t * 11.0)
    for delay, amp in ((0.0, 1.0), (0.011, 0.8), (0.023, 0.6)):
        env += amp * np.exp(-np.maximum(t - delay, 0.0) * 55.0) * (t >= delay)
    x = noise * env
    return (x / max(np.abs(x).max(), 1e-9) * 0.8).astype(np.float32)


def build_audio():
    n = int(DURATION * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    rng = np.random.default_rng(303)

    stem_path = os.path.join(ASSETS, "replicantedgy_demo.wav")

    if not os.path.exists(stem_path):
        raise SystemExit(
            "Missing assets/replicantedgy_demo.wav\n"
            "Render it first:\n"
            "  ninja -C build render_audio_demo\n"
            "  ./build/render_audio_demo assets/replicantedgy_demo.wav"
        )

    stem = load_stem(stem_path, n)

    beat = 60.0 / 132.0
    first_kick = 6.0

    kick = np.zeros(n, dtype=np.float32)
    hit = make_kick(rng)
    k = 0
    while first_kick + k * beat < 64.0:
        pos = int((first_kick + k * beat) * SAMPLE_RATE)
        end = min(pos + len(hit), n)
        kick[pos:end] += hit[: end - pos]
        k += 1

    hats = np.zeros(n, dtype=np.float32)
    hat_closed = make_hat(rng, 0.075, 85.0)
    hat_open = make_hat(rng, 0.20, 20.0, hp=6000.0)
    step = beat / 4.0
    s = 0
    while first_kick + s * step < 64.0:
        at = first_kick + s * step
        if at > 12.0:
            h = hat_open if (s % 4 == 2) else hat_closed
            amp = 0.42 if (s % 4 == 2) else (0.28 if s % 2 else 0.15)
            pos = int(at * SAMPLE_RATE)
            end = min(pos + len(h), n)
            hats[pos:end] += h[: end - pos] * amp
        s += 1

    clap = np.zeros(n, dtype=np.float32)
    clap_hit = make_clap(rng)
    b = 0
    while first_kick + b * beat < 63.0:
        at = first_kick + b * beat
        if b % 4 in (1, 3) and at > 18.0:
            pos = int(at * SAMPLE_RATE)
            end = min(pos + len(clap_hit), n)
            clap[pos:end] += clap_hit[: end - pos] * 0.55
        b += 1

    # Dark drone under the intro and the outro.
    pad = np.zeros((n, 2), dtype=np.float32)
    for ch, detune in ((0, 0.997), (1, 1.003)):
        p = np.zeros(n, dtype=np.float32)
        for f in (55.0, 82.407, 110.0, 164.814):
            p += (2.0 * ((f * detune * t) % 1.0) - 1.0).astype(np.float32) * 0.16
        p = signal.sosfilt(signal.butter(2, 380.0, "lp", fs=SAMPLE_RATE, output="sos"), p)
        pad[:, ch] = p

    pad_gain = env_ramp(n, [(0, 0.55), (6, 0.16), (40, 0.10), (60, 0.15),
                            (68, 0.30), (74, 0.0)])

    # Rain / air.
    rain = np.zeros((n, 2), dtype=np.float32)
    for ch in range(2):
        noise = signal.sosfilt(signal.butter(2, 1900.0, "hp", fs=SAMPLE_RATE, output="sos"),
                               rng.standard_normal(n))
        rain[:, ch] = noise * (0.7 + 0.3 * np.sin(2 * np.pi * t / 15.0 + ch))
    rain *= 0.030

    # Sidechain.
    duck = np.ones(n, dtype=np.float32)
    k = 0
    while first_kick + k * beat < 64.0:
        pos = int((first_kick + k * beat) * SAMPLE_RATE)
        length = int(0.30 * SAMPLE_RATE)
        end = min(pos + length, n)
        e = np.exp(-np.arange(end - pos) / (0.085 * SAMPLE_RATE))
        duck[pos:end] = np.minimum(duck[pos:end], (1.0 - 0.60 * e).astype(np.float32))
        k += 1

    # Swells into the section drops.
    swell = np.zeros(n, dtype=np.float32)
    for start_s, end_s, amp in ((3.0, 6.0, 0.45), (14.0, 18.0, 0.40),
                                (24.0, 28.0, 0.38), (34.0, 38.0, 0.40),
                                (44.0, 48.0, 0.38), (60.0, 64.0, 0.55)):
        a = int(start_s * SAMPLE_RATE)
        b = int(end_s * SAMPLE_RATE)
        seg = b - a
        if seg <= 0:
            continue
        noise = signal.sosfilt(signal.butter(2, 2600.0, "hp", fs=SAMPLE_RATE, output="sos"),
                               rng.standard_normal(seg))
        swell[a:b] += (noise * (np.arange(seg) / seg) ** 2.2).astype(np.float32) * amp

    stem_gain = env_ramp(n, [(0, 0.0), (1.5, 0.9), (66, 0.9), (70, 0.45), (74, 0.0)])

    mix = (stem * stem_gain[:, None] * duck[:, None] * 0.95
           + kick[:, None] * 0.75
           + hats[:, None] * 0.55
           + clap[:, None] * 0.60
           + pad * pad_gain[:, None] * 0.75
           + rain * env_ramp(n, [(0, 1.0), (10, 0.25), (62, 0.25), (70, 0.6), (74, 0.0)])[:, None]
           + swell[:, None] * 0.50)

    mix = np.tanh(mix * 1.12) / np.tanh(1.12)
    peak = np.abs(mix).max()
    if peak > 0:
        mix *= 0.80 / peak

    fade = int(0.5 * SAMPLE_RATE)
    mix[:fade] *= np.linspace(0.0, 1.0, fade)[:, None]
    mix[-int(2.0 * SAMPLE_RATE):] *= np.linspace(1.0, 0.0, int(2.0 * SAMPLE_RATE))[:, None]

    out = os.path.join(ASSETS, "replicantedgy_mix.wav")
    pcm = (np.clip(mix, -1.0, 1.0) * 32767.0).astype(np.int16)

    with wave.open(out, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(pcm.tobytes())

    print(f"[audio] wrote {out}  ({DURATION:.0f}s, peak {np.abs(mix).max():.2f}")
    return out


# ---------------------------------------------------------------------------
# Visual helpers
# ---------------------------------------------------------------------------
def spaced_text(draw, xy, text, fnt, fill, spacing=0.0, anchor="la"):
    widths = [draw.textlength(c, font=fnt) for c in text]
    total = sum(widths) + spacing * max(len(text) - 1, 0)
    x, y = xy
    if anchor[0] == "m":
        x -= total / 2.0
    elif anchor[0] == "r":
        x -= total
    for c, w in zip(text, widths):
        draw.text((x, y), c, font=fnt, fill=fill)
        x += w + spacing
    return total


def spaced_width(draw, text, fnt, spacing=0.0):
    widths = [draw.textlength(c, font=fnt) for c in text]
    return sum(widths) + spacing * max(len(text) - 1, 0)


def make_city():
    rng = np.random.default_rng(7)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    sky = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sky)
    for i in range(26):
        y = 690 + i * 9
        sd.rectangle([0, y, W, y + 9], fill=(255, 40, 130, max(0, 30 - i)))
    layer = Image.alpha_composite(layer, sky.filter(ImageFilter.GaussianBlur(30)))
    d = ImageDraw.Draw(layer)

    x = -40
    while x < W + 40:
        bw = int(rng.integers(50, 150))
        bh = int(rng.integers(160, 420))
        top = H - 180 - bh
        shade = int(rng.integers(8, 16))
        d.rectangle([x, top, x + bw, H], fill=(shade, shade - 1, shade + 6, 255))
        if rng.random() < 0.35:
            ax = x + bw // 2
            d.line([(ax, top), (ax, top - int(rng.integers(30, 90)))],
                   fill=(22, 18, 26, 255), width=2)
        x += bw + int(rng.integers(6, 24))

    x = -80
    while x < W + 80:
        bw = int(rng.integers(90, 240))
        bh = int(rng.integers(220, 560))
        top = H - 40 - bh
        shade = int(rng.integers(3, 9))
        d.rectangle([x, top, x + bw, H], fill=(shade, shade - 1, shade + 4, 255))
        cols = max(2, bw // 22)
        rows = max(3, bh // 26)
        for c in range(cols):
            for r in range(rows):
                if rng.random() < 0.14:
                    wx = x + 8 + c * 22
                    wy = top + 14 + r * 26
                    if wx + 4 < x + bw and wy + 6 < H:
                        col = MAGENTA if rng.random() < 0.65 else CYAN
                        d.rectangle([wx, wy, wx + 5, wy + 7],
                                    fill=col + (int(rng.integers(25, 110)),))
        x += bw + int(rng.integers(4, 18))

    return layer


def make_fog():
    rng = np.random.default_rng(11)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(18):
        x = int(rng.integers(-200, W + 200))
        y = int(rng.integers(500, 1000))
        rx = int(rng.integers(180, 520))
        ry = int(rng.integers(40, 130))
        col = MAGENTA + (int(rng.integers(5, 16)),) if rng.random() < 0.55 \
            else (60, 150, 190, int(rng.integers(5, 14)))
        d.ellipse([x - rx, y - ry, x + rx, y + ry], fill=col)
    return layer.filter(ImageFilter.GaussianBlur(70))


def make_vignette():
    y, x = np.ogrid[:H, :W]
    r = np.sqrt(((x - W / 2) / (W * 0.62)) ** 2 + ((y - H / 2) / (H * 0.62)) ** 2)
    alpha = np.clip((r - 0.55) * 220, 0, 195).astype(np.uint8)
    arr = np.zeros((H, W, 4), dtype=np.uint8)
    arr[..., 3] = alpha
    return Image.fromarray(arr, "RGBA")


def make_scanlines():
    arr = np.zeros((H, W, 4), dtype=np.uint8)
    arr[::3, :, 3] = 36
    arr[1::3, :, 3] = 12
    return Image.fromarray(arr, "RGBA")


def make_noise_tiles(count=5):
    rng = np.random.default_rng(23)
    tiles = []
    for _ in range(count):
        small = rng.integers(0, 26, size=(H // 4, W // 4), dtype=np.uint8)
        arr = np.zeros((H, W, 4), dtype=np.uint8)
        arr[..., 3] = np.repeat(np.repeat(small, 4, axis=0), 4, axis=1)
        tiles.append(Image.fromarray(arr, "RGBA"))
    return tiles


def gradient_background(top=(13, 6, 16), bottom=(2, 1, 4)):
    img = Image.new("RGB", (W, H), bottom)
    px = img.load()
    for y in range(H):
        k = y / (H - 1)
        c = tuple(int(top[i] * (1 - k) + bottom[i] * k) for i in range(3))
        for x in range(W):
            px[x, y] = c
    return img


def draw_rain(img, t, intensity=1.0):
    rng = np.random.default_rng(int(t * 1000) + 5)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(int(260 * intensity)):
        x = int(rng.integers(-100, W + 100))
        y = int(rng.integers(0, H))
        length = int(rng.integers(18, 70))
        dx = int(length * 0.16)
        a = int(rng.integers(14, 55))
        col = (180, 150, 200, a) if rng.random() < 0.8 else MAGENTA + (a,)
        d.line([(x, y), (x + dx, y + length)], fill=col, width=1)
    img.alpha_composite(layer)


def draw_ui(frame, src, rect, accent=MAGENTA, scan_t=None):
    x, y, w, h = rect
    shot = src.resize((w, h), Image.LANCZOS)

    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rectangle([x - 14, y - 14, x + w + 14, y + h + 14], outline=accent + (90,), width=10)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(24)))

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rectangle([x + 10, y + 16, x + w + 10, y + h + 16], fill=(0, 0, 0, 175))
    frame.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(18)))

    frame.alpha_composite(shot.convert("RGBA"), (x, y))

    d = ImageDraw.Draw(frame)
    d.rectangle([x - 2, y - 2, x + w + 2, y + h + 2], outline=accent + (215,), width=2)

    tick = 26
    for cx, cy, sx, sy in ((x - 8, y - 8, 1, 1), (x + w + 8, y - 8, -1, 1),
                           (x - 8, y + h + 8, 1, -1), (x + w + 8, y + h + 8, -1, -1)):
        d.line([(cx, cy), (cx + sx * tick, cy)], fill=CYAN + (200,), width=2)
        d.line([(cx, cy), (cx, cy + sy * tick)], fill=CYAN + (200,), width=2)

    if scan_t is not None:
        sy = y + int((scan_t % 1.0) * h)
        sl = Image.new("RGBA", (W, 40), (0, 0, 0, 0))
        sld = ImageDraw.Draw(sl)
        sld.rectangle([x, 18, x + w, 22], fill=MAGENTA + (60,))
        frame.alpha_composite(sl.filter(ImageFilter.GaussianBlur(6)), (0, sy - 20))


def caption(frame, x, y, kicker, title, sub=None, colour=MAGENTA):
    d = ImageDraw.Draw(frame)
    d.line([(x, y - 16), (x + 70, y - 16)], fill=colour + (255,), width=3)
    d.text((x, y), kicker, font=font(FONT_MONO, 22), fill=colour + (255,))
    d.text((x, y + 34), title, font=font(FONT_TITLE, 58), fill=WHITE + (255,))
    if sub:
        d.text((x, y + 104), sub, font=font(FONT_MONO_REG, 24), fill=MUTED + (255,))


def big_title(frame, text, y, size=190, colour=MAGENTA, spacing=6.0, alpha=255, glitch=0.0):
    d0 = ImageDraw.Draw(frame)
    fnt = font(FONT_TITLE, size)
    total = spaced_width(d0, text, fnt, spacing)
    x = (W - total) / 2.0

    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    spaced_text(gd, (x, y), text, fnt, colour + (255,), spacing)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(28)))

    layer_r = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    spaced_text(ImageDraw.Draw(layer_r), (x - 3 - glitch, y), text, fnt,
                (255, 20, 110, 130), spacing)
    frame.alpha_composite(layer_r)

    layer_c = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    spaced_text(ImageDraw.Draw(layer_c), (x + 3 + glitch, y), text, fnt,
                (40, 230, 255, 120), spacing)
    frame.alpha_composite(layer_c)

    spaced_text(ImageDraw.Draw(frame), (x, y), text, fnt, colour + (alpha,), spacing)


def kick_pulse(t):
    beat = 60.0 / 132.0
    first = 6.0
    if t < first:
        return 0.0
    phase = ((t - first) % beat) / beat
    return float(max(0.0, 1.0 - phase * 3.4))


# ---------------------------------------------------------------------------
# Scenes
# ---------------------------------------------------------------------------
def scene_intro(frame, t):
    d = ImageDraw.Draw(frame)
    d.text((120, 96), "OFF-WORLD AUDIO DIVISION // SIGNAL 303", font=font(FONT_MONO, 24),
           fill=CYAN + (220,))
    d.text((W - 300, 96), "LOS ANGELES  //  2049", font=font(FONT_MONO, 24),
           fill=MUTED + (180,))

    lines = [
        (0.4, "> BOOTING REPLICANT EDGE ............. OK"),
        (1.1, "> VCO 1 / VCO 2 ..................... ONLINE"),
        (1.8, "> 4-POLE LADDER FILTER ............... CALIBRATED"),
        (2.5, "> PATCH BAY .......................... 24 POINTS"),
        (3.2, "> PRESET ARCHIVE ..................... READY"),
        (4.2, "> SIGNAL ACQUIRED"),
    ]

    y = 300
    for at, line in lines:
        if t < at:
            break
        shown = line[: max(0, int((t - at) * 42) + 1)]
        d.text((150, y), shown, font=font(FONT_MONO_REG, 28), fill=MAGENTA + (235,))
        y += 52

    if int(t * 2.5) % 2 == 0:
        d.rectangle([150, y + 2, 168, y + 30], fill=MAGENTA + (220,))

    width = int(min(1.0, t / 5.5) * 640)
    d.line([(150, 272), (150 + width, 272)], fill=MAGENTA + (190,), width=2)


def scene_title(frame, t, local):
    big_title(frame, "REPLICANT EDGE", 350, 176, MAGENTA, 8.0,
              glitch=5.0 if local < 0.35 else 0.0)

    d = ImageDraw.Draw(frame)
    d.line([(660, 620), (1260, 620)], fill=MAGENTA + (200,), width=2)
    spaced_text(d, (W / 2, 656), "NEON PROTOCOL // ACID PERCUSSION",
                font(FONT_MONO, 30), CYAN + (255,), 3.0, anchor="ma")
    d.text((W / 2, 724), "SEMI-MODULAR ANALOG PERCUSSION SYNTHESIZER",
           font=font(FONT_MONO_REG, 23), fill=MUTED + (230,), anchor="ma")
    d.text((W / 2, 768), "2 VCO  ·  24 PATCH POINTS  ·  8-STEP SEQUENCER",
           font=font(FONT_MONO_REG, 21), fill=MAGENTA_HOT + (225,), anchor="ma")
    d.text((W / 2, 818), "VST3   ·   AU   ·   STANDALONE",
           font=font(FONT_MONO_REG, 20), fill=MUTED + (170,), anchor="ma")


def scene_panel(frame, t, local):
    draw_ui(frame, PANEL_MAGENTA, (236, 196, 1448, 860), MAGENTA, scan_t=local * 0.45)
    caption(frame, 80, 40, "01 / THE INSTRUMENT", "TWO VOICES. ONE LADDER.",
            "VCO 1  ·  VCO 2  ·  NOISE  ·  4-POLE FILTER  ·  VCA")


def scene_patch(frame, t, local):
    # Crop the patch bay and blow it up.
    crop = PANEL_MAGENTA.crop((0, 62, 1280, 212))
    rect = (60, 330, 1800, 211)
    draw_ui(frame, crop, rect, CYAN, scan_t=local * 0.9)

    # Animated cable glows over the real jacks.
    scale = rect[2] / 1280.0
    top = [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0),
           (7, 0), (8, 0), (9, 0), (10, 0), (11, 0)]
    pairs = [(8, 0, 0, 1), (9, 0, 0, 0), (11, 0, 4, 1), (7, 0, 1, 1), (11, 1, 6, 1)]
    colours = [MAGENTA, CYAN, YELLOW, LIME, (176, 107, 255)]

    for i, (x1, r1, x2, r2) in enumerate(pairs):
        a = (rect[0] + (104 + x1 * 88) * scale,
             rect[1] + ((120 if r1 == 0 else 180) - 62) * scale)
        b = (rect[0] + (104 + x2 * 88) * scale,
             rect[1] + ((120 if r2 == 0 else 180) - 62) * scale)
        phase = (local * 0.9 + i * 0.2) % 1.0

        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        mid = ((a[0] + b[0]) / 2, max(a[1], b[1]) + 34)
        ld.line([a, mid, b], fill=colours[i] + (120,), width=9, joint="curve")
        frame.alpha_composite(layer.filter(ImageFilter.GaussianBlur(10)))

        d = ImageDraw.Draw(frame)
        d.line([a, mid, b], fill=colours[i] + (235,), width=3, joint="curve")

        # Travelling spark.
        px = a[0] + (b[0] - a[0]) * phase
        py = a[1] + (b[1] - a[1]) * phase + math.sin(phase * math.pi) * 34
        d.ellipse([px - 6, py - 6, px + 6, py + 6], fill=WHITE + (230,))

    caption(frame, 80, 60, "02 / PATCH BAY", "PATCH IT LIKE HARDWARE.",
            "24 JACKS  ·  CV + AUDIO  ·  DRAG OUT TO IN  ·  RIGHT-CLICK TO UNPATCH")

    d = ImageDraw.Draw(frame)
    for i, line in enumerate(("EVERY KNOB HAS A JACK.", "EVERY JACK GOES SOMEWHERE.",
                              "FEEDBACK IS ALLOWED.")):
        d.text((80, 700 + i * 36), line, font=font(FONT_MONO_REG, 23), fill=MUTED + (235,))


def scene_sequence(frame, t, local):
    crop = PANEL_MAGENTA.crop((0, 560, 1280, 758))
    rect = (60, 330, 1800, 279)
    draw_ui(frame, crop, rect, MAGENTA, scan_t=local * 1.1)

    # Step playhead.
    scale = rect[2] / 1280.0
    step = int(local * 4.0) % 8
    x = rect[0] + (596 + step * 84 + 24) * scale
    y = rect[1] + (620 - 560) * scale
    d = ImageDraw.Draw(frame)
    d.rectangle([x - 28 * scale, y - 10, x + 28 * scale, y + 52 * scale],
                outline=MAGENTA + (230,), width=3)

    caption(frame, 80, 60, "03 / SEQUENCER", "EIGHT STEPS. NO MERCY.",
            "PITCH + VELOCITY PER STEP  ·  TRIGGER / PLAY / ADVANCE  ·  INT / MIDI / EXT")

    for i, line in enumerate(("CLOCK DIVISIONS FROM 1/32 TO 1/4,", "WITH TRIPLETS.",
                              "ADV/CLOCK PATCH POINT FOR EXTERNAL GEAR.")):
        d.text((80, 700 + i * 36), line, font=font(FONT_MONO_REG, 23), fill=MUTED + (235,))


def scene_presets(frame, t, local):
    crop = DRAWER.crop((0, 760, 1280, 1100))
    rect = (60, 330, 1800, 450)
    draw_ui(frame, crop, rect, LIME, scan_t=local * 0.8)

    caption(frame, 80, 60, "04 / PRESET ARCHIVE", "FACTORY. AND YOURS.",
            "26 FACTORY PATCHES  ·  SAVE / DELETE  ·  IMPORT + EXPORT .EDGY")

    d = ImageDraw.Draw(frame)

    for i, line in enumerate(("SAVE THE PATCH YOU JUST FOUND.", "NAME IT. KEEP IT.",
                              "TRADE IT AS A SINGLE .EDGY FILE.")):
        d.text((80, 818 + i * 36), line, font=font(FONT_MONO_REG, 23), fill=MUTED + (235,))


def scene_skins(frame, t, local):
    draw_ui(frame, PANEL_LIME, (70, 300, 880, 522), LIME)
    draw_ui(frame, PANEL_BLUE, (970, 300, 880, 522), (79, 195, 255))

    d = ImageDraw.Draw(frame)
    d.text((70, 840), "TOXIC LIME", font=font(FONT_MONO, 24), fill=LIME + (255,))
    d.text((970, 840), "CHROME BLUE", font=font(FONT_MONO, 24), fill=(79, 195, 255, 255))
    d.text((W / 2, 52), "THREE SKINS. ONE CIRCUIT.", font=font(FONT_TITLE, 58),
           fill=WHITE + (245,), anchor="ma")
    d.text((W / 2, 122), "NEON MAGENTA  ·  TOXIC LIME  ·  CHROME BLUE",
           font=font(FONT_MONO_REG, 21), fill=MUTED + (225,), anchor="ma")


def scene_features(frame, t, local):
    d = ImageDraw.Draw(frame)
    d.text((80, 120), "05 / THE CIRCUIT", font=font(FONT_MONO, 24), fill=CYAN + (230,))
    d.line([(80, 168), (220, 168)], fill=MAGENTA + (255,), width=3)

    columns = [
        ("2 VCO", "TRIANGLE / SQUARE / SAW", "HARD SYNC + 1-2 FM"),
        ("24 POINTS", "CV AND AUDIO", "FEEDBACK ALLOWED"),
        ("3 EGs", "VCO · VCF · VCA", "FAST / SLOW VCA"),
        ("8 STEPS", "PITCH + VELOCITY", "INT · MIDI · EXT"),
    ]

    for i, (head, line1, line2) in enumerate(columns):
        x = 80 + (i % 2) * 900
        y = 240 + (i // 2) * 280
        d.rectangle([x, y, x + 820, y + 210], outline=MAGENTA + (110,), width=2)
        d.text((x + 30, y + 26), head, font=font(FONT_TITLE, 62), fill=MAGENTA + (255,))
        d.text((x + 30, y + 110), line1, font=font(FONT_MONO, 21), fill=WHITE + (235,))
        d.text((x + 30, y + 148), line2, font=font(FONT_MONO_REG, 19), fill=MUTED + (225,))


def scene_end(frame, t, local):
    d = ImageDraw.Draw(frame)

    cx, cy = W / 2, 250
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([cx - 62, cy - 62, cx + 62, cy + 62], outline=MAGENTA + (255,), width=3)
    zig = [(cx - 32, cy + 12), (cx - 14, cy - 18), (cx + 2, cy + 14),
           (cx + 18, cy - 18), (cx + 34, cy + 10)]
    gd.line(zig, fill=MAGENTA + (255,), width=6)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(14)))
    d.ellipse([cx - 62, cy - 62, cx + 62, cy + 62], outline=MAGENTA + (230,), width=3)
    d.line(zig, fill=MAGENTA + (255,), width=5)

    big_title(frame, "REPLICANT EDGE", 360, 150, MAGENTA, 7.0)

    d.line([(700, 570), (1220, 570)], fill=MAGENTA + (180,), width=2)
    spaced_text(d, (W / 2, 610), "DESIGNED FOR THE UNDERGROUND",
                font(FONT_MONO, 26), CYAN + (240,), 2.5, anchor="ma")
    d.text((W / 2, 684), "VST3   ·   AU   ·   STANDALONE",
           font=font(FONT_MONO_REG, 22), fill=MUTED + (220,), anchor="ma")
    d.text((W / 2, 726), "24 PATCH POINTS  ·  8-STEP SEQUENCER  ·  PRESET ARCHIVE",
           font=font(FONT_MONO_REG, 20), fill=MAGENTA_HOT + (215,), anchor="ma")
    d.text((W / 2, 786), "github.com/DanimanX/ReplicantEdgy",
           font=font(FONT_MONO_REG, 24), fill=WHITE + (230,), anchor="ma")
    d.text((W / 2, 836), "DANIMANX", font=font(FONT_MONO, 20), fill=MAGENTA + (180,),
           anchor="ma")


# ---------------------------------------------------------------------------
# Frame assembly
# ---------------------------------------------------------------------------
def load_png(name):
    return Image.open(os.path.join(ASSETS, name)).convert("RGB")


CITY = FOG = VIGNETTE = SCAN = NOISE = BG = None
PANEL_MAGENTA = PANEL_LIME = PANEL_BLUE = DRAWER = None


def init_visuals():
    global CITY, FOG, VIGNETTE, SCAN, NOISE, BG
    global PANEL_MAGENTA, PANEL_LIME, PANEL_BLUE, DRAWER
    CITY = make_city()
    FOG = make_fog()
    VIGNETTE = make_vignette()
    SCAN = make_scanlines()
    NOISE = make_noise_tiles()
    BG = gradient_background()
    PANEL_MAGENTA = load_png("replicantedgy_ui_magenta.png")
    PANEL_LIME = load_png("replicantedgy_ui_lime.png")
    PANEL_BLUE = load_png("replicantedgy_ui_blue.png")
    DRAWER = load_png("replicantedgy_drawer.png")


def render_frame(t):
    frame = BG.convert("RGBA").copy()
    frame.alpha_composite(CITY)
    frame.alpha_composite(FOG, (int(30 * math.sin(t * 0.13)), int(12 * math.sin(t * 0.21))))
    draw_rain(frame, t, 1.0 if t < 8 or t > 60 else 0.55)

    pulse = kick_pulse(t)
    if pulse > 0.01:
        glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.ellipse([W / 2 - 520, 100, W / 2 + 520, 700],
                   fill=(255, 40, 140, int(10 + 28 * pulse)))
        frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(80)))

    if t < 5.0:
        scene_intro(frame, t)
    elif t < 11.0:
        scene_title(frame, t, t - 5.0)
    elif t < 18.0:
        scene_panel(frame, t, t - 11.0)
    elif t < 28.0:
        scene_patch(frame, t, t - 18.0)
    elif t < 38.0:
        scene_sequence(frame, t, t - 28.0)
    elif t < 48.0:
        scene_presets(frame, t, t - 38.0)
    elif t < 56.0:
        scene_skins(frame, t, t - 48.0)
    elif t < 64.0:
        scene_features(frame, t, t - 56.0)
    else:
        scene_end(frame, t, t - 64.0)

    frame.alpha_composite(SCAN)
    frame.alpha_composite(VIGNETTE)

    if t > 0.4:
        frame.alpha_composite(NOISE[int(t * 30) % len(NOISE)])

    if t < 0.5:
        k = t / 0.5
        frame = Image.eval(frame, lambda v: int(v * k))
    if t > DURATION - 2.0:
        k = max(0.0, (DURATION - t) / 2.0)
        frame = Image.eval(frame, lambda v: int(v * k))

    cuts = (5.0, 11.0, 18.0, 28.0, 38.0, 48.0, 56.0, 64.0)
    for cut in cuts:
        if 0 <= t - cut < 0.12:
            arr = np.array(frame)
            rng = np.random.default_rng(int(t * 10000) + int(cut))
            for _ in range(5):
                y0 = int(rng.integers(0, H - 60))
                hgt = int(rng.integers(10, 60))
                arr[y0:y0 + hgt] = np.roll(arr[y0:y0 + hgt],
                                            int(rng.integers(-40, 40)), axis=1)
            frame = Image.fromarray(arr)
            break

    return frame.convert("RGB")


def main():
    audio_only = "--audio-only" in sys.argv
    frames_only = "--frames-only" in sys.argv

    if not frames_only:
        mix_path = build_audio()
    else:
        mix_path = os.path.join(ASSETS, "replicantedgy_mix.wav")

    if audio_only:
        return

    init_visuals()

    out_path = os.path.join(ASSETS, "replicantedgy_promo_1080p.mp4")
    poster_path = os.path.join(ASSETS, "replicantedgy_poster.png")

    cmd = [
        "ffmpeg", "-y", "-v", "error",
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
        "-i", mix_path,
        "-c:v", "libx264", "-preset", "medium", "-crf", "20",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart",
        "-c:a", "aac", "-b:a", "192k",
        "-shortest", out_path,
    ]

    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    total = int(DURATION * FPS)

    for i in range(total):
        t = i / FPS
        frame = render_frame(t)

        if i == int(22.0 * FPS):
            frame.save(poster_path)

        proc.stdin.write(frame.tobytes())

        if i % 150 == 0:
            print(f"[video] frame {i}/{total}  ({t:5.1f}s)")

    proc.stdin.close()
    proc.wait()

    if proc.returncode != 0:
        print("[error] ffmpeg failed")
        sys.exit(1)

    print(f"[video] wrote {out_path}")
    print(f"[video] poster {poster_path}")


if __name__ == "__main__":
    main()
