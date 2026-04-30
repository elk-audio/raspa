"""
report.py  –  Visualise a JSON audio-test report in a single matplotlib window.

Usage:
    python report.py -f report.json
    python report.py -f report.json -o report.png --no-show
    python report.py -f report.json -o report.pdf --dpi 300 --no-show
"""

import argparse
import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.patches as mpatches
import numpy as np

# —— Constants —————————————————————————————————————————————————————————————————

MAX_WINDOW_WIDTH   = 15.0
MAX_WINDOW_HEIGHT  = 10.0

# ── palette ───────────────────────────────────────────────────────────────────
CH_COLORS   = ["#2196F3", "#FF5722", "#4CAF50", "#9C27B0"]
PASS_COLOR   = "#2e7d32"   # dark green
FAIL_COLOR   = "#c62828"   # dark red
THRESH_COLOR = "#FF9800"   # orange – threshold / reference lines
IGNORED_COLOR = "#9e9e9e"  # grey – ignored channels
BG_SECTION   = "#f5f5f5"
BG_MAIN      = "#ffffff"

# ── helpers ───────────────────────────────────────────────────────────────────

def ch_color(ch: int, ignored: list) -> str:
    return IGNORED_COLOR if ch in ignored else "black"


def lighten_color(hex_color: str, factor: float = 0.55) -> str:
    """Blend a hex colour toward white by factor (0 = original, 1 = white)."""
    r, g, b = int(hex_color[1:3], 16), int(hex_color[3:5], 16), int(hex_color[5:7], 16)
    r = int(r + (255 - r) * factor)
    g = int(g + (255 - g) * factor)
    b = int(b + (255 - b) * factor)
    return f"#{r:02x}{g:02x}{b:02x}"


def pass_label(passed: bool) -> tuple[str, str]:
    return ("✔ PASS", PASS_COLOR) if passed else ("✘ FAIL", FAIL_COLOR)


def section_title(ax_or_fig, title: str, passed: bool, x=0.0, y=1.0,
                  fontsize=13, ax=None):
    """Draw a bold section headline with a coloured PASS/FAIL badge."""
    label, color = pass_label(passed)
    target = ax if ax is not None else ax_or_fig
    target.text(x, y, title, transform=target.transAxes,
                fontsize=fontsize, fontweight="bold", va="bottom")
    target.text(x + 1.0, y, f"  {label}", transform=target.transAxes,
                fontsize=fontsize - 1, fontweight="bold", color=color,
                va="bottom", ha="right")


def make_fft_xaxis(n_bins: int, max_freq: float = 22050.0) -> np.ndarray:
    return np.linspace(0, max_freq, n_bins)


def fmt(v, unit="", decimals=2):
    if v is None:
        return "–"
    return f"{v:.{decimals}f}{unit}"


# ── section renderers ─────────────────────────────────────────────────────────

def render_main(fig, gs_slot, data: dict):
    """Top banner: configuration + overall pass/fail."""
    ax = fig.add_subplot(gs_slot)
    ax.set_axis_off()
    ax.set_facecolor(BG_MAIN)

    cfg = data.get("configuration", {})
    overall = data.get("all_tests_passed", None)
    badge, badge_color = pass_label(overall) if overall is not None else ("–", "gray")

    lines = [
        ("All tests passed", badge, badge_color),
    ]
    for k, v in cfg.items():
        label = k.replace("_", " ").title()
        val = str(v)
        lines.append((label, val, "black"))

    col_x  = [0.02, 0.35]
    row_h  = 0.13
    y_start = 0.88

    ##ax.text(0.02, 0.97, "Report Summary", transform=ax.transAxes,
    #        fontsize=15, fontweight="bold", va="top")

    section_title(ax, "Report Summary", True, x=0.02, y=0.93, ax=ax)

    for i, (label, val, color) in enumerate(lines):
        y = y_start - i * row_h
        ax.text(col_x[0], y, label + ":", transform=ax.transAxes,
                fontsize=10, va="top", color="#444444")
        ax.text(col_x[1], y, val, transform=ax.transAxes,
                fontsize=10, va="top", color=color, fontweight="bold")

    ax.add_patch(mpatches.FancyBboxPatch(
        (0, 0), 1, 1, transform=ax.transAxes,
        boxstyle="round,pad=0.02", linewidth=1.5,
        edgecolor="#cccccc", facecolor=BG_SECTION, zorder=0,
    ))


