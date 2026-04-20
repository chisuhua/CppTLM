# Stress Test Results

This directory contains output from Phase 8 Phase 5 stress tests.

## Directory Structure

```
test/stress_results/
├── README.md                    # This file
├── hotspot_2cpu/               # HOTSPOT pattern test results
│   ├── metrics.txt            # Text format metrics
│   ├── metrics.json            # JSON format metrics
│   └── metrics.md              # Markdown format metrics
├── strided_cache/             # STRIDED pattern test results
│   └── ...
├── random_full/               # RANDOM pattern test results
│   └── ...
├── mixed_4cpu/                # MIXED multi-pattern test results
│   └── ...
├── backpressure/              # Backpressure test results
│   └── ...
└── full_system/              # Full system test results
    └── ...
```

## Interpreting Results

### Text Format (metrics.txt)

gem5-style aligned column output with comments:

```
---------- Begin Simulation Statistics ----------
system.cpu.requests                           10000  # Total requests issued (count)
system.cpu.latency.mean                         45  # Average latency (cycle)
system.cpu.latency.max                        380  # Max latency (cycle)
---------- End Simulation Statistics ----------
```

### JSON Format (metrics.json)

Nested JSON with full precision:

```json
{
  "cpu": {
    "requests": 10000,
    "latency": {
      "count": 10000,
      "mean": 45.3,
      "min": 10,
      "max": 380
    }
  }
}
```

### Markdown Format (metrics.md)

Human-readable tables + embedded JSON:

```markdown
# Performance Metrics Report

| Metric | Value | Unit |
|--------|-------|------|
| Requests | 10,000 | count |
| Avg Latency | 45.3 | cycle |

## JSON Format

```json
{...}
```
```

## Test Scenarios

| Scenario | Pattern | Duration | Verification |
|----------|---------|----------|--------------|
| hotspot_2cpu | HOTSPOT (80/20) | 100K cycles | Hotspot latency > uniform |
| strided_cache | STRIDED (64B stride) | 100K cycles | Cache miss rate > 50% |
| random_full | RANDOM uniform | 500K cycles | No packet loss, stable latency |
| mixed_4cpu | MIXED (4 patterns) | 1M cycles | System stable |
| backpressure | SEQUENTIAL burst | 100K cycles | Backpressure works |

## CI Integration

Stress tests run automatically in GitHub Actions on every push to main/develop.

See `.github/workflows/ci.yml` for the stress test configuration.

Test timeout: 5 minutes maximum per scenario.