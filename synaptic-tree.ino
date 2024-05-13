#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SPIDevice.h>
#include <Adafruit_Soundboard.h>
#include <Adafruit_ZeroFFT.h>
#include <Wire.h>
#include <arduinoFFT.h>
#include <arm_common_tables.h>
#include <elapsedMillis.h>

// tree modes, everything after MODE_QTY is ignored
enum mode {
  AMBIENT,
  THINK,
  VISUALIZE,
  LIGHTNING,
  HEARTBEAT,
  GAME,
  TEST,
  MODE_QTY
};
// ambient modes, everything after AMDIENTMODE_QTY is ignored
enum ambientmode { 
  RAINBOW,
  SPARKLE,
  AMBIENTMODE_QTY,
  TWINKLE,
  STRIPE,
  FIRE,
  METEOR
};
// test cases, everything after TESTCASE_QTY is ignored
enum testcase {
  ALL,
  ANIMATION,
  PAD_LIGHTS,
  LED_PULSE,
  BRANCH_STEP,
  TOUCH,
  AUDIO,
  VISUALIZER,
  PRINT_PATHS,
  SOUNDFILES,
  GAMEWIN,
  TESTCASE_QTY
};

// Set the active modes here
mode defmode = AMBIENT;
mode altmodes[] = { HEARTBEAT, LIGHTNING };
ambientmode currambientmode = RAINBOW;
testcase currtest = GAMEWIN;

// general settings
#define TESTSTRIP true
#define ZEROFFT true
#define TOUCH_ACTIVE true
#define SFX_ACTIVE true
#define MIC_ACTIVE false
#define PLAY_INTRO false
#define XMAS false

// Needed for touchpad support
#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// C arrays have no size method
#define ARRAY_SIZE(array) ((sizeof(array)) / (sizeof(array[0])))

// physical limits
#define ALLSTRIPS 27
#define BASESTRIPS 5
#define TOUCHPADS 5
#define BRIGHTNESS 50
#define RAINBOWDIMMING 0.05
#define GAMELIMIT 10
#define DEFMODE_MAXDUR 30
#define ALTMODE_MAXDUR 10

// useful colors
#define RED 0
#define PURPLE 1
#define BLUE 2
#define GREEN 3
#define ORANGE 4
#define WHITE 5
#define BLANK 6

// useful tone frequencies
#define NOTE_B0 31
#define NOTE_C1 33
#define NOTE_CS1 35
#define NOTE_D1 37
#define NOTE_DS1 39
#define NOTE_E1 41
#define NOTE_F1 44
#define NOTE_FS1 46
#define NOTE_G1 49
#define NOTE_GS1 52
#define NOTE_A1 55
#define NOTE_AS1 58
#define NOTE_B1 62
#define NOTE_C2 65
#define NOTE_CS2 69
#define NOTE_D2 73
#define TR_TONE0 = 0
#define TR_TONE1 = 1
#define TR_TONE2 = 2
#define TR_TONE3 = 3
#define TR_TONE4 = 4
#define TR_WIN = 5
#define TR_FAIL = 6
#define TR_STARTUP = 7
#define TR_ACTIVATE = 8
#define TR_LIGHTNING = 9
#define TR_HEARTBEAT = 10

// choose any two pins that can be used with SoftwareSerial to RX & TX
#define SFX_TX 1
#define SFX_RX 0

// connect to the RST and ACT pin on the Sound Board
#define SFX_RST 16
#define SFX_ACT 17

// for visualizer
#define SAMPLES 512  // Must be a power of 2
#define MAXSAMPLEVALUE 450
#define MIC_IN A15    // Use A14 or A15 for mic input
#define LONGESTEQ 60  // Total number of  rows in the display

// for easy reference
enum sound_ids {
  ACTIVATE,
  REDPAD,
  PURPLEPAD,
  BLUEPAD,
  GREENPAD,
  ORANGEPAD,
  WIN,
  LOSE,
  BEAT,
  STRIKE,
  SPARKLESOUND,
  TWINKLESOUND,
  THINKSOUND,
  STARTUP
};

// sounds for the audio board
struct sound {
    char name[21];
    char pname[12];
    int pin;
} sounds[] = {
  { "activate",  "T05     WAV", 67 },
  { "redpad",    "T07     WAV", 68 },
  { "greenpad",  "T08     WAV", 69 },
  { "bluepad",   "T09     WAV", 70 },
  { "orangepad", "T10     WAV", 71 },
  { "purplepad", "T11     WAV", 72 },
  { "win",       "T06     WAV", 73 },
  { "lose",      "T00     WAV", 74 },
  { "beat",      "T04     WAV", 54 },
  { "strike",    "T01     WAV", 55 },
  { "sparkle",   "T02     WAV", 56 },
  { "twinkle",   "T12     WAV", 56 }, // not used
  { "think",     "T13     WAV", 56 }, // not used
  { "startup",   "T14     WAV", 56 }  // not used
};

// sound array for gamepad
sound gamenotes[] = {
  sounds[REDPAD],
  sounds[PURPLEPAD],
  sounds[BLUEPAD],
  sounds[GREENPAD],
  sounds[ORANGEPAD]
 };

// touchpad lights order
int padlights[][6] = { 
  { 18, 19, 20, 21, 22, 23 },
  { 24, 25, 26, 27, 28, 29 },
  { 0, 1, 2, 3, 4, 5 },
  { 6, 7, 8, 9, 10, 11 },
  { 12, 13, 14, 15, 16, 17 }
};

// game states
enum gamestate { PROMPT, RESPONSE };

// pin number of each led strip
int ledpins[] = {
  22,  // branch 0 (on A, aligned under base of split leg on B)
  24,  // branch 1 (on A, next one clockwise looking down on segment)
  26,  // branch 2 (on A, next one clockwise looking down on segment)
  28,  // branch 3 (on A, next one clockwise looking down on segment)
  30,  // branch 4 (on A, next one clockwise looking down on segment)
  32,  // branch 5
  34,  // branch 6
  36,  // branch 7
  38,  // branch 8
  40,  // branch 9
  23,  // branch 10
  25,  // branch 11
  27,  // branch 12
  29,  // branch 13
  31,  // branch 14 
  33,  // branch 15 
  35,  // branch 16
  37,  // branch 17
  39,  // branch 18
  41,  // branch 19
  42,  // branch 20
  44,  // branch 21
  46,  // branch 22
  43,  // branch 23
  45,  // branch 24
  47,  // branch 25
  48   // branch 26
};

// padlights pin
int padlight_pin = 49;

// all segment sizes (actual pixels) for reference
// A0:72, A1:72, A2:72, A3:72, A4:72
// B0:60, B1:66, B2:60, B3:72, B4:72, B5:72, B6:72, B7:60, B8:66
// C0:18, C1:24, C2:30, C3:30, C4:30, C5:30, C6:30, C7:30, C8:24
// D0:30, D1:42, D2:72, D3:66, D4:66, D5:66, D6:36, D7:30
// E0:36, E1:36, E2:66, E3:72, E4:66, E5:66, E6:42, E7:24
// F0:30, F1:36, F2:60, F3:66, F4:66, F5:66, F6:42, F7:30
// G0:30, G1:36, G2:72, G3:72, G4:66, G5:36, G6:30, G7:30
// H0:66, H1:66, H2:66
// I0:72, I1:66, I2:66, I3:66, I4:66
// J0:66, J1:66, J2:72, J3:66, J4:72
// K0:66, K1:66, K2:66
// L0:90, L1:84, L2:90
// M0:66, M1:72, M2:72
// N0:66, N1:66, N2:72
// O0:66, O1:66, O2:72
// P0:66, P1:66, P2:72
// Q0:90, Q1:90, Q2:84
// R0:72, R1:66, R2:66