def render_round_trip(fig, gs_slot, test: dict):
    ax = fig.add_subplot(gs_slot)
    ax.set_axis_off()
    ax.set_facecolor(BG_SECTION)

    passed   = test.get("passed", False)
    measured = test.get("measured_delay_samples", [])
    expected = test.get("expected_delay_samples", 0)
    tolerance = test.get("tolerance_samples", None)
    ignored  = test.get("ignored_channels", [])

    section_title(ax, test.get("test_name", "Round-trip delay test"), passed,
                  x=0.02, y=0.93, ax=ax)

    rows = [("Expected delay (samples)", fmt(expected, decimals=0), "black")]
    rows += [(f"Measured – Ch {i} (samples)", fmt(v, decimals=0), ch_color(i, ignored))
             for i, v in enumerate(measured)]
    if tolerance is not None:
        rows.append(("Tolerance (samples)", fmt(tolerance, decimals=0), "black"))

    for i, (label, val, color) in enumerate(rows):
        y = 0.72 - i * 0.18
        ax.text(0.04, y, label + ":", transform=ax.transAxes,
                fontsize=9, va="top", color="#555555")
        ax.text(0.60, y, val, transform=ax.transAxes,
                fontsize=9, va="top", fontweight="bold", color=color)


def render_noise(fig, gs_slot, test: dict):
    ax = fig.add_subplot(gs_slot)
    ax.set_axis_off()
    ax.set_facecolor(BG_SECTION)

    passed    = test.get("passed", False)
    levels    = test.get("levels_db", [])
    threshold = test.get("noise_threshold_db", None)
    ignored   = test.get("ignored_channels", [])

    section_title(ax, test.get("test_name", "Background noise test"), passed,
                  x=0.02, y=0.93, ax=ax)

    rows = []
    for i, v in enumerate(levels):
        rows.append((f"Level – Ch {i}", fmt(v, " dB"), ch_color(i, ignored)))
    if threshold is not None:
        rows.append(("Threshold", fmt(threshold, " dB"), "black"))

    for i, (label, val, color) in enumerate(rows):
        y = 0.72 - i * 0.18
        ax.text(0.04, y, label + ":", transform=ax.transAxes,
                fontsize=9, va="top", color="#555555")
        ax.text(0.60, y, val, transform=ax.transAxes,
                fontsize=9, va="top", fontweight="bold", color=color)


def render_mute(fig, gs_slot, test: dict):
    ax = fig.add_subplot(gs_slot)
    ax.set_axis_off()
    ax.set_facecolor(BG_SECTION)

    passed    = test.get("passed", False)
    levels    = test.get("levels_peak_db", [])
    threshold = test.get("level_threshold_db", None)
    ignored   = test.get("ignored_channels", [])

    section_title(ax, test.get("test_name", "Mute test"), passed,
                  x=0.02, y=0.93, ax=ax)

    rows = []
    for i, v in enumerate(levels):
        rows.append((f"Level – Ch {i}", fmt(v, " dB"), ch_color(i, ignored)))
    if threshold is not None:
        rows.append(("Threshold", fmt(threshold, " dB"), "black"))

    for i, (label, val, color) in enumerate(rows):
        y = 0.72 - i * 0.18
        ax.text(0.04, y, label + ":", transform=ax.transAxes,
                fontsize=9, va="top", color="#555555")
        ax.text(0.60, y, val, transform=ax.transAxes,
                fontsize=9, va="top", fontweight="bold", color=color)


def render_level_crosstalk(fig, gs_slot, test: dict):
    ax = fig.add_subplot(gs_slot)
    ax.set_axis_off()
    ax.set_facecolor(BG_SECTION)

    passed           = test.get("passed", False)
    levels_peak      = test.get("levels_peak", [])
    levels_rms       = test.get("levels_rms", [])
    level_thr        = test.get("level_threshold_db", None)
    crosstalk_thr    = test.get("crosstalk_threshold_db", None)
    ignored          = test.get("ignored_channels", [])

    section_title(ax, test.get("test_name", "Level and crosstalk test"), passed,
                  x=0.02, y=0.97, ax=ax)

    rows = []
    for stim_ch, ch_vals in enumerate(levels_peak):
        for meas_ch, v in enumerate(ch_vals):
            tag = "(signal)" if stim_ch == meas_ch else "(crosstalk)"
            color = IGNORED_COLOR if (stim_ch in ignored) else "black"
            rows.append((f"Peak Ch{stim_ch}→Ch{meas_ch} {tag}", fmt(v, " dB"), color))
    for stim_ch, ch_vals in enumerate(levels_rms):
        for meas_ch, v in enumerate(ch_vals):
            tag = "(signal)" if stim_ch == meas_ch else "(crosstalk)"
            color = IGNORED_COLOR if (stim_ch in ignored) else "black"
            rows.append((f"RMS  Ch{stim_ch}→Ch{meas_ch} {tag}", fmt(v, " dB"), color))
    if level_thr is not None:
        rows.append(("Level threshold", fmt(level_thr, " dB"), "black"))
    if crosstalk_thr is not None:
        rows.append(("Crosstalk threshold", fmt(crosstalk_thr, " dB"), "black"))

    row_h = min(0.085, 0.80 / max(len(rows), 1))
    for i, (label, val, color) in enumerate(rows):
        y = 0.86 - i * row_h
        ax.text(0.04, y, label + ":", transform=ax.transAxes,
                fontsize=8.5, va="top", color="#555555")
        ax.text(0.68, y, val, transform=ax.transAxes,
                fontsize=8.5, va="top", fontweight="bold", color=color)


