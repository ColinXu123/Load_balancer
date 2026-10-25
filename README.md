# Colin's Balance — Load Balancer Simulation

A clock-driven C++17 simulation of web traffic flowing through an IP-range firewall, a job-type
switch, and load balancers that scale their web-server pools to keep 50–80 queued requests per server.

## Build and run

```bash
make                                   # builds ./loadbalancer from src/
./loadbalancer                         # prompts for server count and cycles
./loadbalancer -s 20 -t 100000 -q      # quiet: start/end summaries + checkpoints
./loadbalancer -s 20 -t 100000 -v      # verbose: every significant event, colored
./loadbalancer --single                # one balancer instead of the S/P switch
./loadbalancer --set arrival_probability=0.5 --set scale_interval=50
make logs                              # regenerate logs/ (20 servers, 100,000 cycles)
make docs                              # Doxygen HTML into docs/doxygen/html
python3 experiments/run_experiments.py # parameter sweep -> experiments/results.md
```

Run `./loadbalancer -h` for every flag. Settings live in `config.txt` (arrival rate, burst size,
streaming ratio, per-type processing times, scaling thresholds and interval, min/max servers,
blocked IP ranges, attack traffic share, logging). `--set KEY=VALUE` overrides any key.

## Layout

| Path | Contents |
|---|---|
| `src/Request.h` | `Request` struct: IP in/out, time, job type |
| `src/RequestQueue.*` | FIFO queue of pending requests |
| `src/WebServer.*` | Processes one request at a time; draining for safe removal |
| `src/LoadBalancer.*` | Queue, server pool, clock, scaling rule, statistics |
| `src/Switch.*` | Firewall check and routing of `S`/`P` jobs to separate balancers |
| `src/IPBlocker.*` | Blocked IPv4 ranges (CIDR or `a-b`) |
| `src/RequestGenerator.*` | Random arrivals, IPs, job types, processing times |
| `src/Config.*`, `src/Logger.*` | Configuration file, colored/verbose/quiet logging |
| `src/main.cpp` | Start-up, prompts, main loop, summaries |
| `docs/design.html` | HTML/CSS design page (architecture, classes, workflow, scaling) |
| `docs/doxygen/html/` | Generated API documentation (`index.html`) |
| `logs/` | 20-server / 100,000-cycle logs: switch summary, single summary, full verbose log |
| `experiments/` | Sweep script and `results.md` with findings |

## Scaling rule

Every cycle, unless in cooldown, each balancer compares its queue `Q` with active servers `N`:
`Q > 80·N` adds a server, `Q < 50·N` removes one, then it waits `scale_interval` cycles.
An idle server is removed immediately; otherwise the busy server closest to finishing drains
(finishes its request, takes nothing new) before leaving, so no work is lost. Each run ends with an
accounting check: accepted = processed + queued + in flight.
