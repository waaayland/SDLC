#!/usr/bin/env python3
"""
Генератор монохромных UML 2.5 диаграмм динамического поведения ПО UAVLog 1.0
в строгом соответствии с требованиями СТП 01-2024 (БГУИР):
- чёрно-белая палитра (без цветных заливок, высокая контрастность при печати);
- шрифт Times New Roman, 300 DPI;
- стандартные рамки UML 2.5 без дублирования подрисуночной подписи;
- 100 % синхронизация с текстовыми моделями docs/uml/models/*.puml и кодом project/UAVLog/.
"""

from pathlib import Path
import shutil
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import (
    Circle, Ellipse, FancyArrowPatch, FancyBboxPatch, Polygon, Rectangle
)

plt.rcParams["font.family"] = "Times New Roman"
plt.rcParams["mathtext.fontset"] = "stix"
plt.rcParams["axes.unicode_minus"] = False

ROOT = Path(__file__).resolve().parent
IMAGES_DIR = ROOT / "images"
SDD_IMAGES_DIR = ROOT.parent.parent / "sdd" / "images"
LAB5_SCREENS_DIR = Path(r"e:\01_BSUIR\03_KURS\05_SEM\ЖЦРПО\02_ЛАБЫ_СДАЧА\05_LAB\screens")

BLACK = "#000000"
WHITE = "#FFFFFF"
LIGHT_GRAY = "#F2F2F2"
MID_GRAY = "#E6E6E6"


def setup_canvas(w_in: float, h_in: float, xlim, ylim):
    fig, ax = plt.subplots(figsize=(w_in, h_in), dpi=300)
    ax.set_xlim(xlim)
    ax.set_ylim(ylim)
    ax.axis("off")
    fig.patch.set_facecolor(WHITE)
    ax.set_facecolor(WHITE)
    return fig, ax


def draw_uml_frame(ax, x0, y0, x1, y1, header_text, header_w, header_h=0.46, fontsize=10.2):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, edgecolor=BLACK, lw=1.3, zorder=1))
    notch = 0.22
    pts = [
        (x0, y1),
        (x0 + header_w, y1),
        (x0 + header_w, y1 - header_h + notch),
        (x0 + header_w - notch, y1 - header_h),
        (x0, y1 - header_h),
    ]
    ax.add_patch(Polygon(pts, closed=True, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=1.1, zorder=2))
    ax.text(x0 + 0.14, y1 - header_h / 2.0, header_text,
            ha="left", va="center", fontsize=fontsize, fontweight="bold", color=BLACK, zorder=3)


def draw_actor(ax, x, y, label, scale=1.0, fontsize=8.5):
    r = 0.14 * scale
    ax.add_patch(Circle((x, y + 0.34 * scale), r, facecolor=WHITE, edgecolor=BLACK, lw=1.1, zorder=4))
    ax.plot([x, x], [y + 0.20 * scale, y - 0.10 * scale], color=BLACK, lw=1.1, zorder=4)
    ax.plot([x - 0.20 * scale, x + 0.20 * scale], [y + 0.10 * scale, y + 0.10 * scale], color=BLACK, lw=1.1, zorder=4)
    ax.plot([x, x - 0.17 * scale], [y - 0.10 * scale, y - 0.36 * scale], color=BLACK, lw=1.1, zorder=4)
    ax.plot([x, x + 0.17 * scale], [y - 0.10 * scale, y - 0.36 * scale], color=BLACK, lw=1.1, zorder=4)
    ax.text(x, y - 0.44 * scale, label, ha="center", va="top", fontsize=fontsize, color=BLACK,
            linespacing=1.08, bbox=dict(fc=WHITE, ec="none", pad=0.4), zorder=6)


def draw_note(ax, x, y, w, h, text, fontsize=8.0):
    fold = 0.20
    pts = [(x, y), (x + w, y), (x + w, y + h - fold), (x + w - fold, y + h), (x, y + h)]
    ax.add_patch(Polygon(pts, closed=True, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=0.95, zorder=3))
    fold_pts = [(x + w - fold, y + h), (x + w - fold, y + h - fold), (x + w, y + h - fold)]
    ax.add_patch(Polygon(fold_pts, closed=True, facecolor=MID_GRAY, edgecolor=BLACK, lw=0.85, zorder=4))
    ax.text(x + 0.12, y + h / 2.0, text, ha="left", va="center", fontsize=fontsize, color=BLACK,
            linespacing=1.18, zorder=5)


def draw_arrow(ax, p1, p2, style="-|>", ls="-", lw=1.0, rad=0.0, ms=9.5, facecolor=BLACK, zorder=3):
    arr = FancyArrowPatch(
        p1, p2,
        arrowstyle=style,
        linestyle=ls,
        linewidth=lw,
        facecolor=facecolor,
        edgecolor=BLACK,
        mutation_scale=ms,
        connectionstyle=f"arc3,rad={rad}",
        shrinkA=0,
        shrinkB=0,
        zorder=zorder,
    )
    ax.add_patch(arr)
    return arr


def draw_save(fig, name: str):
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    out = IMAGES_DIR / name
    fig.savefig(out, dpi=300, bbox_inches="tight", pad_inches=0.08, facecolor=WHITE)
    plt.close(fig)
    if SDD_IMAGES_DIR.exists():
        shutil.copy2(out, SDD_IMAGES_DIR / f"uml-{name}")
    LAB5_SCREENS_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(out, LAB5_SCREENS_DIR / f"uml-{name}")
    print(f"Saved {name} ({out.stat().st_size} bytes)")


