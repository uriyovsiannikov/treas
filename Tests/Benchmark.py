#!/usr/bin/env python3
import pathlib
import statistics
import subprocess
import sys
import time


def percentile(values, percent):
    index = (len(values) - 1) * percent / 100
    lower = int(index)
    upper = min(lower + 1, len(values) - 1)
    fraction = index - lower
    return values[lower] * (1 - fraction) + values[upper] * fraction


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    launcher = root / "Build" / "treas"
    image = root / "Build" / "testapp.texb"
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    durations = []

    for _ in range(runs):
        start = time.perf_counter_ns()
        result = subprocess.run(
            [str(launcher), "-b", str(image)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
        )
        elapsed = time.perf_counter_ns() - start
        if result.returncode != 0:
            sys.stderr.buffer.write(result.stderr)
            return result.returncode
        durations.append(elapsed / 1_000_000)

    durations.sort()
    print(f"runs:   {runs}")
    print(f"median: {statistics.median(durations):.3f} ms")
    print(f"p95:    {percentile(durations, 95):.3f} ms")
    print(f"min:    {durations[0]:.3f} ms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