// the number of pixels per segment in order. 0/null means no segment
// segments are individually cut LED strips, identified by the tree segment
// letter and the strip number starting from 0 at the base of the split leg then
// increasing clockwise (G is an exception where it has two strips at base of
// split so left strip is 0
int segments[][6] = {
  { 12, 10, 12, 11, 11 },     // branch 0  (A0, B0, D2, I1, N0)
  { 12, 12, 5, 11, 5, 15 },   // branch 1  (A1, B3, C6, F4, G0, Q0)
  { 12, 12, 3, 11, 11, 11 },  // branch 2  (A2, B4, C0, E2, J1, O1)
  { 12, 12, 5, 5, 11, 11 },   // branch 3  (A3, B5, C3, F0, K0, P0)
  { 12, 12, 5, 10, 11, 11 },  // branch 4  (A4, B6, C4, F2, G4, R2)
  { 11, 11, 11, 11 },         // branch 5  (B1, D3, I2, N1)
  { 10, 5, 11, 5 },           // branch 6  (B2, C5, F3, G6, Q0-merge)
  { 10, 11, 11, 12 },         // branch 7  (B7, D5, I4, N2)
  { 11, 5, 11, 11 },          // branch 8  (B8, D0, H0, M0)
  { 4, 12, 12 },              // branch 9  (C1, E3, J2, O1-merge)
  { 5, 11, 11, 12 },          // branch 10 (C2, E4, J3, O2)
  { 5, 11, 12 },              // branch 11 (C7, E5, J4, O0-merge)
  { 4, 6, 15 },               // branch 12 (C8, E0, L0)
  { 7, 11, 12 },              // branch 13 (D1, H1, M1)
  { 11, 11 },                 // branch 14 (D4, I3, N1-merge)
  { 5, 12 },                  // branch 15 (D7, I0, N0-merge)
  { 6, 11, 12 },              // branch 16 (D6, H2, M2)
  { 6, 14 },                  // branch 17 (E1, L1)
  { 7, 15 },                  // branch 18 (E6, L2)
  { 4, 11, 11 },              // branch 19 (E7, J0, O0)
  { 6, 11, 11 },              // branch 20 (F1, K1, P1)
  { 11, 12, 11 },             // branch 21 (F5, G2, R1)
  { 7, 11, 12 },              // branch 22 (F6, K2, P2)
  { 5, 12 },                  // branch 23 (F7, G3, R1-merge)
  { 6, 15 },                  // branch 24 (G1, Q1)
  { 6, 14 },                  // branch 25 (G5, Q2)
  { 5, 12 }                   // branch 26 (G7, R0)
};

// source branch number, the segment number before split, and which branch it
// splits to there can be multiple cases per same source branch
int splits[][3] = {
  { 1, 0, 5 },    // branch A1 --> B1
  { 1, 0, 6 },    // branch A1 --> B2
  { 4, 0, 7 },    // branch A4 --> B7
  { 4, 0, 8 },    // branch A4 --> B8
  { 3, 1, 9 },    // branch A3 --> C1
  { 3, 1, 10 },   // branch A3 --> C2
  { 1, 1, 11 },   // branch A1 --> C7
  { 1, 1, 12 },   // branch A1 --> C8
  { 0, 1, 13 },   // branch A0 --> D1
  { 6, 0, 14 },   // branch B2 --> D4
  { 4, 1, 15 },   // branch A4 --> D6
  { 13, 1, 15 },  // branch D1 --> D7
  { 15, 1, 16 },  // branch D6 --> D7
  { 2, 2, 17 },   // branch A2 --> E1
  { 11, 2, 18 },  // branch C7 --> E6
  { 17, 2, 19 },  // branch E1 --> E7
  { 18, 2, 19 },  // branch E6 --> E7
  { 4, 2, 20 },   // branch A4 --> F1
  { 10, 2, 22 },  // branch C2 --> F6
  { 11, 2, 22 },  // branch C7 --> F6
  { 10, 2, 21 },  // branch C2 --> F5
  { 11, 2, 21 },  // branch C7 --> F5
  { 20, 2, 23 },  // branch F1 --> F7
  { 22, 2, 23 },  // branch F6 --> F7
  { 21, 3, 24 },  // branch F5 --> G1
  { 4, 3, 25 },   // branch A4 --> G5
  { 24, 3, 26 },  // branch G1 --> G7
  { 25, 3, 26 }   // branch G5 --> G7
};

// starting branch number, branch and segment number that it merges into
int merges[][3] = {
  { 14, 5, 3 },   // branch 26  (D4, I3, N1-merge)
  { 9, 2, 5 },    // branch 9  (C1, E2, J2, O1-merge)
  { 11, 19, 2 },  // branch 11 (C7, E5, J4, O0-merge)
  { 16, 0, 4 },   // branch 15 (D7, I0, N0-merge)
  { 23, 21, 2 },  // branch 22 (F7, G3, R1-merge)
  { 6, 1, 5 }     // branch 6 (B2, C5, F3, G6, Q0-merge)
};

// pin for audio tones
int tonepin = 13;

// keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
elapsedMillis elapsetouched[TOUCHPADS];     // elapsed time while touched
elapsedMillis elapsenottouched[TOUCHPADS];  // elapsed time while not touched
elapsedMillis elapsedVisualize, elapsedAmbient, elapsedThinking,
  elapsedLightning, elapsedHeartbeat, elapsedLastBeat, elapsedLastStrike, elapsedSound, elapsedSerial;
int ambientdur, visualizedur, heartbeatdur, lightningdur, thinkingdur;
int idle_timer = 0;
int gamesteps[GAMELIMIT][5];  // when you reach this limit you win the game
int gamecount = 0;
int trycount = 0;
mode currmode = defmode;
gamestate defstate = PROMPT;
gamestate currstate = defstate;
int timeoutcount = 0;
int thought_qty = random(1, 1);
int strikeWait = 3000;
int flame_size = 10;

// keep track of current animation step
int pulse_count[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int rainbowHue = 0;
int twinkleStep = 0;
int meteorStep = 0;

Adafruit_NeoPixel strips[ALLSTRIPS];
Adafruit_NeoPixel padstrip;

#if TESTSTRIP
// pixel colors are RGB
uint colors[] = {
  strips[0].Color(255, 0, 0),      // red
  strips[0].Color(255, 0, 255),    // purple
  strips[0].Color(0, 0, 255),      // blue
  strips[0].Color(0, 255, 0),      // green
  strips[0].Color(255, 164, 0),    // orange
  strips[0].Color(255, 255, 255),  // white
  strips[0].Color(0, 0, 0)         // blank
};
#else
// pixel colors are BGR
uint colors[] = {
  strips[0].Color(0, 0, 255),      // red
  strips[0].Color(255, 0, 255),    // purple
  strips[0].Color(255, 0, 0),      // blue
  strips[0].Color(0, 255, 0),      // green
  strips[0].Color(0, 164, 255),    // orange
  strips[0].Color(255, 255, 255),  // white
  strips[0].Color(0, 0, 0)         // blank
};
#endif

// in case where tones are used instead of soundboard FX
uint gametones[] = { NOTE_E1, NOTE_G1, NOTE_B1, NOTE_D1, NOTE_F1 };

int paths[5][5][3];
// uint16_t veins[1][400][2];
struct pathPosition {
  int strip, pixel;
};
typedef struct pathPosition Struct;
Struct get_path_position(int path, int index);
void playsound(sound s, bool force = true);
void triggersound(sound s);
void gamestart();
void run_prompt(int speed = 1000);
void gamewin(mode nextmode = defmode);
void gamefail(mode nextmode = defmode);
void gen_path(int target, int start = random(5), bool overlap = false);
void gen_multipath(int count);
void pulse_step(int path, int index, uint32_t color1 = colors[BLUE],
                uint32_t color2 = colors[PURPLE]);
void pulse_step_simple(int strip, int index, uint32_t color1 = colors[BLUE],
                       uint32_t color2 = colors[PURPLE]);
void branch_step(int base_strip, int index, uint32_t color = colors[WHITE],
                 bool overlap = false);
void fill_path(int path, uint32_t color = colors[WHITE]);
void fill_branch(int base_strip, uint32_t color = colors[WHITE],
                 bool overlap = false);
void fill_branch_part(int base_strip, int index,
                      uint32_t color = colors[WHITE]);
void fill_padlights();
void clear_tree();
void sparkle(uint32_t color1 = colors[WHITE], uint32_t color2 = colors[WHITE], int wait = 0);
void stripe(uint32_t color1 = colors[WHITE], uint32_t color2 = colors[WHITE], int wait = 0);
void twinkleRandom(int SpeedDelay = 50, boolean OnlyOne = false);
void rainbow(int hue = 0, int wait = 0);
void fire(int Cooling = 55, int Sparking = 120, int SpeedDelay = 15);
void setPixelHeatColor(int strip, int Pixel, byte temperature);
void meteorRain(int meteorStep, byte red = 0xff, byte green = 0xff,
                byte blue = 0xff, byte meteorSize = 10,
                byte meteorTrailDecay = 64, boolean meteorRandomDecay = true,
                int SpeedDelay = 30);
void fadeToBlack(int strip, int ledNo, byte fadeValue);
void visualize();
void getSamples();
void displayUpdate();
void ambient();
void think(int thoughts = thought_qty);
void strike(int paths = random(1, 3), uint32_t color = colors[WHITE]);
void beat(int speed = 500, uint32_t color = colors[RED]);
void change_mode(mode new_mode = defmode);
void test();

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

// create soundboard object
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);