# =============================================================================
# 1. ДИАГРАММА ВАРИАНТОВ ИСПОЛЬЗОВАНИЯ (01-use-case.png)
# =============================================================================
def render_use_case():
    fig, ax = setup_canvas(12.2, 9.8, (0, 14.2), (0, 11.4))

    draw_uml_frame(ax, 3.75, 0.35, 12.15, 11.15,
                   "uc  ПО «UAVLog 1.0» (АРМ службы эксплуатации парка БЛА)", 6.50, 0.48, fontsize=10.2)

    draw_actor(ax, 0.85, 9.75, "Аутентифицированный\nпользователь", scale=0.88, fontsize=8.3)
    draw_actor(ax, 2.35, 8.25, "Администратор\n(Нач. службы)", scale=0.85, fontsize=8.3)
    draw_actor(ax, 2.35, 6.25, "Наблюдатель\n(Руководитель)", scale=0.85, fontsize=8.3)
    draw_actor(ax, 2.35, 4.45, "Оператор\n(Оператор БЛА)", scale=0.85, fontsize=8.3)
    draw_actor(ax, 2.35, 2.55, "Техник\n(Техник по ТО)", scale=0.85, fontsize=8.3)
    draw_actor(ax, 1.35, 1.00, "Таймер /\nСистемные часы", scale=0.85, fontsize=8.3)

    draw_actor(ax, 13.20, 6.25, "Принтер\n(Спулер Windows)", scale=0.85, fontsize=8.3)

    # Шина обобщения (generalization)
    bus_x = 0.85
    ax.plot([bus_x, bus_x], [2.55, 8.92], color=BLACK, lw=1.0, zorder=2)
    for ay in [8.25, 6.25, 4.45, 2.55]:
        ax.plot([bus_x, 1.98], [ay, ay], color=BLACK, lw=1.0, zorder=2)
    tri = Polygon([(bus_x, 9.03), (bus_x - 0.11, 8.83), (bus_x + 0.11, 8.83)],
                  closed=True, facecolor=WHITE, edgecolor=BLACK, lw=1.05, zorder=5)
    ax.add_patch(tri)

    ucs = {}

    def add_uc(key, cx, cy, w, h, text, fill=WHITE):
        ax.add_patch(Ellipse((cx, cy), w, h, facecolor=fill, edgecolor=BLACK, lw=1.05, zorder=3))
        ax.text(cx, cy, text, ha="center", va="center", fontsize=8.1, color=BLACK, linespacing=1.08, zorder=4)
        ucs[key] = (cx, cy, w / 2.0, h / 2.0)

    add_uc("UC01", 5.90, 10.10, 3.35, 0.66, "UC-01: Войти в систему")
    add_uc("UC08", 5.90, 9.20, 3.55, 0.72, "UC-08: Просмотреть формуляр\nи состояние парка")
    add_uc("UC02", 5.90, 8.20, 3.70, 0.78, "UC-02: Вести справочники парка,\nрегламентов ТО и чек-листов")
    add_uc("UC03", 5.90, 7.22, 3.60, 0.74, "UC-03: Вести реестр операторов\nи сроков допуска")
    add_uc("UC09", 5.90, 6.25, 3.60, 0.74, "UC-09: Сформировать отчёт /\nАкт готовности (CSV/PDF)")
    add_uc("UC05", 5.90, 5.25, 3.35, 0.68, "UC-05: Зарегистрировать полёт")
    add_uc("UC04", 5.90, 4.30, 3.60, 0.74, "UC-04: Провести предполётный\nосмотр по чек-листу (F-15)")
    add_uc("UC07", 5.90, 3.32, 3.60, 0.74, "UC-07: Зарегистрировать /\nустранить неисправность")
    add_uc("UC06", 5.90, 2.38, 3.50, 0.70, "UC-06: Зарегистрировать\nвыполненное ТО")
    add_uc("UC10", 5.90, 1.48, 3.60, 0.72, "UC-10: Резервное копирование\nи восстановление БД")
    add_uc("UC12", 5.90, 0.72, 3.75, 0.60, "UC-12: Автоблокировка (15 мин)\nи календарные сроки ТО")

    add_uc("UC08a", 10.35, 9.20, 2.95, 0.72, "UC-08a: Списать борт\nв архив (R/O)", fill=LIGHT_GRAY)
    add_uc("UC11",  10.35, 7.70, 3.10, 0.80, "UC-11: Зафиксировать событие\nв журнале аудита", fill=LIGHT_GRAY)
    add_uc("UC09a", 10.35, 6.25, 2.95, 0.72, "UC-09a: Распечатать\nдокумент на A4", fill=LIGHT_GRAY)
    add_uc("UC05a", 10.35, 4.95, 3.10, 0.76, "UC-05a: Проверить 5-факторный\nдопуск к полёту", fill=LIGHT_GRAY)
    add_uc("UC05b", 10.35, 2.45, 3.15, 0.82, "UC-05b: Пересчитать налёт,\nциклы АКБ и сроки ТО", fill=LIGHT_GRAY)
    add_uc("UC05c", 10.35, 1.05, 3.15, 0.78, "UC-05c: Сформировать\nпредупреждения (ТО/допуски/АКБ)", fill=LIGHT_GRAY)

    def left_pt(key, dy=0.0):
        cx, cy, rx, _ = ucs[key]
        return (cx - rx, cy + dy)

    def right_pt(key, dy=0.0):
        cx, cy, rx, _ = ucs[key]
        return (cx + rx, cy + dy)

    def top_pt(key):
        cx, cy, _, ry = ucs[key]
        return (cx, cy + ry)

    def bot_pt(key):
        cx, cy, _, ry = ucs[key]
        return (cx, cy - ry)

    # Коммуникации: все линии от Администратора выходят на уровне плеч/туловища (y=8.08..8.45), не задевая подпись!
    draw_arrow(ax, (1.12, 9.95), left_pt("UC01"), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (1.12, 9.65), left_pt("UC08"), style="->", lw=1.0, ms=9)

    # Администратор -> UC-02, UC-03, UC-09, UC-10, UC-08a
    draw_arrow(ax, (2.68, 8.28), left_pt("UC02"), style="->", lw=1.0, ms=9)
    ax.plot([2.68, 3.22, 3.22], [8.20, 8.20, 7.22], color=BLACK, lw=0.95, zorder=2)
    draw_arrow(ax, (3.22, 7.22), left_pt("UC03"), style="->", lw=0.95, ms=9)
    ax.plot([3.22, 3.22], [7.22, 6.40], color=BLACK, lw=0.95, zorder=2)
    draw_arrow(ax, (3.22, 6.40), left_pt("UC09", 0.14), style="->", lw=0.95, ms=9)
    ax.plot([3.22, 3.44, 3.44], [6.40, 6.40, 1.60], color=BLACK, lw=0.9, zorder=2)
    draw_arrow(ax, (3.44, 1.60), left_pt("UC10", 0.12), style="->", lw=0.9, ms=9)
    ax.plot([2.68, 3.56, 3.56, 10.35], [8.44, 8.80, 10.60, 10.60], color=BLACK, lw=0.9, zorder=2)
    draw_arrow(ax, (10.35, 10.60), top_pt("UC08a"), style="->", lw=0.9, ms=9)

    draw_arrow(ax, (2.68, 6.25), left_pt("UC09", -0.05), style="->", lw=1.0, ms=9)

    draw_arrow(ax, (2.68, 4.62), left_pt("UC05"), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (2.68, 4.45), left_pt("UC04", 0.08), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (2.68, 4.28), left_pt("UC07", 0.10), style="->", lw=1.0, ms=9)

    draw_arrow(ax, (2.68, 2.75), left_pt("UC04", -0.10), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (2.68, 2.60), left_pt("UC07", -0.08), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (2.68, 2.45), left_pt("UC06"), style="->", lw=1.0, ms=9)

    draw_arrow(ax, (1.68, 1.15), left_pt("UC10", -0.08), style="->", lw=1.0, ms=9)
    draw_arrow(ax, (1.68, 0.90), left_pt("UC12"), style="->", lw=1.0, ms=9)

    draw_arrow(ax, right_pt("UC09a"), (12.88, 6.25), style="->", lw=1.05, ms=10)

    # <<extend>> с чистыми белыми подложками текста
    draw_arrow(ax, left_pt("UC08a"), right_pt("UC08"), style="->", ls="--", lw=0.95, ms=9)
    ax.text(8.42, 9.33, "«extend»", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    draw_arrow(ax, left_pt("UC09a"), right_pt("UC09"), style="->", ls="--", lw=0.95, ms=9)
    ax.text(8.42, 6.37, "«extend»", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    draw_arrow(ax, top_pt("UC05c"), bot_pt("UC05b"), style="->", ls="--", lw=0.95, ms=9)
    ax.text(10.50, 1.75, "«extend»", ha="left", va="center", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    # <<include>> UC-05 -> UC-05a
    draw_arrow(ax, right_pt("UC05"), left_pt("UC05a", 0.08), style="->", ls="--", lw=0.95, ms=9)
    ax.text(8.42, 5.26, "«include»", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    # Шина включений в UC-05b
    b_bus = 8.22
    ax.plot([right_pt("UC05", -0.14)[0], b_bus, b_bus], [5.11, 5.11, 2.45], color=BLACK, ls="--", lw=0.9, zorder=2)
    ax.plot([right_pt("UC06")[0], b_bus], [2.38, 2.38], color=BLACK, ls="--", lw=0.9, zorder=2)
    ax.plot([right_pt("UC12")[0], b_bus, b_bus], [0.72, 0.72, 2.38], color=BLACK, ls="--", lw=0.9, zorder=2)
    draw_arrow(ax, (b_bus, 2.45), left_pt("UC05b"), style="->", ls="--", lw=0.95, ms=9)
    ax.text(8.50, 2.58, "«include»", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    # Шина включений в UC-11
    a_bus = 7.95
    ax.plot([a_bus, a_bus], [1.48, 10.10], color=BLACK, ls="--", lw=0.85, zorder=2)
    for k in ["UC01", "UC02", "UC03", "UC04", "UC07", "UC10"]:
        rx, ry = right_pt(k)
        ax.plot([rx, a_bus], [ry, ry], color=BLACK, ls="--", lw=0.85, zorder=2)
    ax.plot([right_pt("UC05", 0.14)[0], a_bus], [5.39, 5.39], color=BLACK, ls="--", lw=0.85, zorder=2)
    ax.plot([right_pt("UC06", 0.12)[0], a_bus], [2.50, 2.50], color=BLACK, ls="--", lw=0.85, zorder=2)
    draw_arrow(ax, (a_bus, 7.70), left_pt("UC11"), style="->", ls="--", lw=1.0, ms=9)
    ax.text(8.42, 7.82, "«include»", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    ax.text(
        7.1, 0.10,
        "Обозначения: сплошная стрелка — коммуникация (от инициатора); пустой треугольник — обобщение актёров; "
        "пунктир «include» — обязательное включение; «extend» — условное расширение.",
        ha="center", va="center", fontsize=8.0, style="italic", color=BLACK,
    )

    draw_save(fig, "01-use-case.png")


# =============================================================================
# 2. ДИАГРАММА АКТИВНОСТИ (02-activity-flight-registration.png)
# =============================================================================
def render_activity():
    fig, ax = setup_canvas(12.2, 11.2, (0, 13.4), (0, 12.6))

    draw_uml_frame(ax, 0.25, 0.25, 13.15, 12.35,
                   "act  Регистрация полёта и 5-факторный контроль допуска (UAVLog 1.0)", 7.45, 0.46, fontsize=10.2)

    def action_box(cx, cy, w, h, text, fontsize=8.2, fill=WHITE):
        ax.add_patch(FancyBboxPatch(
            (cx - w / 2.0, cy - h / 2.0), w, h,
            boxstyle="round,pad=0.04,rounding_size=0.18",
            facecolor=fill, edgecolor=BLACK, lw=1.05, zorder=3))
        ax.text(cx, cy, text, ha="center", va="center", fontsize=fontsize, color=BLACK,
                linespacing=1.14, zorder=4)

    def diamond(cx, cy, rx, ry, text, fontsize=7.9):
        pts = [(cx, cy + ry), (cx + rx, cy), (cx, cy - ry), (cx - rx, cy)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=1.05, zorder=3))
        ax.text(cx, cy, text, ha="center", va="center", fontsize=fontsize, color=BLACK,
                linespacing=1.06, zorder=4)

    def final_node(cx, cy):
        ax.add_patch(Circle((cx, cy), 0.16, facecolor=WHITE, edgecolor=BLACK, lw=1.15, zorder=4))
        ax.add_patch(Circle((cx, cy), 0.095, facecolor=BLACK, edgecolor=BLACK, lw=0.5, zorder=5))

    # Начальный узел
    ax.add_patch(Circle((5.45, 11.55), 0.14, facecolor=BLACK, edgecolor=BLACK, zorder=4))
    draw_arrow(ax, (5.45, 11.41), (5.45, 11.06), style="->", lw=1.0, ms=9)

    action_box(5.45, 10.66, 7.60, 0.72,
               "Оператор открывает диалог «Регистрация полёта» (Tab / Enter / Esc),\n"
               "выбирает борт, оператора, исправную АКБ, дату, время взлёта/посадки и задачу")
    draw_arrow(ax, (5.45, 10.30), (5.45, 9.90), style="->", lw=1.0, ms=9)

    action_box(5.45, 9.60, 4.90, 0.52, "Авторасчёт длительности: duration = посадка − взлёт")
    draw_arrow(ax, (5.45, 9.34), (5.45, 9.00), style="->", lw=1.0, ms=9)

    # Решение 1: длительность
    diamond(5.45, 8.60, 1.65, 0.40, "0 < duration ≤ 720 мин?")
    ax.plot([3.80, 1.80, 1.80], [8.60, 8.60, 7.82], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (1.80, 7.95), (1.80, 7.78), style="->", lw=1.0, ms=9)
    ax.text(2.82, 8.70, "[нет]", ha="center", va="bottom", fontsize=8.0, style="italic")
    action_box(1.80, 7.42, 2.75, 0.68, "Вывести «Ошибка времени»;\nзаблокировать «Сохранить»")
    draw_arrow(ax, (1.80, 7.08), (1.80, 6.72), style="->", lw=1.0, ms=9)
    final_node(1.80, 6.54)

    draw_arrow(ax, (5.45, 8.20), (5.45, 7.80), style="->", lw=1.0, ms=9)
    ax.text(5.58, 8.00, "[да]", ha="left", va="center", fontsize=8.0, style="italic")

    action_box(5.45, 7.44, 4.20, 0.66,
               "Выполнить 5-факторную проверку допуска\nAdmissionService::evaluate()  {≤ 200 мс}",
               fill=LIGHT_GRAY)

    draw_note(ax, 8.45, 7.15, 4.35, 1.62,
              "5 факторов допуска (REQ-FUNC-006-3, F-15):\n"
              "1) uavOk: статус борта == «Исправен»\n"
              "2) maintOk: нет просроченных видов ТО\n"
              "3) operatorOk: допуск действует и на тип БЛА\n"
              "4) batteryOk: циклы АКБ < max_cycles\n"
              "5) preflightOk: осмотр F-15 на дату пройден",
              fontsize=7.9)
    ax.plot([7.55, 8.45], [7.44, 7.75], color=BLACK, ls="--", lw=0.9, zorder=2)

    draw_arrow(ax, (5.45, 7.11), (5.45, 6.74), style="->", lw=1.0, ms=9)

    # Решение 2: допуск
    diamond(5.45, 6.34, 1.72, 0.40, "Все 5 факторов выполнены?")

    # [нет — отказ] влево
    ax.plot([3.73, 2.35, 2.35], [6.34, 6.34, 5.62], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (2.35, 5.75), (2.35, 5.56), style="->", lw=1.0, ms=9)
    ax.text(3.04, 6.44, "[нет — отказ]", ha="center", va="bottom", fontsize=8.0, style="italic")

    action_box(2.35, 5.20, 3.60, 0.68,
               "Показать причины отказа (result.reasons[]);\nкнопка «Сохранить» неактивна")
    draw_arrow(ax, (2.35, 4.86), (2.35, 4.46), style="->", lw=1.0, ms=9)

    diamond(2.35, 4.08, 1.48, 0.38, "Нет осмотра F-15?")
    draw_arrow(ax, (2.35, 3.70), (2.35, 3.24), style="->", lw=1.0, ms=9)
    ax.text(2.47, 3.48, "[да]", ha="left", va="center", fontsize=8.0, style="italic")
    action_box(1.90, 2.88, 2.95, 0.64, "Перейти к диалогу\n«Предполётный осмотр» (UC-04)")
    draw_arrow(ax, (1.90, 2.56), (1.90, 2.18), style="->", lw=1.0, ms=9)
    final_node(1.90, 2.00)

    ax.plot([3.83, 4.72, 4.72], [4.08, 4.08, 3.24], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (4.72, 3.40), (4.72, 3.24), style="->", lw=1.0, ms=9)
    ax.text(4.28, 4.18, "[нет]", ha="center", va="bottom", fontsize=8.0, style="italic")
    action_box(4.72, 2.88, 2.15, 0.64, "Отменить ввод (Esc);\nуведомить техника")
    draw_arrow(ax, (4.72, 2.56), (4.72, 2.18), style="->", lw=1.0, ms=9)
    final_node(4.72, 2.00)

    # [да — допущен] вправо
    ax.plot([7.17, 9.40, 9.40], [6.34, 6.34, 5.90], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (9.40, 6.02), (9.40, 5.82), style="->", lw=1.0, ms=9)
    ax.text(8.30, 6.44, "[да — допущен]", ha="center", va="bottom", fontsize=8.0, style="italic")

    action_box(9.40, 5.48, 5.20, 0.62,
               "Активировать «Сохранить» (Enter); проверить права RBAC\n(RbacGuard::requirePermission(RegisterFlight))")
    draw_arrow(ax, (9.40, 5.17), (9.40, 4.86), style="->", lw=1.0, ms=9)

    action_box(9.40, 4.56, 5.20, 0.56,
               "Открыть транзакцию SQLite WAL (BEGIN IMMEDIATE)\nи выполнить INSERT INTO flights")
    draw_arrow(ax, (9.40, 4.28), (9.40, 4.00), style="->", lw=1.0, ms=9)

    # FORK / JOIN
    ax.plot([6.15, 12.65], [3.98, 3.98], color=BLACK, lw=3.2, solid_capstyle="butt", zorder=4)
    fork_xs = [6.96, 8.58, 10.22, 11.84]
    fork_texts = [
        "Пересчёт налёта\nSUM(duration_min)",
        "Пересчёт циклов\nАКБ (COUNT)",
        "Расчёт остатков\nдо ТО (Engine)",
        "Запись события\nв audit_log",
    ]
    for fx, ft in zip(fork_xs, fork_texts):
        draw_arrow(ax, (fx, 3.96), (fx, 3.60), style="->", lw=0.95, ms=8)
        action_box(fx, 3.28, 1.48, 0.58, ft, fontsize=7.6)
        draw_arrow(ax, (fx, 2.99), (fx, 2.66), style="->", lw=0.95, ms=8)

    ax.plot([6.15, 12.65], [2.64, 2.64], color=BLACK, lw=3.2, solid_capstyle="butt", zorder=4)
    draw_arrow(ax, (9.10, 2.62), (9.10, 2.34), style="->", lw=1.0, ms=9)

    # Решение 3: порог ТО
    diamond(9.10, 2.02, 1.48, 0.32, "Остаток до ТО ≤ 0?")
    draw_arrow(ax, (10.58, 2.02), (11.20, 2.02), style="->", lw=0.95, ms=8)
    ax.text(10.86, 2.10, "[да]", ha="center", va="bottom", fontsize=7.8, style="italic")
    action_box(12.06, 2.02, 1.66, 0.56, "UavStateMachine:\n→ Требуется ТО", fontsize=7.7)
    ax.plot([12.06, 12.06, 10.48], [1.74, 1.28, 1.28], color=BLACK, lw=0.95, zorder=2)

    draw_arrow(ax, (9.10, 1.70), (9.10, 1.56), style="->", lw=1.0, ms=8)
    ax.text(9.20, 1.63, "[нет]", ha="left", va="center", fontsize=7.8, style="italic")

    # Решение 4: транзакция OK?
    diamond(9.10, 1.28, 1.38, 0.28, "Транзакция OK?")

    # [да] COMMIT
    ax.plot([7.72, 6.20], [1.28, 1.28], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (6.20, 1.28), (6.20, 1.00), style="->", lw=1.0, ms=8)
    ax.text(7.05, 1.36, "[да]", ha="center", va="bottom", fontsize=7.8, style="italic")
    action_box(6.20, 0.68, 3.25, 0.56, "COMMIT WAL; обновить таблицу\nпарка и предупреждения (UC-05c)", fontsize=7.9)
    draw_arrow(ax, (4.57, 0.68), (4.16, 0.68), style="->", lw=1.0, ms=8)
    final_node(3.96, 0.68)

    # [нет — сбой] ROLLBACK
    ax.plot([10.48, 11.45], [1.28, 1.28], color=BLACK, lw=1.0, zorder=2)
    draw_arrow(ax, (11.45, 1.28), (11.45, 1.00), style="->", lw=1.0, ms=8)
    ax.text(11.12, 1.36, "[нет — сбой]", ha="center", va="bottom", fontsize=7.8, style="italic",
            bbox=dict(fc=WHITE, ec="none", pad=0.4), zorder=6)
    action_box(11.45, 0.68, 2.85, 0.56, "ROLLBACK (REQ-REL-001);\nзапись в uavlog.log; ошибка", fontsize=7.9)
    draw_arrow(ax, (10.02, 0.68), (9.62, 0.68), style="->", lw=1.0, ms=8)
    final_node(9.42, 0.68)

    draw_save(fig, "02-activity-flight-registration.png")


# =============================================================================
# 3. ДИАГРАММА СОСТОЯНИЙ (03-state-uav-lifecycle.png)
# =============================================================================
def render_state_machine():
    fig, ax = setup_canvas(12.2, 9.6, (0, 14.0), (0, 11.2))

    draw_uml_frame(ax, 0.25, 0.25, 13.75, 10.95,
                   "state machine  UavLifecycle — Жизненный цикл борта БЛА (UAVLog 1.0)", 7.20, 0.46, fontsize=10.2)

    ax.add_patch(Circle((7.0, 10.15), 0.14, facecolor=BLACK, edgecolor=BLACK, zorder=4))
    draw_arrow(ax, (7.0, 10.01), (7.0, 9.35), style="->", lw=1.1, ms=10)
    ax.text(7.16, 9.72, "Регистрация борта в реестре\n/ state = Serviceable; audit_log()",
            ha="left", va="center", fontsize=8.3, color=BLACK, linespacing=1.12)

    ax.add_patch(FancyBboxPatch(
        (0.65, 2.70), 12.70, 6.62,
        boxstyle="round,pad=0.02,rounding_size=0.25",
        facecolor=WHITE, edgecolor=BLACK, lw=1.25, zorder=2))
    ax.plot([0.65, 13.35], [8.86, 8.86], color=BLACK, lw=0.95, zorder=3)
    ax.text(7.0, 9.09, "В эксплуатации (Operational)", ha="center", va="center",
            fontsize=9.6, fontweight="bold", color=BLACK, zorder=4)

    ax.add_patch(Circle((3.25, 8.55), 0.11, facecolor=BLACK, edgecolor=BLACK, zorder=4))
    draw_arrow(ax, (3.25, 8.44), (3.25, 8.05), style="->", lw=1.0, ms=9)

    def state_box(x, y, w, h, title, body, fill=WHITE):
        ax.add_patch(FancyBboxPatch(
            (x, y), w, h,
            boxstyle="round,pad=0.02,rounding_size=0.20",
            facecolor=fill, edgecolor=BLACK, lw=1.15, zorder=3))
        ax.plot([x, x + w], [y + h - 0.40, y + h - 0.40], color=BLACK, lw=0.85, zorder=4)
        ax.text(x + w / 2.0, y + h - 0.20, title, ha="center", va="center",
                fontsize=8.9, fontweight="bold", color=BLACK, zorder=5)
        ax.text(x + 0.14, y + (h - 0.40) / 2.0, body, ha="left", va="center",
                fontsize=7.9, color=BLACK, linespacing=1.20, zorder=5)

    state_box(1.05, 6.55, 4.45, 1.48, "Исправен (Serviceable)",
              "entry / разрешить допуск (uavOk = true)\n"
              "do / учёт полётов, осмотров и порогов ТО\n"
              "exit / зафиксировать смену статуса в audit_log",
              fill=LIGHT_GRAY)

    state_box(8.50, 6.55, 4.45, 1.48, "Требуется ТО (DueMaintenance)",
              "entry / заблокировать вылет (maintOk = false)\n"
              "do / критичное предупреждение на панели\n"
              "exit / обновить индикатор в реестре")

    state_box(1.05, 3.05, 4.45, 1.48, "На ТО (InMaintenance)",
              "entry / заблокировать вылет (uavOk = false)\n"
              "do / выполнение работ регламента ТО\n"
              "exit / обнулить наработку по виду ТО")

    state_box(8.50, 3.05, 4.45, 1.48, "Неисправен (Faulty)",
              "entry / заблокировать вылет (uavOk = false)\n"
              "do / ожидание ремонта и отметки устранения\n"
              "exit / проверить отсутствие открытых отказов")

    draw_arrow(ax, (5.50, 7.52), (8.50, 7.52), style="->", lw=1.05, ms=10)
    ax.text(7.00, 7.60, "Порог ТО достигнут\n[remHours ≤ 0 || remDays ≤ 0]\n/ создать предупреждение",
            ha="center", va="bottom", fontsize=7.7, linespacing=1.08)

    draw_arrow(ax, (2.15, 6.55), (2.15, 4.53), style="->", lw=1.05, ms=10)
    ax.text(2.05, 5.56, "Начать плановое ТО\n[Техник | Админ]",
            ha="right", va="center", fontsize=7.6, linespacing=1.08)

    draw_arrow(ax, (3.40, 4.53), (3.40, 6.55), style="->", lw=1.05, ms=10)
    ax.text(3.50, 5.56, "ТО выполнено\n[нет отказов &&\nнет просроч. ТО]\n/ recalculateUav()",
            ha="left", va="center", fontsize=7.6, linespacing=1.06)

    draw_arrow(ax, (8.50, 6.82), (5.50, 4.35), style="->", lw=1.0, ms=9, rad=0.10)
    ax.text(6.62, 6.00, "Начать просроч. ТО\n[Техник | Админ]", ha="center", va="center",
            fontsize=7.5, linespacing=1.05, bbox=dict(fc=WHITE, ec="none", pad=1.2), zorder=6)

    draw_arrow(ax, (5.50, 4.05), (8.50, 6.55), style="->", lw=1.0, ms=9, rad=0.12)
    ax.text(7.45, 4.98, "ТО выполнено\n[остались иные просроч. ТО]", ha="center", va="center",
            fontsize=7.5, linespacing=1.05, bbox=dict(fc=WHITE, ec="none", pad=1.2), zorder=6)

    draw_arrow(ax, (5.20, 6.55), (8.95, 4.53), style="->", lw=1.0, ms=9, rad=-0.16)
    ax.text(6.95, 6.62, "Крит. неисправность /\nзамечание F-15", ha="center", va="center",
            fontsize=7.4, linespacing=1.04, bbox=dict(fc=WHITE, ec="none", pad=1.0), zorder=6)

    draw_arrow(ax, (5.50, 3.60), (8.50, 3.60), style="->", lw=1.05, ms=10)
    ax.text(7.00, 3.68, "Выявлен крит. отказ на ТО\n/ создать запись в faults",
            ha="center", va="bottom", fontsize=7.6, linespacing=1.06)

    draw_arrow(ax, (11.70, 6.55), (11.70, 4.53), style="->", lw=1.05, ms=10)
    ax.text(11.82, 5.56, "Критичная\nнеисправность\n/ блок. допуск",
            ha="left", va="center", fontsize=7.6, linespacing=1.06)

    draw_arrow(ax, (10.35, 4.53), (10.35, 6.55), style="->", lw=1.05, ms=10)
    ax.text(10.22, 5.56, "Отказы устранены\n[есть просроч. ТО]",
            ha="right", va="center", fontsize=7.5, linespacing=1.06)

    ax.plot([9.60, 9.60, 13.15, 13.15, 4.50, 4.50],
            [4.53, 4.70, 4.70, 8.42, 8.42, 8.03], color=BLACK, lw=0.95, zorder=2)
    draw_arrow(ax, (4.50, 8.18), (4.50, 8.03), style="->", lw=0.95, ms=9)
    ax.text(9.00, 8.50, "Все крит. отказы устранены [указаны работы && ТО в норме] / recalculateUav()",
            ha="center", va="bottom", fontsize=7.6, bbox=dict(fc=WHITE, ec="none", pad=1.0), zorder=6)

    draw_arrow(ax, (5.50, 2.70), (5.50, 1.88), style="->", lw=1.15, ms=10)
    ax.text(5.65, 2.28, "Списание борта [роль = Администратор && подтверждение]\n/ архив R/O; открепить АКБ; audit_log()",
            ha="left", va="center", fontsize=8.0, linespacing=1.12)

    state_box(2.75, 0.58, 5.50, 1.28, "Списан (Retired)",
              "entry / запретить полёты, осмотры, ТО и правки (архив «только чтение»)\n"
              "do / постоянное хранение полной истории эксплуатации борта",
              fill=MID_GRAY)

    draw_arrow(ax, (2.75, 1.22), (1.72, 1.22), style="->", lw=1.1, ms=10)
    ax.add_patch(Circle((1.50, 1.22), 0.18, facecolor=WHITE, edgecolor=BLACK, lw=1.2, zorder=4))
    ax.add_patch(Circle((1.50, 1.22), 0.11, facecolor=BLACK, edgecolor=BLACK, zorder=5))

    draw_note(ax, 9.00, 0.62, 4.35, 1.18,
              "Терминальное состояние (REQ-FUNC-003-2):\n"
              "возврат из «Списан» в эксплуатацию и\n"
              "физическое удаление истории запрещены.",
              fontsize=8.0)
    ax.plot([8.25, 9.00], [1.22, 1.22], color=BLACK, ls="--", lw=0.9, zorder=2)

    draw_save(fig, "03-state-uav-lifecycle.png")


# =============================================================================
# 4. ДИАГРАММА ПОСЛЕДОВАТЕЛЬНОСТИ №1 (04-sequence-flight-registration.png)
# =============================================================================
def render_seq_flight():
    fig, ax = setup_canvas(12.4, 11.4, (0, 14.4), (0, 13.8))

    draw_uml_frame(ax, 0.22, 0.22, 14.18, 13.58,
                   "sd  Регистрация полёта и 5-факторный контроль допуска (UAVLog 1.0)", 7.40, 0.46, fontsize=10.2)

    xs = {
        "Oper": 1.05,
        "UI":   3.15,
        "FS":   5.45,
        "AS":   7.65,
        "Res":  9.65,
        "ME":   11.40,
        "DB":   13.25,
    }
    top_y = 12.50
    bot_y = 0.55

    draw_actor(ax, xs["Oper"], top_y + 0.15, "Оператор\nБЛА", scale=0.78, fontsize=8.1)

    def participant_box(cx, cy, w, h, text):
        ax.add_patch(Rectangle((cx - w / 2.0, cy - h / 2.0), w, h,
                               facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=1.1, zorder=4))
        ax.text(cx, cy, text, ha="center", va="center", fontsize=8.0, fontweight="bold",
                color=BLACK, linespacing=1.12, zorder=5)

    participant_box(xs["UI"], top_y, 1.65, 0.62, "«boundary»\n:FlightDialog")
    participant_box(xs["FS"], top_y, 1.65, 0.62, "«control»\n:FlightService")
    participant_box(xs["AS"], top_y, 1.75, 0.62, "«control»\n:AdmissionService")
    participant_box(xs["ME"], top_y, 1.80, 0.62, "«control»\n:MaintenanceEngine")
    participant_box(xs["DB"], top_y, 1.75, 0.62, "«entity»\n:DatabaseManager\n(SQLite WAL)")

    for k in ["Oper", "UI", "FS", "AS", "ME", "DB"]:
        y_start = top_y - 0.56 if k == "Oper" else top_y - 0.31
        ax.plot([xs[k], xs[k]], [y_start, bot_y], color=BLACK, ls=(0, (5, 4)), lw=0.9, zorder=1)
    ax.plot([xs["Res"], xs["Res"]], [9.32, 0.85], color=BLACK, ls=(0, (5, 4)), lw=0.9, zorder=1)

    def act_bar(key, y_top, y_bot):
        ax.add_patch(Rectangle((xs[key] - 0.08, y_bot), 0.16, y_top - y_bot,
                               facecolor=WHITE, edgecolor=BLACK, lw=0.95, zorder=3))

    act_bar("Oper", 11.65, 1.05)
    act_bar("UI",   11.62, 1.05)
    act_bar("FS",   10.88, 8.70)
    act_bar("DB",   10.55, 10.18)
    act_bar("AS",    9.85, 8.95)
    act_bar("FS",    7.50, 2.25)
    act_bar("DB",    6.80, 2.40)
    act_bar("ME",    6.02, 5.56)

    def msg(k1, k2, y, text, ret=False, pad=0.08, fontsize=7.9, align="center", tx_override=None):
        x1 = xs[k1] + (pad if xs[k2] > xs[k1] else -pad)
        x2 = xs[k2] - (pad if xs[k2] > xs[k1] else -pad)
        style = "->" if ret else "-|>"
        ls = "--" if ret else "-"
        draw_arrow(ax, (x1, y), (x2, y), style=style, ls=ls, lw=1.0, ms=9, zorder=5)
        tx = tx_override if tx_override is not None else (x1 + x2) / 2.0
        ax.text(tx, y + 0.06, text, ha=align, va="bottom", fontsize=fontsize, color=BLACK,
                bbox=dict(fc=WHITE, ec="none", pad=0.45), zorder=6)

    def self_msg(k, y, text, fontsize=7.8):
        x = xs[k] + 0.08
        ax.plot([x, x + 0.44, x + 0.44], [y, y, y - 0.24], color=BLACK, lw=0.95, zorder=4)
        draw_arrow(ax, (x + 0.44, y - 0.24), (x, y - 0.24), style="-|>", lw=0.95, ms=8, zorder=5)
        ax.text(x + 0.50, y - 0.10, text, ha="left", va="center", fontsize=fontsize, color=BLACK,
                bbox=dict(fc=WHITE, ec="none", pad=0.45), zorder=6)

    # Фаза 1: Проверка допуска
    msg("Oper", "UI", 11.60, "1. выбрать(борт, оператор, АКБ, дата, время)", align="left", tx_override=xs["Oper"] + 0.14)
    self_msg("UI", 11.34, "2. validateDuration(взлёт, посадка) [0 < dur ≤ 720 мин]")
    msg("UI", "FS", 10.86, "3. evaluateAdmission(dto)")
    msg("FS", "DB", 10.52, "4. SELECT состояние, ТО, допуск, АКБ, осмотр F-15", tx_override=9.05)
    msg("DB", "FS", 10.18, "5. контекст сущностей (AdmissionContext)", ret=True, tx_override=9.05)
    msg("FS", "AS",  9.82, "6. evaluate(ctx)")

    # <<create>> :AdmissionResult
    participant_box(xs["Res"], 9.58, 1.54, 0.54, "«entity»\n:AdmissionResult")
    draw_arrow(ax, (xs["AS"] + 0.08, 9.58), (xs["Res"] - 0.77, 9.58),
               style="->", ls="--", lw=1.0, ms=9, zorder=5)
    ax.text((xs["AS"] + xs["Res"] - 0.77) / 2.0, 9.64, "«create»", ha="center", va="bottom",
            fontsize=7.8, style="italic", bbox=dict(fc=WHITE, ec="none", pad=0.4), zorder=6)

    msg("AS", "FS", 9.02, "7. result : AdmissionResult", ret=True)
    msg("FS", "UI", 8.70, "8. result (allowed, reasons[])", ret=True)

    # Duration constraint {<= 200 мс}
    ax.plot([5.02, 5.18], [10.86, 10.86], color=BLACK, lw=0.85, zorder=5)
    ax.plot([5.02, 5.18], [8.70, 8.70], color=BLACK, lw=0.85, zorder=5)
    draw_arrow(ax, (5.10, 10.86), (5.10, 8.70), style="<->", lw=0.8, ms=7, zorder=5)
    ax.text(5.00, 9.78, "{≤ 200 мс}", ha="right", va="center", fontsize=7.9, fontweight="bold",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    def frag_frame(x0, y0, x1, y1, kw, cond, kw_w=0.80):
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, edgecolor=BLACK, lw=1.05, zorder=4))
        pts = [(x0, y1), (x0 + kw_w, y1), (x0 + kw_w, y1 - 0.24),
               (x0 + kw_w - 0.14, y1 - 0.36), (x0, y1 - 0.36)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=0.95, zorder=5))
        ax.text(x0 + kw_w / 2.0 - 0.04, y1 - 0.18, kw, ha="center", va="center",
                fontsize=8.2, fontweight="bold", zorder=6)
        ax.text(x0 + kw_w + 0.12, y1 - 0.18, cond, ha="left", va="center",
                fontsize=8.0, fontweight="bold", bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)

    # Внешний alt
    frag_frame(0.55, 1.12, 13.85, 8.38, "alt", "[result.allowed == true  —  Допуск к полёту разрешён]", kw_w=0.78)

    msg("Oper", "UI", 7.80, "9. нажать «Сохранить» (Enter)")
    msg("UI", "FS",   7.48, "10. registerFlight(dto, actor)")
    self_msg("FS",    7.28, "11. RbacGuard::requirePermission(RegisterFlight)")
    msg("FS", "DB",   6.78, "12. beginImmediate()  [BEGIN IMMEDIATE]", tx_override=9.30)
    msg("FS", "DB",   6.38, "13. INSERT flights; пересчёт SUM/COUNT (uavs, batteries)", tx_override=9.30)
    msg("FS", "ME",   5.98, "14. calculateRemainders(uav, rules, recs)", tx_override=8.50)
    msg("ME", "FS",   5.58, "15. remainders[]", ret=True, tx_override=8.50)

    # Вложенный opt
    frag_frame(5.20, 4.72, 13.60, 5.40, "opt", "[есть вид ТО с остатком ≤ 0]", kw_w=0.75)
    msg("FS", "DB", 4.86, "16. UPDATE uavs SET state = 'DueMaintenance'", tx_override=9.40)

    msg("FS", "DB", 4.40, "17. INSERT INTO audit_log(actor, 'FLIGHT_CREATE')", tx_override=9.40)

    # Вложенный alt [COMMIT / ROLLBACK]
    frag_frame(0.75, 2.04, 13.65, 4.20, "alt", "[все операции транзакции успешны]", kw_w=0.75)
    msg("FS", "DB", 3.68, "18. commit()", tx_override=9.30)
    msg("FS", "UI", 3.44, "19. true (flightId)", ret=True)
    msg("UI", "Oper", 3.16, "обновить таблицу и предупреждения", ret=True, align="left", tx_override=xs["Oper"] + 0.14)

    ax.plot([0.75, 13.65], [2.98, 2.98], color=BLACK, ls="--", lw=0.9, zorder=4)
    ax.text(0.95, 2.91, "[else — ошибка БД / сбой записи]", ha="left", va="top",
            fontsize=7.9, fontweight="bold", bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)

    # Interaction use (ref)
    rx0, ry0, rx1, ry1 = 5.30, 2.16, 13.48, 2.66
    ax.add_patch(Rectangle((rx0, ry0), rx1 - rx0, ry1 - ry0, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=1.0, zorder=5))
    rpts = [(rx0, ry1), (rx0 + 0.65, ry1), (rx0 + 0.65, ry1 - 0.18), (rx0 + 0.53, ry1 - 0.28), (rx0, ry1 - 0.28)]
    ax.add_patch(Polygon(rpts, closed=True, facecolor=MID_GRAY, edgecolor=BLACK, lw=0.9, zorder=6))
    ax.text(rx0 + 0.30, ry1 - 0.14, "ref", ha="center", va="center", fontsize=8.0, fontweight="bold", zorder=7)
    ax.text((rx0 + rx1) / 2.0 + 0.22, (ry0 + ry1) / 2.0,
            "Handle DB Error / Rollback (REQ-REL-001): rollback(), запись в uavlog.log",
            ha="center", va="center", fontsize=8.0, zorder=7)

    msg("FS", "UI", 2.38, "false (error)", ret=True)
    msg("UI", "Oper", 2.14, "сообщение об ошибке", ret=True)

    # Duration constraint {<= 1000 мс}
    ax.plot([5.02, 5.18], [7.48, 7.48], color=BLACK, lw=0.85, zorder=5)
    ax.plot([5.02, 5.18], [3.44, 3.44], color=BLACK, lw=0.85, zorder=5)
    draw_arrow(ax, (5.10, 7.48), (5.10, 3.44), style="<->", lw=0.8, ms=7, zorder=5)
    ax.text(5.00, 5.46, "{≤ 1000 мс}", ha="right", va="center", fontsize=7.9, fontweight="bold",
            bbox=dict(fc=WHITE, ec="none", pad=0.6), zorder=6)

    # Ветка else внешнего alt
    ax.plot([0.55, 13.85], [1.86, 1.86], color=BLACK, ls="--", lw=1.0, zorder=4)
    ax.text(0.75, 1.78, "[else — result.allowed == false (нарушено ≥ 1 из 5 условий допуска, БЦ-4)]",
            ha="left", va="top", fontsize=8.0, fontweight="bold", bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)
    msg("UI", "Oper", 1.28, "кнопка «Сохранить» отключена; вывод result.reasons[]",
        ret=True, align="left", tx_override=xs["Oper"] + 0.16)

    # Destruction occurrence (X)
    cx, cy = xs["Res"], 0.85
    ax.plot([cx - 0.15, cx + 0.15], [cy - 0.15, cy + 0.15], color=BLACK, lw=1.7, zorder=6)
    ax.plot([cx - 0.15, cx + 0.15], [cy + 0.15, cy - 0.15], color=BLACK, lw=1.7, zorder=6)

    draw_save(fig, "04-sequence-flight-registration.png")


# =============================================================================
# 5. ДИАГРАММА ПОСЛЕДОВАТЕЛЬНОСТИ №2 (05-sequence-preflight-and-readiness.png)
# =============================================================================
def render_seq_preflight():
    fig, ax = setup_canvas(12.4, 10.6, (0, 14.4), (0, 12.6))

    draw_uml_frame(ax, 0.22, 0.22, 14.18, 12.38,
                   "sd  Предполётный осмотр (F-15) и формирование Акта готовности парка", 7.50, 0.46, fontsize=10.2)

    xs = {
        "User":    1.05,
        "UI":      3.15,
        "PS":      5.45,
        "SM":      7.65,
        "DB":      9.75,
        "RS":      11.75,
        "Printer": 13.45,
    }
    top_y = 11.30
    bot_y = 0.55

    draw_actor(ax, xs["User"], top_y + 0.15, "Техник /\nОператор", scale=0.78, fontsize=8.1)
    draw_actor(ax, xs["Printer"], top_y + 0.15, "Принтер\nWindows", scale=0.78, fontsize=8.1)

    def participant_box(cx, cy, w, h, text):
        ax.add_patch(Rectangle((cx - w / 2.0, cy - h / 2.0), w, h,
                               facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=1.1, zorder=4))
        ax.text(cx, cy, text, ha="center", va="center", fontsize=8.0, fontweight="bold",
                color=BLACK, linespacing=1.12, zorder=5)

    participant_box(xs["UI"], top_y, 1.72, 0.62, "«boundary»\n:PreflightDialog")
    participant_box(xs["PS"], top_y, 1.75, 0.62, "«control»\n:PreflightService")
    participant_box(xs["SM"], top_y, 1.75, 0.62, "«control»\n:UavStateMachine")
    participant_box(xs["DB"], top_y, 1.80, 0.62, "«entity»\n:DatabaseManager\n(SQLite WAL)")
    participant_box(xs["RS"], top_y, 1.70, 0.62, "«control»\n:ReportService\n(PdfExporter)")

    for k in xs:
        y_start = top_y - 0.56 if k in ("User", "Printer") else top_y - 0.31
        ax.plot([xs[k], xs[k]], [y_start, bot_y], color=BLACK, ls=(0, (5, 4)), lw=0.9, zorder=1)

    def act_bar(key, y_top, y_bot):
        ax.add_patch(Rectangle((xs[key] - 0.08, y_bot), 0.16, y_top - y_bot,
                               facecolor=WHITE, edgecolor=BLACK, lw=0.95, zorder=3))

    act_bar("User", 10.45, 0.90)
    act_bar("UI",   10.42, 4.30)
    act_bar("PS",   10.12, 9.20)
    act_bar("DB",    9.88, 9.52)
    act_bar("PS",    8.48, 4.55)
    act_bar("DB",    8.18, 4.70)
    act_bar("SM",    5.86, 5.48)
    act_bar("RS",    3.42, 0.95)
    act_bar("PS",    3.06, 2.28)
    act_bar("DB",    2.80, 2.48)

    def msg(k1, k2, y, text, ret=False, async_msg=False, pad=0.08, fontsize=7.9, align="center", tx_override=None):
        x1 = xs[k1] + (pad if xs[k2] > xs[k1] else -pad)
        x2 = xs[k2] - (pad if xs[k2] > xs[k1] else -pad)
        style = "->" if (ret or async_msg) else "-|>"
        ls = "--" if ret else "-"
        draw_arrow(ax, (x1, y), (x2, y), style=style, ls=ls, lw=1.0, ms=9, zorder=5)
        tx = tx_override if tx_override is not None else (x1 + x2) / 2.0
        ax.text(tx, y + 0.06, text, ha=align, va="bottom", fontsize=fontsize, color=BLACK,
                bbox=dict(fc=WHITE, ec="none", pad=0.45), zorder=6)

    def frag_frame(x0, y0, x1, y1, kw, cond, kw_w=0.80):
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, edgecolor=BLACK, lw=1.05, zorder=4))
        pts = [(x0, y1), (x0 + kw_w, y1), (x0 + kw_w, y1 - 0.24),
               (x0 + kw_w - 0.14, y1 - 0.36), (x0, y1 - 0.36)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=LIGHT_GRAY, edgecolor=BLACK, lw=0.95, zorder=5))
        ax.text(x0 + kw_w / 2.0 - 0.04, y1 - 0.18, kw, ha="center", va="center",
                fontsize=8.2, fontweight="bold", zorder=6)
        ax.text(x0 + kw_w + 0.12, y1 - 0.18, cond, ha="left", va="center",
                fontsize=8.0, fontweight="bold", bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)

    # Загрузка контрольной карты
    msg("User", "UI", 10.40, "1. открыть осмотр(uavId, date)", align="left", tx_override=xs["User"] + 0.14)
    msg("UI", "PS",   10.10, "2. loadActiveTemplateForUav(uavId)")
    msg("PS", "DB",    9.84, "3. SELECT checklist_templates, checklist_items")
    msg("DB", "PS",    9.52, "4. ChecklistTemplate + items[] (признак критичности)", ret=True)
    msg("PS", "UI",    9.22, "5. template", ret=True)
    msg("UI", "User",  8.96, "отобразить контрольную карту", ret=True, align="left", tx_override=xs["User"] + 0.14)

    # Сохранение осмотра
    msg("User", "UI",  8.66, "6. отметить пункты; «Сохранить осмотр»", align="left", tx_override=xs["User"] + 0.14)
    msg("UI", "PS",    8.44, "7. saveInspection(uavId, tplId, date, results[], actor)", align="left", tx_override=xs["UI"] + 0.14)
    msg("PS", "DB",    8.14, "8. beginImmediate(); INSERT preflight_checks, preflight_results")

    # alt: пройден / критичное замечание
    frag_frame(0.55, 4.26, 10.65, 7.86, "alt", "[нет критичных замечаний  (passed == true)]", kw_w=0.78)
    msg("PS", "DB",   7.24, "9a. INSERT audit_log('PREFLIGHT_PASS'); commit()")
    msg("PS", "UI",   6.92, "10a. ok (passed = true)", ret=True)
    msg("UI", "User", 6.62, "осмотр пройден (5-й фактор допуска активен)", ret=True, align="left", tx_override=xs["User"] + 0.14)

    ax.plot([0.55, 10.65], [6.40, 6.40], color=BLACK, ls="--", lw=0.95, zorder=4)
    ax.text(0.75, 6.32, "[else — выявлено критичное замечание  (passed == false)]",
            ha="left", va="top", fontsize=8.0, fontweight="bold", bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)
    msg("PS", "SM",   5.82, "9b. transition(curState, CriticalFaultReported)", align="left", tx_override=xs["PS"] + 0.14)
    msg("SM", "PS",   5.50, "10b. UavStatus::Faulty", ret=True)
    msg("PS", "DB",   5.18, "11b. UPDATE uavs SET state='Faulty'; INSERT faults")
    msg("PS", "DB",   4.86, "12b. INSERT audit_log('PREFLIGHT_FAIL_FAULTY'); commit()")
    msg("PS", "UI",   4.62, "13b. ok (passed = false, Faulty)", ret=True)
    msg("UI", "User", 4.38, "допуск заблокирован, борт → «Неисправен»", ret=True, align="left", tx_override=xs["User"] + 0.14)

    # opt: формирование и печать Акта готовности парка A4
    frag_frame(0.55, 0.72, 13.85, 4.02, "opt",
               "[формирование и печать Акта готовности парка (Администратор / Наблюдатель)]", kw_w=0.78)

    msg("User", "RS", 3.38, "14. generateReport(FleetReadinessAct, date)", tx_override=4.80)
    msg("RS", "PS",   3.04, "15. buildFleetReadinessRows(date)", tx_override=8.60)
    msg("PS", "DB",   2.76, "16. SELECT uavs + preflight_checks(date)")
    msg("DB", "PS",   2.48, "readinessRows[]", ret=True)
    msg("PS", "RS",   2.22, "readinessRows[]", ret=True, tx_override=8.60)

    # Self-message: рендер PDF A4
    x = xs["RS"] + 0.08
    ax.plot([x, x + 0.44, x + 0.44], [2.04, 2.04, 1.76], color=BLACK, lw=0.95, zorder=4)
    draw_arrow(ax, (x + 0.44, 1.76), (x, 1.76), style="-|>", lw=0.95, ms=8, zorder=5)
    ax.text(x + 0.50, 1.90, "HTML/CSS →\nPDF A4", ha="left", va="center", fontsize=7.6,
            bbox=dict(fc=WHITE, ec="none", pad=0.4), zorder=6)

    # Duration constraint {<= 15 с}
    ax.plot([11.28, 11.44], [3.38, 3.38], color=BLACK, lw=0.85, zorder=5)
    ax.plot([11.28, 11.44], [0.96, 0.96], color=BLACK, lw=0.85, zorder=5)
    draw_arrow(ax, (11.36, 3.38), (11.36, 0.96), style="<->", lw=0.8, ms=7, zorder=5)
    ax.text(11.26, 1.62, "{≤ 15 с}", ha="right", va="center", fontsize=7.9, fontweight="bold",
            bbox=dict(fc=WHITE, ec="none", pad=0.5), zorder=6)

    msg("RS", "Printer", 1.32, "17. printHtmlA4()", async_msg=True, fontsize=7.8)
    msg("RS", "User",    0.96, "18. файл PDF A4 сохранён / отправлен на печать", ret=True, tx_override=5.50)

    draw_save(fig, "05-sequence-preflight-and-readiness.png")


if __name__ == "__main__":
    render_use_case()
    render_activity()
    render_state_machine()
    render_seq_flight()
    render_seq_preflight()
    print("All 5 monochrome STP 01-2024 UML 2.5 diagrams generated successfully.")
