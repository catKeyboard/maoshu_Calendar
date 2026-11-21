#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DuduClock 壁纸转换工具

把普通图片转换成固件可编译的 wallpaper_data.h（320x240，RGB565）。
生成的壁纸会被编译进固件，只用白天版（桌面壁纸已取消夜间模式）。

用法：
  1) 图形界面（推荐）：
         python wallpaper_convert.py
  2) 命令行生成内置渐变壁纸（恢复默认壁纸）：
         python wallpaper_convert.py --demo
  3) 命令行转换自己的图片：
         python wallpaper_convert.py --day 白天1.png 白天2.png --out wallpaper_data.h

依赖：
  - 生成默认渐变壁纸：无第三方依赖
  - 转换自己的图片：Pillow（pip install pillow）
  - 图形界面：Python 自带的 tkinter

注意：
  每套壁纸约占 150KB 固件空间，套数不宜过多。
"""

import argparse
import os
import sys

# ===== 屏幕参数 =====
W, H = 320, 240
COUNT = W * H

IMG_TYPES = [
    ("图片文件", "*.png *.jpg *.jpeg *.bmp *.webp"),
    ("所有文件", "*.*"),
]

# 仓库里 wallpaper_data.h 的默认位置（本文件在 tools/ 下）
DEFAULT_OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "maoshu_Calendar", "wallpaper_data.h")


def to565(r, g, b):
    """RGB -> RGB565（与 TFT_eSPI 的 color565 一致）"""
    r = 0 if r < 0 else (255 if r > 255 else int(r))
    g = 0 if g < 0 else (255 if g > 255 else int(g))
    b = 0 if b < 0 else (255 if b > 255 else int(b))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def lerp(a, b, t):
    return a + (b - a) * t


# ==================== 内置渐变壁纸（不依赖 Pillow） ====================

def demo_day():
    """日间：暖米色渐变 + 左上暖光 + 右下淡蓝"""
    px = [0] * COUNT
    i = 0
    for y in range(H):
        ty = y / (H - 1)
        r = lerp(250, 212, ty)
        g = lerp(247, 227, ty)
        b = lerp(238, 242, ty)
        for x in range(W):
            dx = (x - 58) / 230.0
            dy = (y - 36) / 170.0
            w = 1.0 - (dx * dx + dy * dy)
            if w < 0:
                w = 0
            rr = r + 16 * w
            gg = g + 9 * w
            bb = b - 10 * w
            dx2 = (x - 262) / 200.0
            dy2 = (y - 212) / 140.0
            w2 = 1.0 - (dx2 * dx2 + dy2 * dy2)
            if w2 > 0:
                a = w2 * 0.5
                rr += -12 * a
                gg += 2 * a
                bb += 8 * a
            px[i] = to565(rr, gg, bb)
            i += 1
    return px


# ==================== 图片转换（依赖 Pillow） ====================

def image_pixels(path, amount):
    """
    读取图片，等比裁剪成 320x240 后转 RGB565。
    向白色混合 amount（0~0.9），保证深色字可读
    """
    try:
        from PIL import Image, ImageOps
    except ImportError:
        raise RuntimeError("缺少 Pillow，请先执行：pip install pillow")

    im = Image.open(path).convert("RGB")
    im = ImageOps.fit(im, (W, H), method=Image.LANCZOS)

    amount = max(0.0, min(0.9, amount))
    white = Image.new("RGB", (W, H), (255, 255, 255))
    im = Image.blend(im, white, amount)

    raw = im.tobytes()
    px = [0] * COUNT
    for i in range(COUNT):
        px[i] = to565(raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2])
    return px


# ==================== 生成头文件 ====================

def write_header(path, sets):
    """sets: [day_pixels, ...]"""
    if not sets:
        raise RuntimeError("没有可生成的壁纸")

    out = []
    out.append("#ifndef __WALLPAPER_DATA_H\n")
    out.append("#define __WALLPAPER_DATA_H\n\n")
    out.append("// 本文件由 tools/wallpaper_convert.py 自动生成，请勿手工修改\n")
    out.append("// 320x240 RGB565，每套一张白天版（桌面壁纸已取消夜间模式）\n\n")
    out.append("#define WALLPAPER_SET_COUNT %d\n" % len(sets))
    out.append("#define WALLPAPER_W 320\n")
    out.append("#define WALLPAPER_H 240\n\n")

    out.append("PROGMEM const unsigned short wallpaperDay"
               "[WALLPAPER_SET_COUNT][WALLPAPER_W * WALLPAPER_H] = {\n")
    for px in sets:
        out.append("  {\n")
        for i in range(0, len(px), 12):
            out.append("    " + ",".join("0x%04X" % v for v in px[i:i + 12]) + ",\n")
        out.append("  },\n")
    out.append("};\n\n")

    out.append("#endif\n")

    folder = os.path.dirname(os.path.abspath(path))
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(out))
    return os.path.getsize(path)


def build_from_files(day_files, out, day_lighten=0.25):
    if not day_files:
        raise RuntimeError("请至少提供一张白天壁纸")

    sets = []
    for i, d in enumerate(day_files):
        print("处理第 %d 套：%s" % (i + 1, os.path.basename(d)))
        sets.append(image_pixels(d, day_lighten))
    return sets


# ==================== 图形界面 ====================

def run_gui():
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
    except Exception as e:
        print("无法启动图形界面（%s）。" % e)
        print("可改用命令行：python wallpaper_convert.py --day 白天1.png 白天2.png")
        return 1

    sets = []
    root = tk.Tk()
    root.title("DuduClock 壁纸转换工具")
    root.geometry("620x460")
    root.minsize(560, 420)

    tip = tk.Label(
        root,
        text="每套壁纸为一张 320x240 白天图，会编译进固件作为桌面背景。\n"
             "每套约占 150KB 固件空间。",
        justify="left", anchor="w", fg="#444")
    tip.pack(fill="x", padx=12, pady=(10, 6))

    list_frame = tk.Frame(root)
    list_frame.pack(fill="both", expand=True, padx=12)
    listbox = tk.Listbox(list_frame, height=9)
    scroll = tk.Scrollbar(list_frame, orient="vertical", command=listbox.yview)
    listbox.configure(yscrollcommand=scroll.set)
    listbox.pack(side="left", fill="both", expand=True)
    scroll.pack(side="right", fill="y")

    lighten_day = tk.DoubleVar(value=0.25)
    out_var = tk.StringVar(value=DEFAULT_OUT)

    def refresh():
        listbox.delete(0, "end")
        for i, d in enumerate(sets):
            listbox.insert("end", "%d. %s" % (i + 1, os.path.basename(d)))

    def add_set():
        d = filedialog.askopenfilename(title="选择壁纸", filetypes=IMG_TYPES)
        if not d:
            return
        sets.append(d)
        refresh()

    def del_set():
        sel = listbox.curselection()
        if not sel:
            return
        del sets[sel[0]]
        refresh()

    def do_build():
        if not sets:
            messagebox.showwarning("提示", "请先添加至少一套壁纸")
            return
        out = out_var.get().strip()
        if not out:
            messagebox.showwarning("提示", "请填写输出文件路径")
            return
        try:
            built = [image_pixels(d, lighten_day.get()) for d in sets]
            size = write_header(out, built)
            messagebox.showinfo("完成",
                                "已生成：\n%s\n\n共 %d 套，文件 %.1f KB\n\n"
                                "重新编译并烧录固件即可生效。"
                                % (out, len(built), size / 1024.0))
        except Exception as e:
            messagebox.showerror("生成失败", str(e))

    btns = tk.Frame(root)
    btns.pack(fill="x", padx=12, pady=6)
    tk.Button(btns, text="添加一套", width=12, command=add_set).pack(side="left")
    tk.Button(btns, text="删除选中", width=12, command=del_set).pack(side="left", padx=6)
    tk.Button(btns, text="恢复默认壁纸", width=14,
              command=lambda: write_header(out_var.get().strip() or DEFAULT_OUT,
                                           [demo_day()])
              ).pack(side="left")

    opt = tk.Frame(root)
    opt.pack(fill="x", padx=12, pady=4)
    tk.Label(opt, text="图片提亮").pack(side="left")
    tk.Scale(opt, from_=0.0, to=0.8, resolution=0.05, orient="horizontal",
             length=160, variable=lighten_day).pack(side="left", padx=2)

    out_row = tk.Frame(root)
    out_row.pack(fill="x", padx=12, pady=4)
    tk.Label(out_row, text="输出文件").pack(side="left")
    tk.Entry(out_row, textvariable=out_var).pack(side="left", fill="x", expand=True, padx=6)
    tk.Button(out_row, text="浏览", command=lambda: out_var.set(
        filedialog.asksaveasfilename(defaultextension=".h",
                                     initialfile="wallpaper_data.h",
                                     filetypes=[("C 头文件", "*.h")]) or out_var.get())
              ).pack(side="left")

    tk.Button(root, text="生成 wallpaper_data.h", height=2,
              command=do_build).pack(fill="x", padx=12, pady=(6, 12))

    root.mainloop()
    return 0


# ==================== 命令行入口 ====================

def main():
    ap = argparse.ArgumentParser(description="DuduClock 壁纸转换工具")
    ap.add_argument("--demo", action="store_true", help="生成内置渐变壁纸")
    ap.add_argument("--day", nargs="+", help="壁纸图片（可多张，按套排列）")
    ap.add_argument("--out", default=DEFAULT_OUT, help="输出的 wallpaper_data.h 路径")
    ap.add_argument("--day-lighten", type=float, default=0.25, help="图片提亮程度 0~0.9")
    args = ap.parse_args()

    try:
        if args.demo:
            size = write_header(args.out, [demo_day()])
            print("已生成默认壁纸：%s（%.1f KB）" % (args.out, size / 1024.0))
            return 0
        if args.day:
            sets = build_from_files(args.day, args.out, args.day_lighten)
            size = write_header(args.out, sets)
            print("已生成：%s（%d 套，%.1f KB）" % (args.out, len(sets), size / 1024.0))
            return 0
    except Exception as e:
        print("生成失败：%s" % e)
        return 1

    return run_gui()


if __name__ == "__main__":
    sys.exit(main())