// visualizer setup
#if ZEROFFT
int16_t vReal[SAMPLES];
int16_t vImag[SAMPLES];
#else
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT();  // Create FFT object
#endif
int Intensity[BASESTRIPS] = {};  // initialize Frequency Intensity to zero

uint32_t sparkle_color1, sparkle_color2;
uint32_t stripe_color1, stripe_color2;

// Initialize everything and prepare to start
void setup() {
  elapsedSerial = 0;
  Serial.begin(115200);
  while (!Serial && (elapsedSerial < 3000)) { // make sure console output is ready
    delay(10);
  }
  Serial.println("Starting setup...");

  // setup uart for soundboard
  Serial1.begin(9600);
  while (!Serial1) { // make sure soundboard is started
    delay(10);
  }

  // setup soundboard
  if (SFX_ACTIVE) {
    digitalWrite(SFX_RST, LOW);
    pinMode(SFX_RST, OUTPUT);
    pinMode(SFX_ACT, INPUT_PULLUP); // activity pin
    delay(10);
    pinMode(SFX_RST, INPUT);
    delay(1000); // give a bit of time to 'boot up'

    if (!sfx.reset()) {
      Serial.println("Adafruit Sound board not found.");
      while (1)
        ; // freeze on error
    }
    Serial.println("SFX board found!");
    delay(2000);
    for (int i = 0; i < 11; i++) {
      pinMode(sounds[i].pin, INPUT_PULLUP);
    }

  } else {
    Serial.println("SFX board skipped.");
  }

  // startup sound
  if (PLAY_INTRO && SFX_ACTIVE) {
    playsound(sounds[STARTUP]);
  }

  // visualizer input
  if (MIC_ACTIVE) {
    pinMode(MIC_IN, INPUT);
  }

  // set seed for randomizer
  randomSeed(analogRead(A0));

  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (TOUCH_ACTIVE) {
    if (!cap.begin(0x5A)) {
      Serial.println("MPR121 touch sensor not found.");
      while (1)
        ; // freeze on error
    }
    Serial.println("Touch sensor found!");
  } else {
    Serial.println("Touch sensor skipped.");
  }

  // setup all strips
  for (int i = 0; i < ALLSTRIPS; i++) {
    int branchpixels = 0;
    bool hassplits = false;
    for (int j = 0; j < ARRAY_SIZE(segments[i]); j++) {
      branchpixels += segments[i][j];
    }
    strips[i] =
      Adafruit_NeoPixel(branchpixels, ledpins[i], NEO_GRB + NEO_KHZ800);
    strips[i].setBrightness(BRIGHTNESS);
    strips[i].begin();
    strips[i].clear();
    strips[i].show();
    Serial.print("Branch ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(branchpixels);
    Serial.print("p (pin ");
    Serial.print(ledpins[i]);
    Serial.println(")");
  }

  padstrip = Adafruit_NeoPixel(30, padlight_pin, NEO_GRB + NEO_KHZ800);
  padstrip.setBrightness(BRIGHTNESS);
  padstrip.begin();
  padstrip.clear();
  padstrip.show();
  Serial.print("Pad strip: ");
  Serial.print("30p (pin ");
  Serial.print(padlight_pin);
  Serial.println(")");

  // delay for startup sound
  if (PLAY_INTRO && SFX_ACTIVE) {
    delay(6000);
  }

  Serial.println("Setup is done.");

  // go to default mode
  change_mode(defmode);
}

// Main loop
void loop() {
  if (defmode != TEST) {
    // Get the currently touched pads
    currtouched = cap.touched();
    // timeoutcount = 0; // reset timeout count
    for (int i = 0; i < TOUCHPADS; i++) {
      // it if *is* touched and *wasnt* touched before
      if (currtouched & _BV(i)) {
        elapsenottouched[i] = 0;
        if (!(lasttouched & _BV(i))) {
          elapsetouched[i] = 0;
          Serial.print(i);
          Serial.println(" touched");
          if (currmode == GAME) {
            playsound(gamenotes[i]);
            // light up the branch for the current try and the one to the left
            fill_branch(i, colors[i]);
            // strips[i].fill(colors[i]);
            // strips[i].show();
            int striptoleft;
            if (i == (TOUCHPADS - 1)) {
              striptoleft = 0;
            } else {
              striptoleft = i + 1;
            }
            fill_branch(striptoleft, colors[i]);
            delay(600);
            clear_tree();
          }
        }
        // bump out of game mode if any pad touched too long
        if ((elapsetouched[i] > 10000) && (currmode == GAME)) {
          Serial.println("someone touched pad too long");
          elapsetouched[i] = 0;
          gamefail();
        }
      }
      // if it *was* touched and now *isnt*, alert!
      if (!(currtouched & _BV(i))) {
        if (lasttouched & _BV(i)) {
          Serial.print(i);
          Serial.println(" released");
          if ((currmode == GAME) && (currstate == RESPONSE)) {
            if (gamesteps[trycount][0] == i) {
              Serial.println("correct pad touched!");
              // turn off the branch for the current try

              if (trycount < gamecount) {
                trycount++;  // increment try count if successful try
              } else {
                if (gamecount == ARRAY_SIZE(gamesteps) - 1) {
                  gamewin();
                } else {
                  // add another path, run prompt then try again
                  Serial.println("next prompt...");
                  gamecount++;
                  gamesteps[gamecount][0] = random(TOUCHPADS);
                  int speed = 400 / gamecount + 1;
                  run_prompt(speed);
                  trycount = 0;
                }
              }
            } else {
              Serial.println("wrong pad touched, sorry");
              gamefail();
            }
          } else {
            Serial.print("elapsetouched ");
            Serial.println(elapsetouched[i]);
            // go to game mode if any pad touched for 2 sec
            if (elapsetouched[i] > 2000) {
              clear_tree();
              gamestart();
            }
          }
          elapsenottouched[i] = 0;
        }
      }
    }
    // fail game if all touch panels timeout
    if ((currmode == GAME) && (currstate == RESPONSE)) {
      timeoutcount = 0;
      for (int i = 0; i < TOUCHPADS; i++) {
        if (elapsenottouched[i] > 4000) {
          timeoutcount++;
        }
      }
      if (timeoutcount >= TOUCHPADS) {
        Serial.println("All touchpads timed out.");
        gamefail();
      } else {
        if (elapsenottouched[0] % 3 == 0) {
          Serial.print(".");
        }
      }
    }

    // reset touch state
    lasttouched = currtouched;

    // turn pad lights white if not in game
    if (currmode != GAME) {
      padstrip.fill(colors[WHITE]);
      padstrip.show();
    }

    // handle animation cases
    switch (currmode) {
      case THINK:
        // Serial.print("Thinking "); Serial.println(elapsedThinking);
        if (elapsedThinking < thinkingdur) {
          // show next pulse step for each thought path
          for (int i = 0; i < BASESTRIPS; i++) {
            pulse_step_simple(i, pulse_count[i]);
            // check if at end of path, if so then reset to beginning
            if (pulse_count[i] < strips[i].numPixels() - 1) {
              pulse_count[i]++;
            } else {
              strips[i].clear();
              strips[i].show();
              pulse_count[i] = 0;
            }
          }
        } else {
          memset(pulse_count, 0, sizeof pulse_count);
          // if in default mode and there are alternative modes, then pick one
          if ((currmode == defmode) && (ARRAY_SIZE(altmodes) > 0)){
            clear_tree();
            mode next_mode = altmodes[random(ARRAY_SIZE(altmodes))];
            //Serial.print("next altmode: ");
            //Serial.println(next_mode);
            change_mode(next_mode);
          }
          else {
            change_mode(defmode);
          }
        }
        break;
      case VISUALIZE:
        if (elapsedVisualize < visualizedur) {
          getSamples();
          displayUpdate();
        } else {
          // if in default mode and there are alternative modes, then pick one
          if ((currmode == defmode) && (ARRAY_SIZE(altmodes) > 0)) {
            clear_tree();
            mode next_mode = altmodes[random(ARRAY_SIZE(altmodes))];
            //Serial.print("next altmode: ");
            //Serial.println(next_mode);
            change_mode(next_mode);
          }
          else {
            change_mode(defmode);
          }
        }
        break;
      case AMBIENT:
        if (elapsedAmbient < ambientdur) {
          switch (currambientmode) {
            case SPARKLE:
              clear_tree();
              sparkle(sparkle_color1, sparkle_color2);
              playsound(sounds[SPARKLESOUND], false); // loop sound if ended
              break;
            case STRIPE:
              clear_tree();
              stripe(stripe_color1, stripe_color2);
              break;
            case TWINKLE:
              twinkleRandom(100, false);
              twinkleStep++;
              if (twinkleStep >= 10) {
                twinkleStep = 0;
                clear_tree();
              }
              break;
            case FIRE:
              // clear_tree();
              fire(25, 70, 25);
              break;
            case METEOR:
              meteorRain(meteorStep, 0xff, 0xff, 0xff, 2, 64, true, 0);
              meteorStep++;
              if (meteorStep >= 60 * 1.5) {
                meteorStep = 0;
                clear_tree();
              }
              break;
            default:  // RAINBOW
              rainbow(rainbowHue);
              rainbowHue += 256;
              if (rainbowHue >= 5 * 65536) {
                rainbowHue = 0;
              }
              playsound(sounds[TWINKLESOUND], false); // loop sound if ended
          }
        } else {
          // if in default mode and there are alternative modes, then pick one
          if ((currmode == defmode) && (ARRAY_SIZE(altmodes) > 0)) {
            clear_tree();
            mode next_mode = altmodes[random(ARRAY_SIZE(altmodes))];
            //Serial.print("next altmode: ");
            //Serial.println(next_mode);
            change_mode(next_mode);
          }
          else {
            change_mode(defmode);
          }
        }
        break;
      case LIGHTNING:
        if (elapsedLightning < lightningdur) {
          //Serial.print("Mode is lightning: ");
          //Serial.println(elapsedLastStrike);
          if (elapsedLastStrike > random(3000, 6000)) {
            // Serial.print("Strike wait is "); Serial.println(strikeWait);
            strike();
          }
        } else {
          // if in default mode and there are alternative modes, then pick one
          if ((currmode == defmode) && (ARRAY_SIZE(altmodes) > 0)) {
            clear_tree();
            mode next_mode = altmodes[random(ARRAY_SIZE(altmodes))];
            //Serial.print("next altmode: ");
            //Serial.println(next_mode);
            change_mode(next_mode);
          }
          else {
            change_mode(defmode);
          }
        }
        break;
      case HEARTBEAT:
        if (elapsedHeartbeat < heartbeatdur) {
          //Serial.print("Mode is heartbeat: ");
          //Serial.println(elapsedHeartbeat);
          if (elapsedLastBeat > random(2000, 3000)) {
            beat();
          }
        } else {
          // if in default mode and there are alternative modes, then pick one
          if ((currmode == defmode) && (ARRAY_SIZE(altmodes) > 0)) {
            clear_tree();
            mode next_mode = altmodes[random(ARRAY_SIZE(altmodes))];
            //Serial.print("next altmode: ");
            //Serial.println(next_mode);
            change_mode(next_mode);
          }
          else {
            change_mode(defmode);
          }
        }
        break;
      default:
        ;
    }
  }
}

// Returns the Red component of a 32-bit color
uint8_t Red(uint32_t color) {
  return (color >> 16) & 0xFF;
}

// Returns the Green component of a 32-bit color
uint8_t Green(uint32_t color) {
  return (color >> 8) & 0xFF;
}

// Returns the Blue component of a 32-bit color
uint8_t Blue(uint32_t color) {
  return color & 0xFF;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strips[0].Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return strips[0].Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strips[0].Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

// Calculate 50% dimmed version of a color (used by ScannerUpdate)
uint32_t DimColor(uint32_t color) {
  // Shift R, G and B components one bit to the right
  uint32_t dimColor =
    strips[0].Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
  return dimColor;
}

void reset_touchtimeout() {
  for (int i = 0; i < TOUCHPADS; i++) {
    elapsenottouched[i] = 0;
    elapsetouched[i] = 0;
  }
}

void print_array(int *arr) {
  Serial.print("Array of ");
  Serial.print(ARRAY_SIZE(arr));
  Serial.print(": {");
  for (int i = 0; i < ARRAY_SIZE(arr); i++) {
    Serial.print(arr[i]);
    Serial.print(",");
  }
  Serial.println("}");
}

// generates a new path an assigns to the target, assumes start is not a
// duplicate
void gen_path(int target, int start, bool overlap) {
  int currstrip = start;
  int nextstrip;
  int split_options[5][2];  // assume not more than 5 split options for a given
                            // strip
  int path_index = 0;
  int split_endpoint;
  int split_count;
  int path_size = 0;
  bool end = false;
  // clear the previous version of the path
  memset(paths[target], 0, sizeof paths[target]);
  while (!end) {
    // get the available splits for the current strip in the path
    split_count = 0;
    // Serial.print("For strip "); Serial.print(currstrip); Serial.println(":");
    for (int j = 0; j < ARRAY_SIZE(splits); j++) {
      if (splits[j][0] == currstrip) {
        // Serial.print("Split found at "); Serial.println(splits[j][2]);
        split_options[split_count][0] = splits[j][2];  // split branch
        split_options[split_count][1] =
          splits[j][1];  // last segment before split
        split_count++;
      }
    }
    // add one of the split strips to the path, or not if rando_split =
    // split_count
    int rando_split = random(split_count + 1);
    int pixel = 0;
    // if (path_index > 0) {
    //   Serial.print(" -> ");
    // }
    // Serial.print(currstrip);
    //  in the case where there is no avail split or just don't want to split
    if ((split_count == 0) || (split_count == rando_split)) {
      // add current strip up to the end and drop out of loop
      split_endpoint = ARRAY_SIZE(segments[currstrip]) - 1;
      end = true;
      // if (split_count == 0) Serial.println("No splits.");
      // else Serial.println("Ignoring splits.");
    }
    // in the case where there are splits and want to use one
    else {
      // check if any other path is using the next strip
      bool used = false;
      for (int j = 0; j < ARRAY_SIZE(paths); j++) {
        for (int k = 0; k < ARRAY_SIZE(paths[j]); k++) {
          if (paths[j][k][0] == split_options[rando_split][0]) {
            used = true;
          }
        }
      }
      if (!used) {
        // use the split, add current strip up to skip
        split_endpoint = split_options[rando_split][1];
        nextstrip = split_options[rando_split][0];
        // Serial.println("Using a split.");
      } else {
        // add current strip up to the end and drop out of loop
        split_endpoint = ARRAY_SIZE(segments[currstrip]) - 1;
        end = true;
      }
    }
    for (int x = 0; x <= split_endpoint; x++) {
      for (int y = 0; y < segments[currstrip][x]; y++) {
        pixel++;
      }
    }
    paths[target][path_index][0] = currstrip;
    paths[target][path_index][1] = 0;
    paths[target][path_index][2] = pixel - 1;
    path_size += pixel;
    // Serial.print(" (0.."); Serial.print(pixel); Serial.println(")");
    if (!end) {
      currstrip = nextstrip;
    }
    path_index++;
  }
  // add merge strip if needed
  if (overlap) {
    for (int j = 0; j < ARRAY_SIZE(merges); j++) {
      if (merges[j][0] == currstrip) {
        nextstrip = merges[j][1];
        // determine the led start position based on the segment of the merge
        // strip
        int striplast = 0;
        int startpixel = 0;
        for (int k = 0; k < ARRAY_SIZE(segments[nextstrip]); k++) {
          if (k < merges[j][2]) {
            startpixel += segments[nextstrip][k];
          }
          striplast += segments[nextstrip][k];
        }
        paths[target][path_index][0] = nextstrip;
        paths[target][path_index][1] =
          startpixel;  // the pixel index at start of segment
        paths[target][path_index][2] =
          striplast - 1;  // the pixel index at end of segment
        path_size += striplast - startpixel;
        // Serial.print(" -m> "); Serial.print(nextstrip);
        // Serial.print(" ("); Serial.print(startpixel); Serial.print("..");
        // Serial.print(branchsize-1); Serial.print(")");
      }
    }
  }
  Serial.print("GEN PATH: ");
  Serial.print(target);
  Serial.print(" strip ");
  Serial.print(start);
  Serial.print(" size ");
  Serial.println(path_size);
  // print out path for debugging
  for (int i = 0; i < ARRAY_SIZE(paths[target]); i++) {
    Serial.print(paths[target][i][0]);
    Serial.print("(");
    Serial.print(paths[target][i][2]);
    Serial.print(")");
    Serial.print(",");
  }
  Serial.println(" Done.");
}

// game start animation
void gamestart() {
  Serial.println("Starting a new game...");
  playsound(sounds[ACTIVATE]);
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].fill(colors[WHITE]);
    strips[i].show();
  }
  delay(1000);
  fill_padlights();
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  delay(1000);
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < TOUCHPADS; i++) {
      playsound(gamenotes[i]);
      fill_branch(i, colors[i]);
      int striptoleft;
      if (i == (TOUCHPADS - 1)) {
        striptoleft = 0;
      } else {
        striptoleft = i + 1;
      }
      fill_branch(striptoleft, colors[i]);
      delay(100);
      clear_tree();
    }
  }
  memset(gamesteps, 0, sizeof gamesteps);
  gamesteps[0][0] = random(TOUCHPADS);
  gamecount = 0;
  trycount = 0;
  currmode = GAME;
  delay(1000);
  run_prompt(400);
}

