# Pitch Shift Effect Flowchart

This document describes the flow of the `pitch_shift_effect_process()` function in `main/effects/pitch_shift_effect.c`.

## Process Flow

```
┌─────────────────────────────────────┐
│           START                     │
│   (input_left, input_right)         │
└─────────────────┬───────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │  config.enable?     │───No──► Return (pass through)
        └─────────┬───────────┘
                  │ Yes
                  ▼
        ┌─────────────────────┐
        │  Buffer allocated?  │───No──► Output = Input, Return
        └─────────┬───────────┘
                  │ Yes
                  ▼
        ┌─────────────────────┐
        │  pitch_ratio > 0?   │───No──► Output = Input, Return
        └─────────┬───────────┘
                  │ Yes
                  ▼
┌─────────────────────────────────────┐
│  WRITE INPUT TO CIRCULAR BUFFER     │
│  buffer[write_pos] = left           │
│  buffer[write_pos+1] = right        │
│  write_pos += 2 (wrap if needed)    │
└─────────────────┬───────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │  Buffer primed?     │
        └─────────┬───────────┘
                  │ No
                  ▼
        ┌─────────────────────┐
        │ samples_written <   │───Yes──► Output = Input, Return
        │ min_samples_to_prime│         (still filling buffer)
        └─────────┬───────────┘
                  │ No (enough samples)
                  ▼
┌─────────────────────────────────────┐
│  INITIALIZE READ POSITION           │
│  Set read_pos behind write_pos      │
│  by initial_distance                │
│  (scaled by pitch_ratio if > 1.0)   │
│  pitch_buffer_primed = true         │
└─────────────────┬───────────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│  ADVANCE READ POSITION              │
│  read_pos += 2.0 * pitch_ratio      │
│  (wrap to buffer bounds)            │
└─────────────────┬───────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │  Calculate distance │
        │  between write and  │
        │  read positions     │
        └─────────┬───────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │ distance <          │───No───────────────────┐
        │ min_buffer_distance?│                        │
        └─────────┬───────────┘                        │
                  │ Yes                                │
                  ▼                                    │
        ┌─────────────────────┐                        │
        │  distance < 100?    │───No──────────────────┐│
        └─────────┬───────────┘                       ││
                  │ Yes (critical)                    ││
                  ▼                                   ││
┌─────────────────────────────────────┐               ││
│  CROSSFADE PREPARATION              │               ││
│  Save current sample values         │               ││
│  crossfade_progress = 0             │               ││
│  (start fade transition)            │               ││
└─────────────────┬───────────────────┘               ││
                  │                                   ││
                  ▼                                   ││
┌─────────────────────────────────────┐               ││
│  RESET READ POSITION                │               ││
│  Move read_pos back to safe         │               ││
│  distance from write_pos            │               ││
└─────────────────┬───────────────────┘               ││
                  │                                   ││
                  ▼                                   ││
        ┌─────────────────────┐                       ││
        │  Still distance     │                       ││
        │  < 100?             │───Yes──► Output=Input ││
        └─────────┬───────────┘          Return       ││
                  │ No                                ││
                  ▼◄──────────────────────────────────┘│
                  ▼◄───────────────────────────────────┘
┌─────────────────────────────────────┐
│  LINEAR INTERPOLATION               │
│  read_idx = floor(read_pos)         │
│  fraction = read_pos - read_idx     │
│  next_idx = (read_idx+2) % buf_size │
│                                     │
│  out_left = left1 + (left2-left1)   │
│             * fraction              │
│  out_right = right1 + (right2-right1)│
│              * fraction             │
└─────────────────┬───────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │ crossfade_progress  │───No (=1.0)───┐
        │ < 1.0?              │               │
        └─────────┬───────────┘               │
                  │ Yes                       │
                  ▼                           │
┌─────────────────────────────────────┐       │
│  APPLY CROSSFADE                    │       │
│  Blend old sample with new:         │       │
│  out = old*(1-progress) +           │       │
│        new*progress                 │       │
│  progress += 1/CROSSFADE_SAMPLES    │       │
└─────────────────┬───────────────────┘       │
                  │                           │
                  ▼◄──────────────────────────┘
┌─────────────────────────────────────┐
│  OUTPUT RESULT                      │
│  *left_sample = out_left            │
│  *right_sample = out_right          │
└─────────────────┬───────────────────┘
                  │
                  ▼
               [ END ]
```

## Key Concepts

| Component | Purpose |
|-----------|---------|
| **Circular Buffer** | Stores incoming audio samples for time-stretching |
| **Read/Write Positions** | Write advances at 1x speed; read advances at `pitch_ratio` speed |
| **Priming Phase** | Fills buffer before processing to prevent reading empty data |
| **Distance Check** | Ensures read position doesn't catch up to write position |
| **Linear Interpolation** | Smooths output when reading between sample indices |
| **Crossfade** | Prevents clicks when read position is forcibly reset |

## Pitch Shift Logic

- **pitch_ratio > 1.0** → Read faster than write → **Higher pitch**
- **pitch_ratio < 1.0** → Read slower than write → **Lower pitch**
- **pitch_ratio = 1.0** → Read at same speed → **No change**

## Buffer Configuration

The effect uses adaptive buffer sizing based on available memory:

| Buffer Size | Duration | Memory Usage |
|-------------|----------|--------------|
| 50ms | Preferred (fewer clicks) | 76,800 bytes |
| 25ms | Fallback | 38,400 bytes |
| 12.5ms | Smaller fallback | 19,200 bytes |
| 6.25ms | Minimum viable | 9,600 bytes |

Buffer allocation priority:
1. PSRAM (external RAM)
2. Internal RAM
3. Default heap

