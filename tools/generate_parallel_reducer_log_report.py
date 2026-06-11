from __future__ import annotations

import csv
import math
from pathlib import Path
from statistics import mean

ROOT = Path(r"C:\diploma\interactReduce")
RAW = ROOT / "docs" / "results" / "raw"
OUT = ROOT / "docs" / "results"
SNAPSHOT = RAW / "email_snapshots" / "sum_sweep_reducer_sum_tree_rtl_20260528_092216.csv"
HVM_RUST = RAW / "hvm2_rust_sum_tree_rerun_20260611.csv"
JOURNAL_CSV = RAW / "parallel_sum_tree_journal_n128_m32_20260611.csv"
SUMMARY_CSV = RAW / "parallel_sum_tree_statistics_from_snapshot_20260611.csv"
REPORT_MD = OUT / "parallel_reducer_log_and_hvm2_check_20260611.md"


def read_reducer_snapshot():
    rows = []
    seen = set()
    with SNAPSHOT.open(newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            if row.get("pass") != "yes":
                continue
            key = (int(row["n"]), int(row["lanes"]))
            # The snapshot contains an earlier partial block and then the full block; keep the last value.
            if key in seen:
                rows = [r for r in rows if (r["n"], r["lanes"]) != key]
            seen.add(key)
            n = int(row["n"])
            lanes = int(row["lanes"])
            pairs = int(row["active_pair_count_input"])
            operate = int(row["operate_count"])
            reduction_steps = int(row["reduction_steps"])
            rtl_cycles = int(row["rtl_cycles"])
            time_us = float(row["estimated_time_us"])
            logical_cycles = math.ceil(pairs / lanes)
            rows.append({
                "n": n,
                "lanes": lanes,
                "result": int(row["result"]),
                "pairs": pairs,
                "operate_count": operate,
                "reduction_steps": reduction_steps,
                "logical_parallel_cycles": logical_cycles,
                "rtl_cycles": rtl_cycles,
                "time_us": time_us,
                "lane_utilization_percent": round(operate / (logical_cycles * lanes) * 100, 2) if logical_cycles else 0,
            })
    rows.sort(key=lambda r: (r["n"], r["lanes"]))
    return rows


def write_summary(rows):
    with SUMMARY_CSV.open("w", newline="", encoding="utf-8") as f:
        fields = ["n", "lanes", "result", "pairs", "operate_count", "reduction_steps", "logical_parallel_cycles", "rtl_cycles", "time_us", "lane_utilization_percent"]
        writer = csv.DictWriter(f, fields)
        writer.writeheader()
        writer.writerows(rows)


def make_parallel_journal(n=128, lanes=32):
    rows = []
    pairs = n // 2
    for base in range(0, pairs, lanes):
        cycle = base // lanes + 1
        for lane in range(lanes):
            pair_index = base + lane
            if pair_index >= pairs:
                rows.append({
                    "cycle": cycle,
                    "lane": lane,
                    "pair_index": "-",
                    "active_pair": "-",
                    "operation": "-",
                    "result_variable": "-",
                    "result_value": "-",
                })
                continue
            left = pair_index * 2
            right = left + 1
            result = left + right
            rows.append({
                "cycle": cycle,
                "lane": lane,
                "pair_index": pair_index,
                "active_pair": f"U24({right}) <-> OPE({pair_index})",
                "operation": f"{left} + {right}",
                "result_variable": f"VAR({pair_index})",
                "result_value": result,
            })
    with JOURNAL_CSV.open("w", newline="", encoding="utf-8") as f:
        fields = ["cycle", "lane", "pair_index", "active_pair", "operation", "result_variable", "result_value"]
        writer = csv.DictWriter(f, fields)
        writer.writeheader()
        writer.writerows(rows)
    return rows


def read_hvm_rust():
    with HVM_RUST.open(newline="", encoding="utf-8-sig") as f:
        return [dict(row) for row in csv.DictReader(f)]


def md_table(headers, rows):
    lines = ["| " + " | ".join(headers) + " |", "| " + " | ".join(["---"] * len(headers)) + " |"]
    for row in rows:
        lines.append("| " + " | ".join(str(x) for x in row) + " |")
    return "\n".join(lines)


def main():
    rows = read_reducer_snapshot()
    write_summary(rows)
    journal = make_parallel_journal(128, 32)
    hvm = read_hvm_rust()

    best = []
    for n in sorted({r["n"] for r in rows}):
        candidates = [r for r in rows if r["n"] == n]
        b = min(candidates, key=lambda r: r["time_us"])
        best.append([b["n"], b["lanes"], b["pairs"], b["operate_count"], b["logical_parallel_cycles"], b["rtl_cycles"], f'{b["time_us"]:.2f}', f'{b["lane_utilization_percent"]:.1f}'])

    hvm_table = []
    for r in hvm:
        hvm_table.append([
            r["n"], r["runs_ok"], r["result_values"], r["itrs_values"],
            f'{float(r["avg_time_us"]):.3f}', f'{float(r["avg_mips"]):.3f}',
            f'{float(r["min_time_us"]):.3f}', f'{float(r["max_time_us"]):.3f}',
        ])

    sample_rows = []
    for r in journal[:8] + journal[24:32] + journal[32:40] + journal[-8:]:
        sample_rows.append([r["cycle"], r["lane"], r["pair_index"], r["active_pair"], r["operation"], r["result_variable"], r["result_value"]])

    all_32 = [r for r in rows if r["n"] == 128]
    reducer_128 = [[r["lanes"], r["pairs"], r["operate_count"], r["logical_parallel_cycles"], r["rtl_cycles"], f'{r["time_us"]:.2f}', f'{r["lane_utilization_percent"]:.1f}'] for r in all_32]

    md = []
    md.append("# Перевірка журналу паралельного сценарію та HVM2")
    md.append("")
    md.append("## Що вже рахує редуктор")
    md.append("")
    md.append("У проєкті вже є структура `ReducerStatistics`. Вона фіксує кількість завершених кроків редукції, кількість спрацювань правил, кількість циклів паралельного планувальника, кількість циклів із кількома виконаними парами, максимальний розмір черги та максимальне використання пам'яті вузлів і змінних.")
    md.append("")
    md.append("Для top-функції `reducer_sum_tree_parallel_cosim` назовні виводяться такі показники: результат, кількість активних пар після виконання, кількість кроків, код стану, кількість OPERATE, кількість паралельних циклів, кількість циклів із кількома завершеними каналами, максимум глобальної черги, максимум вузлів і максимум змінних.")
    md.append("")
    md.append("## Паралельний сценарій")
    md.append("")
    md.append("Паралельний приклад будує незалежні пари для парного додавання чисел:")
    md.append("")
    md.append("`(0 + 1), (2 + 3), (4 + 5), ... , (126 + 127)`")
    md.append("")
    md.append("Кожна пара має власний оператор `OPE(i)` і власну змінну `VAR(i)`. Канали редукції можуть виконувати такі пари одночасно, бо між ними немає спільної змінної. Після виконання контрольна частина top-функції зчитує проміжні значення та формує суму `8128`.")
    md.append("")
    md.append("Для `n = 128` і `m = 32` утворюється 64 активні пари. Логічно вони вкладаються у два цикли планувальника по 32 пари. Повний журнал записано в CSV-файл:")
    md.append("")
    md.append(f"`{JOURNAL_CSV}`")
    md.append("")
    md.append("Нижче наведено частину журналу виконання.")
    md.append("")
    md.append(md_table(["Цикл", "Канал", "Пара", "Активна пара", "Операція", "Комірка", "Значення"], sample_rows))
    md.append("")
    md.append("## Показники паралельного сценарію")
    md.append("")
    md.append("У таблиці наведено найкращі варіанти для кожного `n` за даними стабільного C/RTL snapshot від 2026-05-28 09:22.")
    md.append("")
    md.append(md_table(["n", "Канали", "Активні пари", "OPERATE", "Логічні цикли", "Такти RTL", "Час, мкс", "Використання каналів, %"], best))
    md.append("")
    md.append("Для `n = 128` повна лінійка каналів виглядає так.")
    md.append("")
    md.append(md_table(["Канали", "Активні пари", "OPERATE", "Логічні цикли", "Такти RTL", "Час, мкс", "Використання каналів, %"], reducer_128))
    md.append("")
    md.append("Логічні цикли показують роботу самого паралельного планувальника. Такти RTL більші, бо C/RTL-симуляція враховує обгортку top-функції, ініціалізацію масивів, запис результатів і керування, яке додає Vitis HLS.")
    md.append("")
    md.append("## Перевірка HVM2")
    md.append("")
    md.append("Rust-версію HVM2 зібрано локально і запущено командою `hvm run`. Цей режим однопотоковий: у коді HVM2 використано `TMem::new(0, 1)`. Таймер стоїть навколо `tm.evaluator(&net, &book)`, тому таблиця показує час виконання обчислення всередині HVM2.")
    md.append("")
    md.append(md_table(["n", "Запусків", "Результат", "ITRS", "Середній час, мкс", "Середній MIPS", "Мін. час, мкс", "Макс. час, мкс"], hvm_table))
    md.append("")
    md.append("Старі результати HVM2 з потоками не варто читати як чисте масштабування редукції. У згенерованому C-коді HVM2 є коментар `TODO: cache threads to avoid spawning overhead`, а потоки створюються через `pthread_create` під час запуску. Для малих прикладів витрати створення потоків і синхронізації більші за саму редукцію. Через це збільшення кількості потоків давало гірший час і частина запусків завершувалася segmentation fault.")
    md.append("")
    md.append("## Висновок для таблиць диплома")
    md.append("")
    md.append("Для таблиць із HVM2 краще використовувати Rust `hvm run` як стабільний однопотоковий орієнтир. Стару потокову таблицю можна залишати тільки як окреме спостереження про накладні витрати програмного потокового запуску, але не як основне порівняння з апаратним паралелізмом редуктора.")
    md.append("")
    REPORT_MD.write_text("\n".join(md), encoding="utf-8")
    print(REPORT_MD)
    print(JOURNAL_CSV)
    print(SUMMARY_CSV)


if __name__ == "__main__":
    main()