// run through the prompt with current gamesteps
void run_prompt(int speed) {
  currstate = PROMPT;
  Serial.print("Prompt running for round ");
  Serial.print(gamecount);
  Serial.println(":");
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  for (int i = 0; i <= gamecount; i++) {
    Serial.print(gamesteps[i][0]);
    playsound(gamenotes[gamesteps[i][0]]);
    // fill the strip and the one to the left to cover both sides of touchpad
    fill_branch(gamesteps[i][0], colors[gamesteps[i][0]]);
    // strips[gamesteps[i][0]].fill(colors[gamesteps[i][0]]);
    // strips[gamesteps[i][0]].show();
    int striptoleft;
    if (gamesteps[i][0] == (TOUCHPADS - 1)) {
      striptoleft = 0;
    } else {
      striptoleft = gamesteps[i][0] + 1;
    }
    fill_branch(striptoleft, colors[gamesteps[i][0]]);
    // strips[striptoleft].fill(colors[gamesteps[i][0]]);
    // strips[striptoleft].show();
    // Serial.print("delay is "); Serial.println(speed);
    delay(speed);
    clear_tree();
    // strips[gamesteps[i][0]].clear();
    // strips[gamesteps[i][0]].show();
    // strips[striptoleft].clear();
    // strips[striptoleft].show();
  }
  reset_touchtimeout();
  currstate = RESPONSE;
}