def render_distortion(fig, gs_slot_text, gs_slot_plot, test: dict):
    """Two sub-sections: a text summary + the FFT plot."""
    # ── text panel ────────────────────────────────────────────────────────────
    ax_text = fig.add_subplot(gs_slot_text)
    ax_text.set_axis_off()
    ax_text.set_facecolor(BG_SECTION)

    passed    = test.get("passed", False)
    ratios    = test.get("ratios", [])
    threshold = test.get("threshold_db", None)
    ignored   = test.get("ignored_channels", [])

    section_title(ax_text, test.get("test_name", "Distortion test"), passed,
                  x=0.02, y=0.93, ax=ax_text)

    rows = [(f"THD+N ratio – Ch {i}", fmt(v, " dB"), ch_color(i, ignored)) for i, v in enumerate(ratios)]
    if threshold is not None:
        rows.append(("Threshold", fmt(threshold, " dB"), "black"))

    for i, (label, val, color) in enumerate(rows):
        y = 0.72 - i * 0.18
        ax_text.text(0.04, y, label + ":", transform=ax_text.transAxes,
                     fontsize=9, va="top", color="#555555")
        ax_text.text(0.60, y, val, transform=ax_text.transAxes,
                     fontsize=9, va="top", fontweight="bold", color=color)

    # ── FFT plot ──────────────────────────────────────────────────────────────
    ax = fig.add_subplot(gs_slot_plot)
    fft_data  = test.get("fft", [])
    threshold = test.get("threshold_db", None)

    for i, ch in enumerate(fft_data):
        arr = np.array(ch, dtype=float)
        # replace zeros/negatives so log scale works
        arr = np.where(arr > 0, arr, np.nan)
        x = make_fft_xaxis(len(arr))
        base = CH_COLORS[i % len(CH_COLORS)]
        color = lighten_color(base) if i in ignored else base
        lw = 0.5 if i in ignored else 1.2
        ax.plot(x, arr, color=color, linewidth=lw, label=f"Ch {i}", alpha=0.85)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(20, 22050)
    ax.set_xlabel("Frequency (Hz)", fontsize=8)
    ax.set_ylabel("Amplitude", fontsize=8)
    ax.set_title("Distortion FFT", fontsize=9, fontweight="bold")
    ax.legend(fontsize=7, loc="upper right")
    ax.grid(True, which="both", linestyle="--", linewidth=0.35, alpha=0.5)
    ax.tick_params(labelsize=7)
    _set_freq_xticks(ax)


def render_frequency_response(fig, gs_slot_text, gs_slot_plot, test: dict):
    """Two sub-sections: a text summary + the FFT plot."""
    # ── text panel ────────────────────────────────────────────────────────────
    ax_text = fig.add_subplot(gs_slot_text)
    ax_text.set_axis_off()
    ax_text.set_facecolor(BG_SECTION)

    passed   = test.get("passed", False)
    response = test.get("response", [])
    margin   = test.get("margin_db", None)
    ignored  = test.get("ignored_channels", [])

    section_title(ax_text, test.get("test_name", "Frequency response test"), passed,
                  x=0.02, y=0.93, ax=ax_text)

    rows = []
    for i, ch in enumerate(response):
        rows.append((f"Ch {i} max", fmt(ch.get("max_db"), " dB"), ch_color(i, ignored)))
        rows.append((f"Ch {i} min", fmt(ch.get("min_db"), " dB"), ch_color(i, ignored)))
    if margin is not None:
        rows.append(("Margin", fmt(margin, " dB"), "black"))

    for i, (label, val, color) in enumerate(rows):
        y = 0.72 - i * 0.14
        ax_text.text(0.04, y, label + ":", transform=ax_text.transAxes,
                     fontsize=9, va="top", color="#555555")
        ax_text.text(0.60, y, val, transform=ax_text.transAxes,
                     fontsize=9, va="top", fontweight="bold", color=color)

    # ── FFT plot ──────────────────────────────────────────────────────────────
    ax = fig.add_subplot(gs_slot_plot)
    fft_data = test.get("fft", [])

    for i, ch in enumerate(fft_data):
        arr = np.array(ch, dtype=float)
        arr = np.where(arr > 0, arr, np.nan)
        x = make_fft_xaxis(len(arr))
        base = CH_COLORS[i % len(CH_COLORS)]
        color = lighten_color(base) if i in ignored else base
        lw = 0.5 if i in ignored else 1.2
        ax.plot(x, arr, color=color, linewidth=lw, label=f"Ch {i}", alpha=0.85)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(20, 22050)
    ax.set_xlabel("Frequency (Hz)", fontsize=8)
    ax.set_ylabel("Amplitude", fontsize=8)
    ax.set_title("Frequency Response FFT", fontsize=9, fontweight="bold")
    ax.legend(fontsize=7, loc="lower left")
    ax.grid(True, which="both", linestyle="--", linewidth=0.35, alpha=0.5)
    ax.tick_params(labelsize=7)
    _set_freq_xticks(ax)


