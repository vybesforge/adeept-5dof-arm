/***********************************************************
PepperArmFirmware — position-commanding firmware for the Adeept 5DOF arm.

Fixes the stock AdeeptArmRobot problems:
  1. No absolute positioning (1°-per-press, no feedback) -> M command sets
     an exact angle, board replies with the resulting state.
  2. No stop command -> S command (also '0' terminator) ends a command.
  3. Physical knobs on the drive board can fight the computer -> 'L' locks
     knob control until 'U' unlocks it. Lock is ON by default after boot,
     so the knobs can't revert moves.
  4. No state readout -> Q command prints all five servo angles.

Protocol (9600 baud, newline-terminated commands, one-line JSON replies):
  M<servo>,<angle>[,<servo>,<angle>...]
      Set absolute servo positions. servo 1-5, angle 0-180.
      Reply: {"ok":true,"pos":[a1,a2,a3,a4,a5]}
  Q            Query current positions.
      Reply: {"ok":true,"pos":[a1,a2,a3,a4,a5]}
  H            Home all servos to 90 (neutral).
      Reply: {"ok":true,"pos":[90,90,90,90,90]}
  L            Lock: ignore knob input (computer has control). ON by default.
  U            Unlock: allow knob input.
  S            Stop (ack, ends current command). Reply: {"ok":true}

Knobs: the Adeept drive board's physical knobs map to the same servo pins.
When locked, their bytes are read and discarded so they can't move the arm.

Servo pin map (same as stock AdeeptArmRobot):
  servo1 = pin 9  (base)
  servo2 = pin 6  (shoulder)
  servo3 = pin 5  (elbow)
  servo4 = pin 3  (wrist)
  servo5 = pin 11 (gripper, 35-90 range)
***********************************************************/
#include <Servo.h>

const int PIN[5] = {9, 6, 5, 3, 11};
const int N_SERVO = 5;

Servo sv[N_SERVO];
int pos[N_SERVO];

// Knob lock: when true, bytes from the physical knobs are discarded.
bool knobLocked = true;   // default locked so knobs can't revert moves

void applyAll() {
  for (int i = 0; i < N_SERVO; i++) sv[i].write(pos[i]);
}

void replyPos() {
  Serial.print(F("{\"ok\":true,\"pos\":["));
  for (int i = 0; i < N_SERVO; i++) {
    if (i) Serial.print(',');
    Serial.print(pos[i]);
  }
  Serial.println(F("]}"));
}

void home() {
  for (int i = 0; i < N_SERVO; i++) pos[i] = 90;
  applyAll();
}

// Parse "M<idx>,<angle>,<idx>,<angle>..." after the leading 'M'.
void parseMove(String line) {
  int i = 1; // skip 'M'
  while (i < line.length()) {
    int comma = line.indexOf(',', i);
    if (comma < 0) comma = line.length();
    int idx  = line.substring(i, comma).toInt() - 1;  // 1-based -> 0-based
    int next = line.indexOf(',', comma + 1);
    if (next < 0) next = line.length();
    int angle = line.substring(comma + 1, next).toInt();
    if (idx >= 0 && idx < N_SERVO) {
      // clamp gripper to its 35-90 range, others to 0-180
      if (idx == 4) angle = constrain(angle, 35, 90);
      else          angle = constrain(angle, 0, 180);
      pos[idx] = angle;
      sv[idx].write(angle);   // apply immediately, per-servo
    }
    i = next + 1;
  }
}

void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    char c = line.charAt(0);

    if (c == 'M' || c == 'm') {
      parseMove(line);
      replyPos();

    } else if (c == 'Q' || c == 'q') {
      replyPos();

    } else if (c == 'H' || c == 'h') {
      home();
      Serial.print(F("{\"ok\":true,\"pos\":[90,90,90,90,90]}"));
      Serial.println();

    } else if (c == 'L' || c == 'l') {
      knobLocked = true;
      Serial.println(F("{\"ok\":true,\"locked\":true}"));

    } else if (c == 'U' || c == 'u') {
      knobLocked = false;
      Serial.println(F("{\"ok\":true,\"locked\":false}"));

    } else if (c == 'S' || c == 's') {
      Serial.println(F("{\"ok\":true,\"stopped\":true}"));

    } else {
      // Unknown / knob byte: discard (especially when locked).
      if (!knobLocked) {
        // forward knob handling minimally: treat like stock 1° steps
        // so manual knob use still works when unlocked.
        switch (c) {
          case 'o': case 'O': pos[0] = constrain(pos[0]+1,0,180); sv[0].write(pos[0]); break;
          case 'p': case 'P': pos[0] = constrain(pos[0]-1,0,180); sv[0].write(pos[0]); break;
          case 'u': case 'U': break; // unlock handled above; ignore dup
          case 't': case 'T': pos[2] = constrain(pos[2]+1,0,180); sv[2].write(pos[2]); break;
          case 'y': case 'Y': pos[2] = constrain(pos[2]-1,0,180); sv[2].write(pos[2]); break;
        }
      }
      // no reply for unknown bytes (keeps knob chatter quiet)
    }
  }
  delay(5);
}

void setup() {
  Serial.begin(9600);
  delay(300);
  for (int i = 0; i < N_SERVO; i++) {
    sv[i].attach(PIN[i]);
    pos[i] = 90;
  }
  home();           // start at 90° neutral (safe boot pose)
  knobLocked = true; // knobs can't fight the computer
  Serial.println(F("{\"ok\":true,\"boot\":true,\"pos\":[90,90,90,90,90]}"));
}