// game win animation then go to next mode
void gamewin(mode nextmode) {
  Serial.println("Game is won!");
  for (int x = 0; x < 7; x++) {
    playsound(sounds[WIN], false); // repeat sound if stopped
    for (int i = 0; i < ALLSTRIPS; i++) {
      strips[i].fill(colors[GREEN]);
      strips[i].show();
    }
    delay(300);
    for (int i = 0; i < ALLSTRIPS; i++) {
      strips[i].clear();
      strips[i].show();
    }
    delay(50);
  }
  delay(1000);
  currmode = nextmode;
}

// game fail animation then go to next mode
void gamefail(mode nextmode) {
  Serial.println("Game failed.");
  playsound(sounds[LOSE]);
  for (int i = 0; i < ALLSTRIPS; i++) {
    // all red and sad tone
    strips[i].fill(colors[RED]);
    strips[i].show();
  }
  delay(4000);
  clear_tree();
  //delay(1000);
  // flash the correct branch
  for (int x = 0; x < 4; x++) {
    playsound(gamenotes[gamesteps[trycount][0]]);
    fill_branch(gamesteps[trycount][0], colors[gamesteps[trycount][0]]);
    // strips[gamesteps[trycount][0]].fill(colors[gamesteps[trycount][0]]);
    // strips[gamesteps[trycount][0]].show();
    int striptoleft;
    if (gamesteps[trycount][0] == (TOUCHPADS - 1)) {
      striptoleft = 0;
    } else {
      striptoleft = gamesteps[trycount][0] + 1;
    }
    fill_branch(striptoleft, colors[gamesteps[trycount][0]]);
    // strips[striptoleft].fill(colors[gamesteps[trycount][0]]);
    // strips[striptoleft].show();
    delay(100);
    clear_tree();
    delay(10);
  }
  delay(2000);
  currmode = nextmode;
}

void gen_multipath(int count) {
  int strip_range = BASESTRIPS;
  Serial.print("Generating paths: ");
  Serial.println(count);
  if (count > strip_range) {
    count = strip_range;
  }
  // make an array of unique starting strips, one for each thought
  int starting_strips[count];
  int strip_count = 0;
  while (strip_count < count) {
    int new_strip = random(strip_range);
    bool already_used = false;
    for (int i = 0; i <= strip_count; i++) {
      if (starting_strips[i] == new_strip) {
        already_used = true;
      }
    }
    if (!already_used) {
      starting_strips[strip_count] = new_strip;
      strip_count++;
    }
  }
  // generate a new path for each thought
  for (int i = 0; i < count; i++) {
    int currstrip = starting_strips[i];
    // Serial.print("Path: ");Serial.print(i);Serial.print(" ");
    gen_path(i, currstrip);
  }
  Serial.println("Done generating paths.");
}

// fill a path
void fill_path(int path, uint32_t color) {
  for (int segment = 0; segment < ARRAY_SIZE(paths[path]); segment++) {
    // exclude empty paths
    if (paths[path][segment][2] != 0) {
      int first = paths[path][segment][1];
      int count = paths[path][segment][2] - paths[path][segment][1] + 1;
      // Serial.print("fill: str "); Serial.print(paths[path][segment][0]);
      // Serial.print(" fir "); Serial.print(first); Serial.print(" cou ");
      // Serial.println(count);
      strips[paths[path][segment][0]].fill(color, first, count);
      strips[paths[path][segment][0]].show();
    }
  }
}

// fill a strip and all its splits and merges
void fill_branch(int base_strip, uint32_t color, bool overlap) {
  // fill main leg
  strips[base_strip].fill(color);
  strips[base_strip].show();
  if (overlap) {
    // fill any merge from main leg
    for (int j = 0; j < ARRAY_SIZE(merges); j++) {
      if (merges[j][0] == base_strip) {
        int nextstrip = merges[j][1];
        // determine the led start position based on the segment of the merge
        // branch
        int startpixel = 0;
        for (int k = 0; k < ARRAY_SIZE(segments[nextstrip]); k++) {
          if (k < merges[j][2]) {
            startpixel += segments[nextstrip][k];
          }
        }
        strips[nextstrip].fill(color, startpixel);
        strips[nextstrip].show();
      }
    }
  }
  // fill the splits
  for (int i = 0; i < ARRAY_SIZE(splits); i++) {
    if (splits[i][0] == base_strip) {
      // if there is another split for the same destination leg and it is listed
      // before this one then skip it
      bool skip = false;
      if (!overlap) {
        for (int j = 0; j < ARRAY_SIZE(splits); j++) {
          if ((splits[j][2] == splits[i][2]) && (j < i)) {
            skip = true;
            Serial.println("skip");
          }
        }
      }
      if ((!skip) || (overlap)) {
        int currstrip = splits[i][2];
        strips[currstrip].fill(color);
        strips[currstrip].show();
        if (overlap) {
          // fill any merge off current leg
          for (int j = 0; j < ARRAY_SIZE(merges); j++) {
            if (merges[j][0] == currstrip) {
              int nextstrip = merges[j][1];
              // determine the led start position based on the segment of the
              // merge strip
              int startpixel = 0;
              for (int k = 0; k < ARRAY_SIZE(segments[nextstrip]); k++) {
                if (k < merges[j][2]) {
                  startpixel += segments[nextstrip][k];
                }
              }
              strips[nextstrip].fill(color, startpixel);
              strips[nextstrip].show();
            }
          }
        }
      }
    }
  }
}

// fill padlights with appropriate colors
void fill_padlights() {
  for (int i = 0; i < TOUCHPADS; i++) {
    // padstrip.fill(colors[i], padlights[i][0], 6);
    padstrip.setPixelColor((padlights[i][0]) / 6, colors[i]);
    // Serial.print("fill padlight "); Serial.print(i);Serial.print(" ");
    // Serial.print(" ");Serial.print(padlights[i][0]); Serial.println(" 6");
  }
  padstrip.show();
}

void clear_tree() {
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
}

// think animation
void think(int thoughts) {
  //memset(paths, 0, sizeof paths);
  clear_tree();
  //gen_multipath(thoughts);
  currmode = THINK;
  thinkingdur = random(DEFMODE_MAXDUR * 1000 / 1.5, DEFMODE_MAXDUR * 1000);
  elapsedThinking = 0;
}

void strike(int path_count, uint32_t color) {
  playsound(sounds[STRIKE]);
  memset(paths, 0, sizeof paths);
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  elapsedMillis sinceStrike;  // tbd use this to make a stutter effect
  gen_multipath(path_count);
  for (int i = 0; i < path_count; i++) {
    fill_path(i, color);
  }
  delay(150);
  // fade the strikes
  while (sinceStrike < 500) {
    color = DimColor(color);
    for (int i = 0; i < path_count; i++) {
      fill_path(i, color);
    }
    delay(30);
  }
  currmode = LIGHTNING;
  elapsedLastStrike = 0;
}