def _set_freq_xticks(ax):
    ticks = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
    ax.set_xticks(ticks)
    ax.set_xticklabels([
        "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"
    ], fontsize=7)
    ax.xaxis.set_minor_formatter(plt.NullFormatter())


# ── constants ─────────────────────────────────────────────────────────────────

SIMPLE_TESTS = [
    "Round-trip delay test",
    "Background noise test",
    "Mute test",
    "Level and crosstalk test",
]
FFT_TESTS = [
    "Distortion test",
    "Frequency response test",
]

# Height weights per panel: 1 title line + N data rows.
HEADER_ROWS = 1.4   # title line weight (a bit taller than a data row)
DATA_ROW_H  = 1.0   # one label+value line


def _panel_height(test: dict) -> float:
    """Return a proportional height weight for a left-column text panel."""
    name = test.get("test_name", "")

    if name == "Round-trip delay test":
        measured = test.get("measured_delay_samples", [])
        has_tol  = "tolerance_samples" in test
        n_rows   = 1 + len(measured) + int(has_tol)

    elif name == "Background noise test":
        levels  = test.get("levels_db", [])
        has_thr = "noise_threshold_db" in test
        n_rows  = len(levels) + int(has_thr)

    elif name == "Mute test":
        levels  = test.get("levels_db", [])
        has_thr = "level_threshold_db" in test
        n_rows  = len(levels) + int(has_thr)

    elif name == "Level and crosstalk test":
        peak   = test.get("levels_peak", [])
        rms    = test.get("levels_rms",  [])
        n_rows = (sum(len(r) for r in peak)
                  + sum(len(r) for r in rms)
                  + int("level_threshold_db"    in test)
                  + int("crosstalk_threshold_db" in test))
    else:
        n_rows = 3

    return HEADER_ROWS + n_rows * DATA_ROW_H


def _main_panel_height(data: dict) -> float:
    """Height for the summary banner: title + overall-pass + config fields."""
    n_rows = 1 + len(data.get("configuration", {}))
    return HEADER_ROWS + n_rows * DATA_ROW_H


# ── layout builder ────────────────────────────────────────────────────────────

def build_layout(data: dict):
    """
    Two-column layout:
      Left  – summary banner + text-only test panels, each sized by row count.
      Right – one FFT plot per FFT test, equal height, stacked vertically.
    """
    tests    = data.get("tests", [])
    test_map = {t["test_name"]: t for t in tests}

    present_simple = [n for n in SIMPLE_TESTS if n in test_map]
    present_fft    = [n for n in FFT_TESTS    if n in test_map]

    # Height weight for every left-column slot
    left_heights = [_main_panel_height(data)]
    for name in present_simple:
        left_heights.append(_panel_height(test_map[name]))

    n_fft      = len(present_fft)
    UNIT_H     = 0.55          # inches per height-weight unit
    right_h    = max(n_fft, 1) * 4.5
    left_h     = sum(left_heights) * UNIT_H
    fig_height = max(right_h, left_h) + 0.6
    fig_width  = 16.0

    # ── fit to screen ─────────────────────────────────────────────────────────
    fig_width  *= MAX_WINDOW_WIDTH / fig_width
    fig_height *= MAX_WINDOW_HEIGHT /fig_height

    fig = plt.figure(figsize=(fig_width, fig_height), facecolor=BG_MAIN)

    # Outer 2-column grid  (left 40 % | right 60 %)
    outer = gridspec.GridSpec(
        1, 2, figure=fig,
        width_ratios=[4, 6],
        left=0.03, right=0.97, top=0.97, bottom=0.04,
        wspace=0.08,
    )

    # Left column: panels proportionally sized by content
    left_gs = gridspec.GridSpecFromSubplotSpec(
        len(left_heights), 1,
        subplot_spec=outer[0],
        height_ratios=left_heights,
        hspace=0.35,
    )

    # Right column: equal-height rows, one per FFT test
    right_gs = gridspec.GridSpecFromSubplotSpec(
        max(n_fft, 1), 1,
        subplot_spec=outer[1],
        hspace=0.45,
    )

    return fig, left_gs, right_gs, present_simple, present_fft, test_map


