"""Графики по результатам бенчмарков.

Запуск из корня проекта (нужен matplotlib: pip3 install matplotlib):
    python3 benchmarks/plot.py benchmark_results/results.csv

PNG-файлы сохраняются рядом с CSV.
"""

import csv
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # рисуем сразу в файл, окно не нужно
import matplotlib.pyplot as plt

# Цвет закреплён за вариантом, а не за его номером на графике:
# один и тот же указатель на всех графиках одного цвета.
COLORS = {
    "raw": "#52514e", # базовый вариант "без умных указателей" — нейтральный серый
    "UnqPtr": "#2a78d6",
    "std::unique_ptr": "#eb6834",
    "ShrdPtr": "#1baf7a",
    "std::shared_ptr": "#eda100",
    "std::make_shared": "#e87ba4",
    "MsPtr ++": "#008300",
    "MsPtr[i]": "#4a3aa7",
}
# Второй признак помимо цвета — форма маркера 
MARKERS = {
    "raw": "o", "UnqPtr": "s", "std::unique_ptr": "D", "ShrdPtr": "^",
    "std::shared_ptr": "v", "std::make_shared": "P", "MsPtr ++": "X", "MsPtr[i]": "*",
}

TITLES = {
    "create": "Создание и удаление N объектов",
    "access": "Чтение *ptr через N указателей",
    "copy": "Копирование указателя N раз (счётчик ссылок +1 / −1)",
    "traverse": "Обход массива из N int",
}

TEXT = "#0b0b0b"
MUTED = "#52514e"
GRID = "#e4e3df"


def read_results(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as file:
        for row in csv.DictReader(file):
            for key in ("count", "allocations", "heap_bytes", "handle_bytes"):
                row[key] = int(row[key])
            for key in ("total_ms", "ns_per_op"):
                row[key] = float(row[key])
            rows.append(row)
    return rows


def style_axes(axes):
    axes.grid(True, color=GRID, linewidth=0.8)
    axes.set_axisbelow(True)
    for side in ("top", "right"):
        axes.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        axes.spines[side].set_color(MUTED)
    axes.tick_params(colors=MUTED)
    axes.xaxis.label.set_color(TEXT)
    axes.yaxis.label.set_color(TEXT)
    axes.title.set_color(TEXT)


def group_by_variant(rows, scenario):
    series = defaultdict(list)  # вариант -> [(N, строка)], в порядке появления в CSV
    for row in rows:
        if row["scenario"] == scenario:
            series[row["variant"]].append((row["count"], row))
    return series


def plot_lines(series, value, ylabel, title, path, log_y=False):
    figure, axes = plt.subplots(figsize=(9, 5.5), dpi=150)
    for variant, points in series.items():
        xs = [count for count, _ in points]
        ys = [value(row) for _, row in points]
        axes.plot(xs, ys, label=variant, color=COLORS.get(variant, MUTED),
                  marker=MARKERS.get(variant, "o"), markersize=7, linewidth=2,
                  linestyle="--" if variant == "raw" else "-")
    axes.set_xscale("log")
    if log_y:
        axes.set_yscale("log")
    else:
        axes.set_ylim(bottom=0)
    axes.set_xlabel("N (логарифмическая шкала)")
    axes.set_ylabel(ylabel)
    axes.set_title(title, fontsize=13, loc="left")
    style_axes(axes)
    axes.legend(frameon=False, labelcolor=TEXT)
    figure.tight_layout()
    figure.savefig(path)
    plt.close(figure)
    print("  " + str(path))


def plot_memory_per_object(rows, path):
    series = group_by_variant(rows, "create")
    variants = list(series.keys())
    largest = [points[-1][1] for points in series.values()]  # строка с самым большим N
    heap = [row["heap_bytes"] / row["count"] for row in largest]
    handle = [row["handle_bytes"] / row["count"] for row in largest]

    figure, axes = plt.subplots(figsize=(9, 4.8), dpi=150)
    positions = range(len(variants))
    axes.barh(positions, heap, color="#2a78d6", height=0.6, label="объект и счётчик в куче")
    axes.barh(positions, handle, left=heap, color="#c3c2b7", height=0.6,
              edgecolor="white", linewidth=2, label="сам указатель (sizeof)")
    for position, h, s in zip(positions, heap, handle):
        axes.text(h + s + 0.5, position, f"{h + s:.0f} Б", va="center", color=TEXT, fontsize=10)
    axes.set_yticks(list(positions))
    axes.set_yticklabels(variants)
    axes.invert_yaxis()
    axes.set_xlabel("байт на один объект int (запрошено у operator new)")
    axes.set_title("Память на один объект", fontsize=13, loc="left")
    style_axes(axes)
    axes.grid(True, axis="x", color=GRID)
    axes.grid(False, axis="y")
    axes.set_xlim(0, max(h + s for h, s in zip(heap, handle)) * 1.15)  # место для подписей справа
    axes.legend(frameon=False, labelcolor=TEXT, ncol=2, loc="upper left", bbox_to_anchor=(0, -0.15))
    figure.tight_layout()
    figure.savefig(path)
    plt.close(figure)
    print("  " + str(path))


def main():
    if len(sys.argv) != 2:
        print("Использование: python3 benchmarks/plot.py benchmark_results/results.csv")
        return 1
    csv_path = Path(sys.argv[1])
    rows = read_results(csv_path)
    output_dir = csv_path.parent
    print("Графики:")

    for scenario, title in TITLES.items():
        series = group_by_variant(rows, scenario)
        if series:
            plot_lines(series, lambda row: row["ns_per_op"], "наносекунд на объект / операцию",
                       title, output_dir / f"time_{scenario}.png")

    create = group_by_variant(rows, "create")
    if create:
        plot_lines(create, lambda row: (row["heap_bytes"] + row["handle_bytes"]) / 2**20,
                   "мегабайт (логарифмическая шкала)", "Память на N объектов (куча + указатели)",
                   output_dir / "memory_create.png", log_y=True)
        plot_memory_per_object(rows, output_dir / "memory_per_object.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