// heart beats, speed is not used as audio fx are fixed duration
void beat(int speed, uint32_t color) {
  playsound(sounds[BEAT]);
  for (int i = 0; i < ALLSTRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  for (int j = 0; j < 2; j++) {
    elapsedMillis sincehalfbeat;
    for (int k = 0; k < 2; k++) {
      for (int i = 0; i < ALLSTRIPS; i++) {
        strips[i].fill(color);
        strips[i].show();
      }
      uint nextcolor = color;
      sincehalfbeat = 0;
      while (sincehalfbeat < 300) {
        nextcolor = DimColor(nextcolor);
        for (int i = 0; i < ALLSTRIPS; i++) {
          strips[i].fill(nextcolor);
          strips[i].show();
        }
        delay(20);
      }
    }
    delay(150);
  }
  currmode = HEARTBEAT;
  elapsedLastBeat = 0;
}

// thinking animation iteration - only base strips
void pulse_step_simple(int strip, int index, uint32_t color1, uint32_t color2) {

  if (index == 0 && strip == 2) {
    playsound(sounds[THINKSOUND]);
  }

  // pick a color
  uint32_t color = color1;
  if (random(2) == 1) {
    color = color2;
  }
  strips[strip].setPixelColor(index, color);
  strips[strip].show();

  // set tail length
  int tail_length = 3;
  int tail_end = 0;
  if (index > tail_length) {
    tail_end = index - tail_length;
  }

  // draw the tail as fading pixels
  uint32_t nextcolor;
  for (int i = index; i >= tail_end; i--) {
    if (i == tail_end) {
      nextcolor = colors[BLANK];
    } else {
      nextcolor = DimColor(strips[strip].getPixelColor(i));
    }
    strips[strip].setPixelColor(i - 1, nextcolor);
    strips[strip].show();
  }
}

// thinking animation iteration
void pulse_step(int path, int index, uint32_t color1, uint32_t color2) {
  if ((index == 0) && (path == 0)) {
    playsound(sounds[THINKSOUND]);
  }
  Struct position;
  uint32_t color = color1;
  if (random(2) == 1) {
    color = color2;
  }
  position = get_path_position(path, index);
  // if not found then make a new path
  if (position.strip == -1) {
    Serial.print("Path ");
    Serial.print(path);
    Serial.print(" index ");
    Serial.print(index);
    Serial.println(" not found, making a new one.");
    pulse_count[path] = 0;
    gen_path(path);
    index = 0;
    position = get_path_position(path, index);
  }
  // if (index == 0) {
  // Serial.print("I have a thought "); Serial.print(path); Serial.print(", st
  // "); Serial.println(paths[path][0][0]); test();
  //}
  strips[position.strip].setPixelColor(position.pixel, color);
  strips[position.strip].show();
  if (position.strip == 6) {
    Serial.print("path ");
    Serial.print(path);
    Serial.print(" (");
    Serial.print(index);
    Serial.print(") strip ");
    Serial.print(position.strip);
    Serial.print(" pixel ");
    Serial.print(position.pixel);
    Serial.print(" color ");
    Serial.println(color);
    delay(300);
  }
  // set tail length
  int tail_length = 3;
  int tail_end = 0;
  if (index > tail_length) {
    tail_end = index - tail_length;
  }
  uint32_t prevcolor;
  // draw the tail as fading pixels
  for (int i = index; i >= tail_end; i--) {
    position = get_path_position(path, i);
    if (i == tail_end) {
      prevcolor = colors[BLANK];
    } else {
      prevcolor = strips[position.strip].getPixelColor(position.pixel);
    }
    position = get_path_position(path, i - 1);
    strips[position.strip].setPixelColor(position.pixel, DimColor(prevcolor));
  }
  strips[position.strip].show();
  int path_size = 0;
  for (int i = 0; i < ARRAY_SIZE(paths[path]); i++) {
    // exclude empty paths
    if (paths[path][i][2] != 0) {
      path_size += paths[path][i][2] - paths[path][i][1] + 1;
    }
  }
  // check if at end of path, if so then reset to beginning
  if (pulse_count[path] < path_size - 1) {
    pulse_count[path]++;
  } else {
    if (position.strip == 6) {
      Serial.print("Path ");
      Serial.print(path);
      Serial.print(" reached end (");
      Serial.print(path_size);
      Serial.print(" pixels). ");
      Serial.println("Making a new path.");
    }
    fill_path(path, colors[BLANK]);
    pulse_count[path] = 0;
    // Pick a new strip that is not being used
    int new_strip;
    bool done = false;
    while (!done) {
      new_strip = random(BASESTRIPS);
      bool already_used = false;
      for (int i = 0; i < thought_qty; i++) {
        if (paths[i][0][0] == new_strip) {
          already_used = true;
        }
      }
      if (!already_used) {
        done = true;
      }
    }
    gen_path(path, new_strip);
    fill_path(path, colors[BLANK]);
  }
}

Struct get_path_position(int path, int index) {
  Struct position;
  position.strip = -1;
  position.pixel = 0;
  int counter = 0;
  for (int segment = 0; segment < ARRAY_SIZE(paths[path]); segment++) {
    for (int pixel = paths[path][segment][1]; pixel <= paths[path][segment][2];
         pixel++) {
      if (counter == index) {
        position.strip = paths[path][segment][0];
        position.pixel = pixel;
      }
      counter++;
    }
  }
  return position;
}

void fill_branch_part(int base_strip, int index, uint32_t color) {
  Serial.println("-----------");
  strips[base_strip].fill(color, 0, index + 1);
  strips[base_strip].show();
  Serial.print(index);
  Serial.print("  ");
  Serial.print(base_strip);
  Serial.print("  ");
  Serial.print(index);
  Serial.print("  ");
  Serial.println();
  for (int i = 0; i < ARRAY_SIZE(splits); i++) {
    if (splits[i][0] == base_strip) {
      // if there is another split for the same destination leg and it is listed
      // before this one then skip it
      bool skip = false;
      for (int j = 0; j < ARRAY_SIZE(splits); j++) {
        if ((splits[j][2] == splits[i][2]) && (j < i)) {
          skip = true;
          Serial.println("skip");
        }
      }
      if (!skip) {
        int currstrip = splits[i][2];
        int pixel = 0;
        for (int j = 0; j <= splits[i][1]; j++) {
          pixel += segments[base_strip][j];
        }
        if (index >= pixel) {
          strips[currstrip].fill(color, 0, index - pixel + 1);
          strips[currstrip].show();
          Serial.print(index);
          Serial.print("  ");
          Serial.print(currstrip);
          Serial.print("  ");
          Serial.print(index - pixel);
          Serial.print("  ");
          Serial.println();
        }
      }
    }
  }
}

// colors a pixel on all legs of a branch based on an index
void branch_step(int base_strip, int index, uint32_t color, bool overlap) {
  Serial.println("-----------");
  if (index < strips[base_strip].numPixels()) {
    strips[base_strip].setPixelColor(index, color);
    strips[base_strip].show();
    Serial.print(index);
    Serial.print("  ");
    Serial.print(base_strip);
    Serial.print("  ");
    Serial.print(index);
    Serial.print("  ");
    Serial.println();
  } else {
    if (overlap) {
      // handle any merge from main leg
      for (int j = 0; j < ARRAY_SIZE(merges); j++) {
        if (merges[j][0] == base_strip) {
          int nextstrip = merges[j][1];
          // determine the led start position based on the segment of the merge
          // branch
          int startpixel = 0;
          for (int k = 0; k < ARRAY_SIZE(segments[nextstrip]); k++) {
            if (k < merges[j][2]) {
              startpixel += segments[nextstrip][k];
            }
          }
          // calc position on the merge leg
          int delta = index - strips[base_strip].numPixels();
          if (delta >= 0) {
            strips[nextstrip].setPixelColor(startpixel + delta, color);
            strips[nextstrip].show();
            Serial.print(index);
            Serial.print("  ");
            Serial.print(nextstrip);
            Serial.print("  ");
            Serial.print(startpixel + delta);
            Serial.print("  ");
            Serial.println();
          }
        }
      }
    }
  }
  // handle the splits
  for (int i = 0; i < ARRAY_SIZE(splits); i++) {
    if (splits[i][0] == base_strip) {
      // if there is another split for the same destination leg and it is listed
      // before this one then skip it
      bool skip = false;
      if (!overlap) {
        for (int j = 0; j < ARRAY_SIZE(splits); j++) {
          if ((splits[j][2] == splits[i][2]) && (j < i)) {
            skip = true;
            Serial.println("skip");
          }
        }
      }
      if ((!skip) || (overlap)) {
        int currstrip = splits[i][2];
        int pixel = 0;
        for (int j = 0; j <= splits[i][1]; j++) {
          pixel += segments[base_strip][j];
        }
        if (index >= pixel) {
          strips[currstrip].setPixelColor(index - pixel, color);
          strips[currstrip].show();
          Serial.print(index);
          Serial.print("  ");
          Serial.print(currstrip);
          Serial.print("  ");
          Serial.print(index - pixel);
          Serial.print("  ");
          Serial.println();
        }
        if (overlap) {
          // fill any merge off current leg
          for (int j = 0; j < ARRAY_SIZE(merges); j++) {
            if (merges[j][0] == currstrip) {
              int nextstrip = merges[j][1];
              // determine the led start position based on the segment of the
              // merge strip
              int startpixel = 0;
              for (int k = 0; k < merges[j][2]; k++) {
                startpixel += segments[nextstrip][k];
              }
              // calc position on the merge leg
              int delta = index - strips[base_strip].numPixels();
              if (delta >= 0) {
                strips[nextstrip].setPixelColor(startpixel + delta, color);
                strips[nextstrip].show();
                Serial.print(index);
                Serial.print("  ");
                Serial.print(nextstrip);
                Serial.print("  ");
                Serial.print(startpixel + delta);
                Serial.print("  ");
                Serial.println();
              }
            }
          }
        }
      }
    }
  }
}

// for visualization
void getSamples() {
  if (MIC_ACTIVE) {
    for (int i = 0; i < SAMPLES; i++) {
      vReal[i] = analogRead(MIC_IN);
      vImag[i] = 0;
    }
  }

  // avoid very high values for first few samples
  int sample_offset = 2;

// FFT
#if ZEROFFT
  ZeroFFT(vReal, SAMPLES);
#else
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
#endif

  // for(int i = 0; i < BASESTRIPS; i++){
  //   Serial.print(vReal[i + sample_offset]); Serial.print(" ");
  // }
  // Serial.println();

  // Update Intensity Array
  int maxbin = SAMPLES / 2;
  for (int i = 0; i < maxbin; i += int(maxbin / BASESTRIPS)) {
    int position = i;
    if (i = 0) position += sample_offset;
    vReal[position] = constrain(
      vReal[position], 0, MAXSAMPLEVALUE);  // set max value for input data
    vReal[position] = map(vReal[position], 0, MAXSAMPLEVALUE, 0,
                          LONGESTEQ);  // map data to fit our display
    if (Intensity[i] > 0)
      Intensity[i]--;                      // Value steps down if not same or increasing
    if (vReal[position] > Intensity[i]) {  // Match displayed value to measured value
      Intensity[i] = vReal[position];
    }
  }
  // for(int i = 0; i < BASESTRIPS; i++){
  //   Serial.print(vReal[i + sample_offset]); Serial.print(" ");
  // }
  // Serial.println();
}

// for visualization
void displayUpdate() {
  // bool peak = false;
  // if (Intensity[0] >= LONGESTEQ) peak = true;
  for (int i = 0; i < BASESTRIPS; i++) {
    int value = Intensity[i];
    int color = colors[i];
    // int level = BRIGHTNESS;
    // if (value > LONGESTEQ) {
    //   value = LONGESTEQ;
    // }
    // if (peak) {
    //   level = BRIGHTNESS * 1.5;
    //   if (level > 255) level = 255;
    // }
    if (value >= strips[i].numPixels()) {
      value = strips[i].numPixels() - 1;
    }
    // if (value > 0) {
    // fill_branch_part(i, value, colors[BLANK]);
    // fill_branch_part(i, value, color);
    strips[i].clear();
    strips[i].show();
    strips[i].fill(color, 0, value);
    // strips[i].setBrightness(level);
    strips[i].show();
    // }
  }
  // if (peak) {
  //   delay(50);
  // }
}

// rainbow animation iteration
void rainbow(int hue, int wait) {
  if (hue < 5 * 65536) {
    int pixelHue = hue + 65536L;
    for (int j = 0; j < ALLSTRIPS; j++) {
      strips[j].fill(strips[j].gamma32(strips[j].ColorHSV(pixelHue)));
      strips[j].setBrightness(round(BRIGHTNESS * RAINBOWDIMMING));
      strips[j].show();
    }
    delay(wait);
  }
}

// sparkle animation iteration
void sparkle(uint32_t color1, uint32_t color2, int wait) {
  for (int j = 0; j < ALLSTRIPS; j++) {
    int pixel = random(strips[j].numPixels());
    uint32_t color = color1;
    if (random(2)) color = color2;
    strips[j].setPixelColor(pixel, color);
    strips[j].show();
    delay(wait);
    strips[j].setPixelColor(pixel, colors[BLANK]);
  }
}

// each strip is striped with two colors
void stripe(uint32_t color1, uint32_t color2, int wait) {
  for (int j = 0; j < ALLSTRIPS; j++) {
    for (int i = 0; i < strips[j].numPixels() - 1; i++) {
      if (i % 2) {
        strips[j].setPixelColor(i, color1);
      }
      else {
        strips[j].setPixelColor(i, color2);
      }
      strips[j].show();
    }
  }
}

void twinkleRandom(int SpeedDelay, boolean OnlyOne) {
  for (int strip = 0; strip < 6; strip++) {
    uint32_t color =
      strips[strip].Color(random(0, 255), random(0, 255), random(0, 255));
    strips[strip].setPixelColor(random(strips[strip].numPixels()), color);
    strips[strip].show();
    delay(SpeedDelay);
    if (OnlyOne) {
      strips[strip].clear();
      strips[strip].show();
    }
  }
  delay(SpeedDelay);
}

void meteorRain(int meteorStep, byte red, byte green, byte blue,
                byte meteorSize, byte meteorTrailDecay,
                boolean meteorRandomDecay, int SpeedDelay) {
  for (int strip = 0; strip < BASESTRIPS; strip++) {
    // for(int i = 0; i < strips[strip].numPixels() * 2; i++) {

    // fade brightness all LEDs one step
    for (int j = 0; j < strips[strip].numPixels(); j++) {
      if ((!meteorRandomDecay) || (random(10) > 5)) {
        fadeToBlack(strip, j, meteorTrailDecay);
      }
    }
    // draw meteor
    for (int j = 0; j < meteorSize; j++) {
      if ((meteorStep - j < strips[strip].numPixels()) && (meteorStep - j >= 0)) {
        strips[strip].setPixelColor(meteorStep - j,
                                    strips[strip].Color(red, green, blue));
      }
    }
    strips[strip].show();
    delay(SpeedDelay);
    //}
  }
}

void fadeToBlack(int strip, int ledNo, byte fadeValue) {
  uint32_t oldColor;
  uint8_t r, g, b;
  int value;

  oldColor = strips[strip].getPixelColor(ledNo);
  r = (oldColor & 0x00ff0000UL) >> 16;
  g = (oldColor & 0x0000ff00UL) >> 8;
  b = (oldColor & 0x000000ffUL);

  r = (r <= 10) ? 0 : (int)r - (r * fadeValue / 256);
  g = (g <= 10) ? 0 : (int)g - (g * fadeValue / 256);
  b = (b <= 10) ? 0 : (int)b - (b * fadeValue / 256);

  strips[strip].setPixelColor(ledNo, strips[strip].Color(r, g, b));
}

void fire(int Cooling, int Sparking, int SpeedDelay) {
  // while (true) {
  for (int strip = 0; strip < BASESTRIPS; strip++) {
    byte heat[flame_size];  // was static, but can't be constant
    // memset(heat, 0, sizeof heat);
    int cooldown;
    // Step 1.  Cool down every cell a little
    for (int i = 0; i < flame_size; i++) {
      cooldown = random(0, ((Cooling * 10) / flame_size) + 2);
      if (cooldown > heat[i]) {
        heat[i] = 0;
      } else {
        heat[i] = heat[i] - cooldown;
      }
    }
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = flame_size - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    // Step 3.  Randomly ignite new 'sparks' near the bottom
    if (random(255) < Sparking) {
      int y = random(7);
      heat[y] = heat[y] + random(160, 255);
      // heat[y] = random(160,255);
    }
    // Step 4.  Convert heat to LED colors
    for (int j = 0; j < flame_size; j++) {
      setPixelHeatColor(strip, j, heat[j]);
    }
    strips[strip].show();
    //}
  }
  delay(SpeedDelay);
}

void setPixelHeatColor(int strip, int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature / 255.0) * 191);

  // calculate ramp up from
  byte heatramp = t192 & 0x3F;  // 0..63
  heatramp <<= 2;               // scale up to 0..252

  // figure out which third of the spectrum we're in:
  if (t192 > 0x80) {  // hottest
    strips[strip].setPixelColor(Pixel, strips[strip].Color(255, 255, heatramp));
  } else if (t192 > 0x40) {  // middle
    strips[strip].setPixelColor(Pixel, strips[strip].Color(255, heatramp, 0));
  } else {  // coolest
    strips[strip].setPixelColor(Pixel, strips[strip].Color(heatramp, 0, 0));
  }
}