# ── main ──────────────────────────────────────────────────────────────────────

def parse_args():
    parser = argparse.ArgumentParser(
        description="Print a visual report from a JSON audio-test file."
    )
    parser.add_argument("-f", "--file", required=True, metavar="PATH",
                        help="Path to the JSON report file.")
    parser.add_argument("-o", "--output", metavar="PATH", default=None,
                        help="Save the figure to this path "
                             "(format inferred from extension: .png, .pdf, .svg …).")
    parser.add_argument("--no-show", action="store_true",
                        help="Do not open an interactive window.")
    parser.add_argument("--dpi", type=int, default=150,
                        help="DPI for saved images (default: 150).")
    return parser.parse_args()


def load_json(path: str) -> dict:
    p = Path(path)
    if not p.exists():
        print(f"Error: file not found: {path}", file=sys.stderr)
        sys.exit(1)
    with p.open() as fh:
        try:
            return json.load(fh)
        except json.JSONDecodeError as exc:
            print(f"Error: invalid JSON: {exc}", file=sys.stderr)
            sys.exit(1)


def render_fft_only(fig, gs_slot, test: dict):
    """
    Right-column renderer: just the FFT plot, with the test name as title
    and PASS/FAIL in the subtitle.
    """
    ax = fig.add_subplot(gs_slot)
    fft_data  = test.get("fft", [])
    passed    = test.get("passed", False)
    test_name = test.get("test_name", "")
    ignored   = test.get("ignored_channels", [])

    for i, ch in enumerate(fft_data):
        arr = np.array(ch, dtype=float)
        arr = np.where(arr > 0, arr, np.nan)
        x = make_fft_xaxis(len(arr))
        base = CH_COLORS[i % len(CH_COLORS)]
        color = lighten_color(base) if i in ignored else base
        lw = 0.5 if i in ignored else 1.2
        ax.plot(x, arr, color=color, linewidth=lw, label=f"Ch {i}", alpha=0.88)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(20, 22050)
    ax.set_xlabel("Frequency (Hz)", fontsize=8)
    ax.set_ylabel("Amplitude", fontsize=8)
    ax.grid(True, which="both", linestyle="--", linewidth=0.35, alpha=0.5)
    ax.legend(fontsize=7, loc="upper right")
    ax.tick_params(labelsize=7)
    _set_freq_xticks(ax)

    badge, badge_color = pass_label(passed)
    ax.set_title(f"{test_name}  –  {badge}", fontsize=10, fontweight="bold",
                 color=badge_color if not passed else "black",
                 loc="left")
    # Colour just the badge part by using two title calls
    ax.set_title(f"{test_name}", fontsize=10, fontweight="bold", loc="left")
    ax.text(1.0, 1.01, badge, transform=ax.transAxes, fontsize=9,
            fontweight="bold", color=badge_color, ha="right", va="bottom")


def main():
    args = parse_args()
    data = load_json(args.file)

    fig, left_gs, right_gs, present_simple, present_fft, test_map = build_layout(data)

    # ── Left column ───────────────────────────────────────────────────────────
    render_main(fig, left_gs[0], data)

    for row_idx, name in enumerate(present_simple, start=1):
        test = test_map[name]
        if name == "Round-trip delay test":
            render_round_trip(fig, left_gs[row_idx], test)
        elif name == "Background noise test":
            render_noise(fig, left_gs[row_idx], test)
        elif name == "Mute test":
            render_mute(fig, left_gs[row_idx], test)
        elif name == "Level and crosstalk test":
            render_level_crosstalk(fig, left_gs[row_idx], test)

    # ── Right column – one FFT plot per test ──────────────────────────────────
    for row_idx, name in enumerate(present_fft):
        render_fft_only(fig, right_gs[row_idx], test_map[name])

    if args.output:
        fig.savefig(args.output, dpi=args.dpi, bbox_inches="tight")
        print(f"Saved: {args.output}")

    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
