#!/usr/bin/env python3
"""Runs the simulation under different parameters and writes experiments/results.md.

Each experiment changes one parameter from the baseline (20 servers,
100,000 cycles, config.txt values) and records the combined summary numbers.
Run from the project root after `make`:  python3 experiments/run_experiments.py
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "loadbalancer"

BASE = {"servers": 20, "cycles": 100000, "set": {}}

EXPERIMENTS = [
    ("Initial server count", [
        ("5 servers",   {"servers": 5}),
        ("20 servers (baseline)", {}),
        ("60 servers",  {"servers": 60}),
    ]),
    ("Request arrival frequency", [
        ("p = 0.10 (light)",  {"set": {"arrival_probability": "0.10"}}),
        ("p = 0.30 (baseline)", {}),
        ("p = 0.50 (heavy)",  {"set": {"arrival_probability": "0.50"}}),
        ("p = 0.00 (no new traffic)", {"set": {"arrival_probability": "0.0"}}),
    ]),
    ("Request processing duration", [
        ("short (P 2-15, S 10-40)", {"set": {"processing_time_min": "2", "processing_time_max": "15",
                                             "streaming_time_min": "10", "streaming_time_max": "40"}}),
        ("baseline (P 5-45, S 40-120)", {}),
        ("long (P 20-120, S 100-300)", {"set": {"processing_time_min": "20", "processing_time_max": "120",
                                                "streaming_time_min": "100", "streaming_time_max": "300"}}),
    ]),
    ("Server scaling interval", [
        ("25 cycles",   {"set": {"scale_interval": "25"}}),
        ("250 cycles (baseline)", {}),
        ("2,500 cycles", {"set": {"scale_interval": "2500"}}),
        ("25 cycles, start at 5 servers", {"servers": 5, "set": {"scale_interval": "25"}}),
        ("2,500 cycles, start at 5 servers", {"servers": 5, "set": {"scale_interval": "2500"}}),
    ]),
    ("Total simulation duration", [
        ("10,000 cycles",  {"cycles": 10000}),
        ("100,000 cycles (baseline)", {}),
        ("500,000 cycles", {"cycles": 500000}),
    ]),
    ("Routing architecture", [
        ("Switch (S/P split, baseline)", {}),
        ("Single balancer", {"single": True}),
    ]),
]

OBSERVATIONS = [
    "## Observations", "",
    "- **The server count converges to the load, not to the starting value.** Starting with 5, 20 or 60 "
    "servers all end near the same pool size, roughly arrival rate x average job time. The starting "
    "count only changes how many scaling actions it takes to get there.",
    "- **Utilization stays near 100% whenever traffic keeps arriving.** The 50-80 rule deliberately keeps "
    "50-80 requests waiting per server, so an idle server always finds work. Idle capacity only appears "
    "when traffic stops (p = 0): the pool shrinks toward `min_servers` and utilization drops.",
    "- **Queue wait is the price of that rule.** With 50-80 queued per server, a request waits roughly "
    "65 x the average job length. Longer jobs mean proportionally longer waits and a larger pool.",
    "- **Arrival frequency scales the pool linearly.** Doubling arrivals roughly doubles the servers kept "
    "online and the throughput, while queue length per server stays in the band.",
    "- **The scaling interval sets reaction speed.** At steady load it barely matters. Starting far "
    "from equilibrium (5 servers), a long interval reacts late and then overshoots: the pool swings "
    "above and below the right size, roughly doubling the add/remove churn and raising the average "
    "queue and waits. A short interval converges smoothly.",
    "- **Duration:** long runs settle at the same per-cycle throughput; short runs are dominated by the "
    "initial queue and ramp-up.",
    "- **Switch vs single balancer:** total throughput is about the same. Splitting by job type lets short "
    "processing jobs avoid waiting behind long streaming jobs, so processing waits drop while streaming "
    "waits rise; each pool can scale on its own.",
    "",
]

FIELDS = {
    "servers": r"Servers \(initial -> final\)\s+(\d+ -> \d+)",
    "scaling": r"Servers added / removed\s+(\d+ / \d+)",
    "generated": r"Requests generated \(incl\. initial\)\s+(\d+)",
    "blocked": r"Requests blocked by firewall\s+(\d+)",
    "processed": r"Requests processed\s+(\d+)",
    "left": r"Remaining in queue\s+(\d+)",
    "avgq": r"Average total queue size\s+([\d.]+)",
    "util": r"Average server utilization\s+([\d.]+%)",
    "tput": r"Requests completed per cycle\s+([\d.]+)",
}


def run(params):
    p = {**BASE, **params}
    cmd = [str(BIN), "-s", str(p["servers"]), "-t", str(p["cycles"]), "-q", "--no-color"]
    cmd.append("--single" if p.get("single") else "--switch")
    for k, v in p.get("set", {}).items():
        cmd += ["--set", f"{k}={v}"]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True, cwd=ROOT).stdout
    combined = out.split("--- All balancers combined ---", 1)[1]
    row = {}
    for key, pattern in FIELDS.items():
        m = re.search(pattern, combined)
        row[key] = m.group(1) if m else "?"
    waits = re.findall(r"Average wait in queue \(cycles\)\s+([\d.]+)", out)
    row["wait"] = " / ".join(waits)
    return row


def main():
    lines = ["# Performance experiments", "",
             "Baseline: 20 servers, 100,000 cycles, seed 42, switch routing, values from `config.txt`.",
             "Each table changes one parameter. Wait is the average queue wait in cycles per balancer",
             "(streaming / processing in switch mode).", ""]
    header = ("| Setting | Servers start→end | Added / removed | Processed | Left in queue | "
              "Avg queue | Avg wait | Utilization | Done / cycle |")
    sep = "|---|---|---|---|---|---|---|---|---|"
    for title, cases in EXPERIMENTS:
        lines += [f"## {title}", "", header, sep]
        for label, params in cases:
            r = run(params)
            lines.append(f"| {label} | {r['servers'].replace(' -> ', ' → ')} | {r['scaling']} | "
                         f"{r['processed']} | {r['left']} | {r['avgq']} | {r['wait']} | "
                         f"{r['util']} | {r['tput']} |")
            print(f"{title:28} {label:32} {r}")
        lines.append("")
    lines += OBSERVATIONS
    out = ROOT / "experiments" / "results.md"
    out.write_text("\n".join(lines))
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