void visualize() {
  clear_tree();
  currmode = VISUALIZE;
  int maxdur = DEFMODE_MAXDUR;
  if (defmode != VISUALIZE) maxdur = ALTMODE_MAXDUR;
  visualizedur = random(maxdur * 1000 / 1.5, maxdur * 1000);
  elapsedVisualize = 0;
}

void ambient() {
  currmode = AMBIENT;
  // randomly select an ambient mode
  currambientmode = ambientmode(random(AMBIENTMODE_QTY));
  Serial.print("Current ambient mode: ");
  Serial.println(currambientmode);
  switch (currambientmode) {
    case SPARKLE:
      {
        clear_tree();
        sparkle_color1 = colors[random(6)];
        sparkle_color2 = colors[random(6)];
        if (XMAS) {
          if (random(1)) {
            sparkle_color1 =colors[WHITE];
          }
          else {
            sparkle_color1 = colors[GREEN];
          }
          sparkle_color2 = colors[RED];
        }
        playsound(sounds[SPARKLESOUND]); // stop any existing sound and play this
        break;
      }
    case STRIPE:
      {
        clear_tree();
        stripe_color1 = colors[random(6)];
        stripe_color2 = colors[random(6)];
        if (XMAS) {
          if (random(2)) {
            stripe_color1 = colors[WHITE];
          }
          else {
            stripe_color1 = colors[GREEN];
          }
          stripe_color2 = colors[RED];
        }
        playsound(sounds[TWINKLESOUND]); // stop any existing sound and play this
        break;
      }
    // case TWINKLE:
    // case FIRE:
    // case METEOR:
    default:
      {
        playsound(sounds[TWINKLESOUND]); // stop any existing sound and play this
      }
  }
  int maxdur = DEFMODE_MAXDUR;
  if (defmode != AMBIENT) maxdur = ALTMODE_MAXDUR;
  ambientdur = random(maxdur * 1000 / 1.5, maxdur * 1000);
  elapsedAmbient = 0;
}

