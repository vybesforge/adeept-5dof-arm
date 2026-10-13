# Adeept 5DOF Robotic Arm — serial control firmware (Apache 2.0)

Serial control for the Adeept 5DOF arm (ADA031, ATmega328P / "Arduino UNO R3
compatible"), driven from a Linux box over USB (CH340 → `/dev/ttyUSB0`).

Two firmware sketches:

- **`PepperArmFirmware/PepperArmFirmware.ino`** — the improved firmware:
  absolute positioning, JSON state replies, a knob-lock so the physical dials
  can't fight the computer, and home/stop/query commands.
- **`AdeeptArmRobot/AdeeptArmRobot.ino`** — the stock serial firmware from the
  ADA031 kit (1°-per-press, no feedback), kept for reference and as the
  factory-compatible baseline.

## Why the improved firmware

The stock `AdeeptArmRobot` firmware has four gaps that make computer control
fragile:

1. **No absolute positioning** — only 1°-per-press steps, no way to set an
   exact angle, no feedback.
2. **No stop command** — a move can't be interrupted cleanly.
3. **The physical knobs fight the computer** — the drive board's pots map to
   the same servo pins, so a hand on a knob reverts a programmatic move.
4. **No state readout** — you can never ask where the arm is.

`PepperArmFirmware` fixes all four.

## Protocol (PepperArmFirmware)

9600 baud, newline-terminated commands, one-line JSON replies:

| Command | Meaning | Reply |
|---|---|---|
| `M<servo>,<angle>[,<servo>,<angle>...]` | Set absolute servo positions (servo 1-5, angle 0-180) | `{"ok":true,"pos":[a1,a2,a3,a4,a5]}` |
| `Q` | Query current positions | `{"ok":true,"pos":[a1,a2,a3,a4,a5]}` |
| `H` | Home all servos to 90° (neutral) | `{"ok":true,"pos":[90,90,90,90,90]}` |
| `L` | Lock: ignore knob input (computer has control) | `{"ok":true,"locked":true}` |
| `U` | Unlock: allow knob input | `{"ok":true,"locked":false}` |
| `S` | Stop (ack, ends current command) | `{"ok":true,"stopped":true}` |

Knob lock is **ON by default after boot**, so the physical dials can't revert
moves. The gripper clamps to its 35-90° range; other servos clamp 0-180°.

## Servo pin map (same as stock)

- servo1 = pin 9 (base)
- servo2 = pin 6 (shoulder)
- servo3 = pin 5 (elbow)
- servo4 = pin 3 (wrist)
- servo5 = pin 11 (gripper, 35-90 range)

## Flashing

```bash
arduino-cli core install arduino:avr
arduino-cli lib install Servo
arduino-cli compile --fqbn arduino:avr:uno --output-dir build PepperArmFirmware
arduino-cli upload --fqbn arduino:avr:uno -p /dev/ttyUSB0 PepperArmFirmware
```

The UNO bootloader opens a window after the CH340's DTR auto-reset, so upload
from an idle port works. Reversible: flash the kit's `Learning.ino` example
back to restore the factory OLED+dial behavior.

## Notes

- The arm can ship running the factory `Potentiometer_control` firmware, which
  has **zero `Serial.` code** — it only reads the knobs and ignores every
  serial byte. If the arm holds position but no command moves it, flash the
  serial firmware first.
- There is no position feedback on the stock hardware — drive by command and
  verify with a camera, don't try to read an angle back.
- Home/reset: toggle DTR (Arduino UNO auto-reset) → board reboots, all servos
  home to 90°.

## License

Apache 2.0.
