# 🔧 ESP32 Motor Tuning Guide

## Quick Reference - Serial Commands

Open **Arduino Serial Monitor** at 115200 baud, then use these single-key commands:

| Command | Action |
|---------|--------|
| `1` `2` `3` | Select slot (Para/Amox/Ceti) |
| `t` | **Test dispense** - full cycle |
| `m` | Manual pulse (100ms) |
| `s` | Emergency stop |
| `+` `-` | Adjust speed ±10 |
| `c` `n` `f` | Set Crawl/Normal/Fast speed |
| `[` `]` | Adjust run time ±50ms |
| `{` `}` | Adjust pause ±50ms |
| `p` | Print current settings |
| `h` | Show help |

---

## Calibration Procedure

### Step 1: Find Minimum Starting Speed
1. Press `1` to select Paracetamol slot
2. Press `c` to set CRAWL speed (80 / 30%)
3. Press `m` for a quick pulse
4. **Watch the motor** - does it move?
5. If NO movement: Press `+` to increase speed, repeat pulse
6. Find the **minimum speed where motor reliably starts**

### Step 2: Tune Run Time
1. Set speed to NORMAL with `n`
2. Press `t` to test full cycle
3. **Count tablets dispensed:**
   - **Too many?** → Press `[` to decrease run time
   - **Not enough/incomplete?** → Press `]` to increase run time
4. Repeat until ONE tablet per cycle

### Step 3: Tune Pause Time
1. After `t` test, watch the spring
2. **Spring still vibrating?** → Press `}` to increase pause
3. **Spring stable quickly?** → Press `{` to decrease pause (faster overall)

### Step 4: Repeat for Each Slot
Test all 3 slots (`1`, `2`, `3`) as spring tension may vary.

---

## Default Settings

| Parameter | Value | Description |
|-----------|-------|-------------|
| Speed | 120 (47%) | Safe starting point |
| Ramp Up | 300 ms | Gentle acceleration |
| Run Time | 500 ms | **Main tuning target** |
| Ramp Down | 250 ms | Smooth stop |
| Pause | 400 ms | Spring settle time |
| **Total** | ~1450 ms | Per tablet |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Motor doesn't move | Increase speed (`+`), check wiring |
| Dispenses 2+ tablets | Decrease run time (`[`) |
| Incomplete dispense | Increase run time (`]`) |
| Spring bouncing | Increase pause (`}`) |
| Too slow overall | Decrease pause (`{`), increase speed |
| Motor jerky | Already using soft start/stop |

---

## After Tuning

Once you find optimal values, update these in `esp32_dispenser.ino`:

```cpp
int targetSpeed = YOUR_VALUE;     // e.g., 100
int runTimeMs = YOUR_VALUE;       // e.g., 450
int pauseBetweenMs = YOUR_VALUE;  // e.g., 350
```

---

## PWM Speed Reference

| Value | Duty % | Use Case |
|-------|--------|----------|
| 80 | 30% | Very slow, precision |
| 100 | 40% | Slow, controlled |
| 120 | 47% | **Default** |
| 150 | 60% | Fast |
| 180 | 70% | Too fast for springs |
| 255 | 100% | Never use! |