void change_mode(mode new_mode) {
  int maxdur = DEFMODE_MAXDUR;
  if (defmode != new_mode) maxdur = ALTMODE_MAXDUR;
  // reset brightness in case previous mode was dimmed
  for (int j = 0; j < ALLSTRIPS; j++) {
    strips[j].setBrightness(BRIGHTNESS);
  }
  switch (new_mode) {
    case THINK:
      think();
      break;
    case VISUALIZE:
      visualize();
      break;
    case AMBIENT:
      ambient();
      break;
    case LIGHTNING: 
      lightningdur = random(maxdur * 1000 / 1.5, maxdur * 1000);
      elapsedLightning = 0;
      strike();
      break;
    case HEARTBEAT:
      heartbeatdur = random(maxdur * 1000 / 1.5, maxdur * 1000);
      elapsedHeartbeat = 0;
      beat();
      break;
    default:
      test();
  }
}

// play sound using serial command, if forced then will cut off any current playing sound
// if not forced then it will not play if there is a current playing sound
void playsound (sound s, bool force) {
  bool play = true;
  if (!digitalRead(SFX_ACT)) {
    if (force) {
      if (!sfx.stop()) {
        sfx.reset();
        Serial.println("Reset soundboard.");
      }
      else {
        Serial.println("Stopped prev sound.");
      }
    }
    else {
      play = false;
    }
  }
  if (play) {
    char filename[11];
    memcpy(filename, s.pname, 11);
    if (!sfx.playTrack(filename)) {
      Serial.print("Failed to play: ");
      Serial.println(s.name);
    }
    else {
      //Serial.print("Played: ");
      //Serial.println(s.name);
    }
  }
}

// play a sound using the trigger pins, will wait to finish current playing sound first
void triggersound(sound s) {
  // wait until previous playback is over
  elapsedSound = 0;
  while (!digitalRead(SFX_ACT)) {
    if (elapsedSound > 1000) {
      elapsedSound = 0;
      Serial.print(".");
    }
  }
  pinMode(s.pin, OUTPUT);
  digitalWrite(s.pin, LOW); // pull to ground to trigger sound full playback
  delay(130); // to make sure the trigger happens
  pinMode(s.pin, INPUT_PULLUP); // trigger pin shouldn't take voltage when off
  Serial.print("Playing ");
  Serial.println(s.name);
  delay(130); // to make sure action pin is set as active
}

void test() {
  clear_tree();
  Serial.println("Testing...");
  // print out the paths data
  if (currtest == PRINT_PATHS) {
    Serial.println("Print all paths:");
    for (int i = 0; i < 3; i++) {
      gen_path(i, i);
    }
    Serial.println("**********");
    for (int i = 0; i < ARRAY_SIZE(paths); i++) {
      Serial.print("Path ");
      Serial.println(i);
      for (int j = 0; j < ARRAY_SIZE(paths[i]); j++) {
        Serial.print(j);
        Serial.print(": ");
        for (int k = 0; k < ARRAY_SIZE(paths[i][j]); k++) {
          Serial.print(paths[i][j][k]);
          Serial.print(",");
        }
        Serial.println("");
      }
    }
  }
  // this test the pad lights
  if (currtest == PAD_LIGHTS) {
    Serial.println("Fill all strips.");
    for (int i = 0; i < TOUCHPADS; i++) {
      padstrip.fill(colors[i], padlights[i][0], 6);
      padstrip.setPixelColor(i, colors[i]);
    }
    padstrip.show();
  }
  // test the simple pulse animation
  if (currtest == LED_PULSE) {
    int index = 0;
    int loop_count = 0;
    while (true) {
      delay(10);
      loop_count++;
      Serial.print(loop_count);
      Serial.print(" - ");
      Serial.println(digitalRead(SFX_ACT));
      if (loop_count == 1) {
        //if (digitalRead(SFX_ACT) != LOW) {
        playsound(sounds[THINKSOUND]);
      }
      for (int i = 0; i < BASESTRIPS; i++) {
        pulse_step_simple(i, pulse_count[i]);
        // check if at end of path, if so then reset to beginning
        if (pulse_count[i] < strips[i].numPixels() - 1) {
          pulse_count[i]++;
        } else {
          strips[i].clear();
          strips[i].show();
          pulse_count[i] = 0;
        }
        clear_tree();
        fill_branch(i, colors[i]);
        int striptoleft;
        if (i == (TOUCHPADS - 1)) {
          striptoleft = 0;
        } else {
          striptoleft = i + 1;
        }
        fill_branch(striptoleft, colors[i]);
        delay(4000);
      }
    }
  }
  // test the branch step animation
  if (currtest == BRANCH_STEP) {
    int index = 0;
    while(true) {
      //branch_step(1, index, colors[BLUE]);
      delay(300);
      index++;
      if (index > 60) {
        index = 0;
      }
    }
  }
  // test the touch pads
  if (currtest == TOUCH) {
    Serial.println("Showing touch data:");
    while (true) {
      for (int i = 0; i < TOUCHPADS; i++) {
        // if *is* touched and *wasnt* touched before
        if (currtouched & _BV(i)) {
          if (!(lasttouched & _BV(i))) {
            Serial.print(i);
            Serial.println(" touched");
          }
        }
        // if is *not* touched and *was* touched before
        if (!(currtouched & _BV(i))) {
          if (lasttouched & _BV(i)) {
            Serial.print(i);
            Serial.println(" released");
          }
        }
      }
      // debugging info, what
      Serial.print("\t\t\t\t\t\t\t\t\t\t\t\t\t 0x");
      Serial.println(cap.touched(), HEX);
      Serial.print("Filt: ");
      for (uint8_t i = 0; i < 12; i++) {
        Serial.print(cap.filteredData(i));
        Serial.print("\t");
      }
      Serial.println();
      Serial.print("Base: ");
      for (uint8_t i = 0; i < 12; i++) {
        Serial.print(cap.baselineData(i));
        Serial.print("\t");
      }
      Serial.println();

      // put a delay so it isn't overwhelming
      delay(100);
    }
  }
  // test an animation
  if (currtest == ANIMATION) {
    while (true) {
      rainbow(rainbowHue);
      rainbowHue += 256;
      if (rainbowHue >= 5 * 65536) {
        rainbowHue = 0;
      }
    }
  }
  // test the vizualizer
  if (currtest == VISUALIZER) {
    Serial.println("Testing visualizer:");
    while (true) {
      // Collect Samples
      getSamples();
      // Update Display
      displayUpdate();
    }
  }
  if (currtest == SOUNDFILES) {
    for (int i = 0; i < 11; i++) {
      triggersound(sounds[i]);
    }
  }
  if (currtest == GAMEWIN) {
    for (int i = 0; i < 3; i++) {
      gamewin();
      delay(3000);
    }
  }
}