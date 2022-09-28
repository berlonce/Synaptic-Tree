
#include <elapsedMillis.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_SPIDevice.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Soundboard.h>

// tree modes
enum mode { THINK, LIGHTNING, HEARTBEAT, GAME, TEST };
enum testcase { ALL, LED, TOUCH, AUDIO};
mode defmode = HEARTBEAT;
testcase currtest = LED;
// set false if production strip
#define TESTSTRIP true

// Needed for touchpad support
#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// C arrays have no size method
#define ARRAY_SIZE(array) ((sizeof(array))/(sizeof(array[0])))

// physical limits
#define STRIPS 27
#define BASESTRIPS 5
#define TOUCHPADS 5
#define BRIGHTNESS 100
#define GAMELIMIT 10
#define THINKING_MAXDUR 90
#define LIGHTNING_MAXDUR 10
#define HEARTBEAT_MAXDUR 10

// useful colors
#define RED 0
#define GREEN 1
#define BLUE 2
#define ORANGE 3
#define PURPLE 4
#define WHITE 5
#define BLANK 6

// useful tone frequencies
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73

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

// connect to the RST pin on the Sound Board
#define SFX_RST 2

// basic tone generation, not used right now
char gamenotes[TOUCHPADS][20] = {
  "TONE0   WAV",
  "TONE1   WAV",
  "TONE2   WAV",
  "TONE3   WAV",
  "TONE4   WAV"
};

// touchpad lights order
int padlights[][6] = {
  {18, 19, 20, 21, 22, 23},
  {24, 25, 26, 27, 28, 29},
  { 0,  1,  2,  3,  4,  5},
  { 6,  7,  8,  9, 10, 11},
  {12, 13, 14, 15, 16, 17}
};

// game states
enum gamestate { PROMPT, RESPONSE };

// pin number of each led strip
int ledpins[] = {
  22, // branch 0 (on A, aligned under base of split leg on B)
  24, // branch 1 (on A, next one clockwise looking down on segment)
  26, // branch 2 (on A, next one clockwise looking down on segment)
  28, // branch 3 (on A, next one clockwise looking down on segment)
  30, // branch 4 (on A, next one clockwise looking down on segment)
  32, // branch 5
  34, // branch 6
  36, // branch 7
  38, // branch 8
  40, // branch 9
  23, // branch 10
  25, // branch 11
  27, // branch 12
  29, // branch 13
  31, // branch 14
  33, // branch 15
  35, // branch 16
  37, // branch 17
  39, // branch 18
  41, // branch 19
  42, // branch 20
  44, // branch 21
  46, // branch 22
  43, // branch 23
  45, // branch 24
  47, // branch 25
  48  // branch 26
};

// padlights pin
int padlight_pin = 52;

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
// segments are individually cut LED strips, identified by the tree segment letter 
// and the strip number starting from 0 at the base of the split leg then increasing
// clockwise (G is an exception where it has two strips at base of split so left strip is 0
int segments[][6] = {
  {12, 10, 12, 11, 11},     // branch 0  (A0, B0, D2, I1, N0)
  {12, 12, 5, 11, 5, 15},   // branch 1  (A1, B3, C6, F4, G0, Q0)
  {12, 12, 3, 11, 11, 11},  // branch 2  (A2, B4, C0, E2, J1, O1)
  {12, 12, 5, 5, 11, 11},   // branch 3  (A3, B5, C3, F0, K0, P0)
  {12, 12, 5, 10, 11, 11},  // branch 4  (A4, B6, C4, F2, G4, R2)
  {11, 11, 11, 11},         // branch 5  (B1, D3, I2, N1)
  {10, 5, 11, 5},           // branch 6  (B2, C5, F3, G6, Q0-merge)
  {10, 11, 11, 12},         // branch 7  (B7, D5, I4, N2)
  {11, 5, 11, 11},          // branch 8  (B8, D0, H0, M0)
  {4, 12, 12},              // branch 9  (C1, E3, J2, O1-merge)
  {5, 11, 11, 12},          // branch 10 (C2, E4, J3, O2)
  {5, 11, 12},              // branch 11 (C7, E5, J4, O0-merge)
  {4, 6, 15},               // branch 12 (C8, E0, L0)
  {7, 11, 12},              // branch 13 (D1, H1, M1)
  {6, 11, 12},              // branch 14 (D6, H2, M2)
  {5, 12},                  // branch 15 (D7, I0, N0-merge)
  {6, 14},                  // branch 16 (E1, L1)
  {7, 15},                  // branch 17 (E6, L2)
  {4, 11, 11},              // branch 18 (E7, J0, O0)
  {6, 11, 11},              // branch 19 (F1, K1, P1)
  {7, 11, 12},              // branch 20 (F6, K2, P2)
  {11, 12, 11},             // branch 21 (F5, G2, R1)
  {5, 12},                  // branch 22 (F7, G3, R1-merge)
  {6, 15},                  // branch 23 (G1, Q1)
  {6, 14},                  // branch 24 (G5, Q2)
  {5, 12},                  // branch 25 (G7, R0)
  {11, 11}                  // branch 26 (D4, I3, N1-merge)
};

// source branch number, the segment number before split, and which branch it splits to
// there can be multiple cases per same source branch
int splits[][3] = {
  { 1, 0,  5},   // branch A1 --> B1
  { 1, 0,  6},   // branch A1 --> B2
  { 4, 0,  7},   // branch A4 --> B7
  { 4, 0,  8},   // branch A4 --> B8
  { 3, 1,  9},   // branch A3 --> C1
  { 3, 1, 10},  // branch A3 --> C2
  { 1, 1, 11},  // branch A1 --> C7
  { 1, 1, 12},  // branch A1 --> C8
  { 0, 1, 13},  // branch A0 --> D1
  { 6, 0, 14},  // branch B2 --> D4
  { 4, 1, 14},  // branch A4 --> D6
  {13, 1, 15}, // branch D1 --> D7
  {14, 1, 15}, // branch D6 --> D7
  { 2, 2, 16},  // branch A2 --> E1
  {11, 2, 17}, // branch C7 --> E6
  {16, 2, 18}, // branch E1 --> E7
  {17, 2, 18}, // branch E6 --> E7
  { 4, 2, 19},  // branch A4 --> F1
  {10, 2, 20}, // branch C2 --> F6
  {11, 2, 20}, // branch C7 --> F6
  {10, 2, 21}, // branch C2 --> F5
  {11, 2, 21}, // branch C7 --> F5
  {19, 2, 22}, // branch F1 --> F7
  {20, 2, 22}, // branch F6 --> F7
  {21, 3, 23}, // branch F5 --> G1
  { 4, 3, 24},  // branch A4 --> G5
  {23, 3, 25}, // branch G1 --> G7
  {24, 3, 25}  // branch G5 --> G7
};

// starting branch number, branch and segment number that it merges into
int merges[][3] = {
  {26,  5, 3},  // branch 26  (D4, I3, N1-merge)
  { 9,  2, 5},   // branch 9  (C1, E2, J2, O1-merge)
  {11, 18, 2}, // branch 11 (C7, E5, J4, O0-merge)
  {15,  0, 4},  // branch 15 (D7, I0, N0-merge)
  {22, 21, 2}, // branch 22 (F7, G3, R1-merge)
  { 6,  1, 5}    // branch 6 (B2, C5, F3, G6, Q0-merge)
};

// pin for audio tones
int tonepin = 14;

// keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
elapsedMillis elapsetouched[TOUCHPADS]; // elapsed time while touched
elapsedMillis elapsenottouched[TOUCHPADS]; // elapsed time while not touched
elapsedMillis elapsedThinking;
elapsedMillis elapsedLightning;
elapsedMillis elapsedHeartbeat;
elapsedMillis elapsedLastBeat;
elapsedMillis elapsedLastStrike;
int idle_timer = 0;
int gamesteps[GAMELIMIT][5]; // when you reach this limit you win the game
int gamecount = 0;
int trycount = 0;
mode currmode = defmode;
gamestate defstate = PROMPT;
gamestate currstate = defstate;
int timeoutcount = 0;
int thought_qty = random(3,5);
// keep track of current pulse step for each path
int pulse_count[10] = {0,0,0,0,0,0,0,0,0,0};
int strikeWait = 3000;

Adafruit_NeoPixel strips[STRIPS];
Adafruit_NeoPixel padstrip;

#if TESTSTRIP
// pixel colors are RGB
  uint colors[] = {
    strips[0].Color(255,0,0),   // red
    strips[0].Color(0,255,0),   // green
    strips[0].Color(0,0,255),   // blue
    strips[0].Color(255,164,0), // orange
    strips[0].Color(255,0,255),  // purple
    strips[0].Color(255,255,255),   // white
    strips[0].Color(0,0,0)       // blank    
  };
#else
// pixel colors are BGR
  uint colors[] = {
    strips[0].Color(0,0,255),   // red
    strips[0].Color(0,255,0),   // green
    strips[0].Color(255,0,0),   // blue
    strips[0].Color(0,164,255), // orange
    strips[0].Color(255,0,255),  // purple
    strips[0].Color(255,255,255),   // white
    strips[0].Color(0,0,0)       // blank 
  };  
#endif

// in case where tones are used instead of soundboard FX
uint gametones[] = {
  NOTE_E1,
  NOTE_G1,
  NOTE_B1,
  NOTE_D1,
  NOTE_F1
};

int paths[5][5][3];
//uint16_t veins[1][400][2];
struct pathPosition {
  int strip, pixel;
};
typedef struct pathPosition Struct;
Struct get_path_position(int path, int index);
void gamestart();
void run_prompt(int speed = 1000);
void gamewin(mode nextmode = defmode);
void gamefail(mode nextmode = defmode);
void gen_path(int target, int start = random(5), int branch_max = 5);
void gen_multipath(int count);
void pulse_step(int path, int index, uint32_t color1 = colors[BLUE], uint32_t color2 = colors[PURPLE]);
void fill_path(int path, uint32_t color = colors[WHITE]);
void fill_branch(int strip, uint32_t color = colors[WHITE]);
void fill_padlights();
void clear_tree();
void think(int thoughts = thought_qty);
void strike(int paths = random(1,3), uint32_t color = colors[WHITE]);
void beat(int speed = 500, uint32_t color = colors[RED]);
void test();

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

// create soundboard object
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);

// Initialize everything and prepare to start
void setup()
{
  Serial.begin(115200);
  //while (!Serial) { // needed to keep leonardo/micro from starting too fast!
  //  delay(10);
  //}
  delay(2000);
  Serial.println("Starting setup...");

  // setup uart for soundboard
  Serial1.begin(9600);

  // setup soundboard
  if ((defmode != TEST) || ((currtest == AUDIO) || (currtest == ALL))) {
    if (!sfx.reset()) {
      Serial.println("Adafruit Sound board not found.");
      while (1);
    }
    Serial.println("SFX board found!");
    delay(1000);
  }
  else {
    Serial.println("SFX board skipped.");
  }

  // startup sound
  if (defmode == THINK) {
    char soundfile[] = "STARTUP WAV";
    if (!sfx.playTrack(soundfile)) {
      Serial.println("Failed to play track."); Serial.println(soundfile);
    }
  }

  // set seed for randomizer
  randomSeed(analogRead(A0));

  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if ((defmode != TEST) || ((currtest == TOUCH) || (currtest == ALL))) {
    if (!cap.begin(0x5A)) {
      Serial.println("MPR121 touch sensor not found.");
      while (1);
    }
    Serial.println("Touch sensor found!");
  }
  else {
    Serial.println("Touch sensor skipped.");
  }
  
  // setup all strips
  for (int i = 0; i < STRIPS; i++) {
    int branchpixels = 0;
    bool hassplits = false;
    for (int j = 0; j < ARRAY_SIZE(segments[i]); j++) {
      branchpixels += segments[i][j];
    }
    strips[i] = Adafruit_NeoPixel(branchpixels, ledpins[i], NEO_GRB + NEO_KHZ800);
    strips[i].setBrightness(BRIGHTNESS);
    strips[i].begin();
    strips[i].clear();
    strips[i].show();
    Serial.print("Branch "); Serial.print(i);
    Serial.print(": "); Serial.print(branchpixels);
    Serial.print("p (pin "); Serial.print(ledpins[i]); Serial.println(")");
  }

  padstrip = Adafruit_NeoPixel(30, padlight_pin, NEO_GRB + NEO_KHZ800);
  padstrip.begin();
  padstrip.clear();
  padstrip.show();
  Serial.print("Pad strip: ");
  Serial.print("30p (pin "); Serial.print(padlight_pin); Serial.println(")");

  // delay for startup sound
  if ((defmode == THINK) && !TESTSTRIP) {
    delay(6000);
  }

  Serial.println("Setup is done.");
  switch (defmode) {
    case THINK:
      think();
      break;
    case LIGHTNING:
      elapsedLightning = 0;
      strike();
      break;
    case HEARTBEAT:
      elapsedHeartbeat = 0;
      beat();
      break;
    default:
      test();
  }
}

// Main loop
void loop()
{
  if (defmode != TEST) {
    // Get the currently touched pads
    currtouched = cap.touched();
    //timeoutcount = 0; // reset timeout count
    for (int i = 0; i < TOUCHPADS; i++) {
      // it if *is* touched and *wasnt* touched before
      if (currtouched & _BV(i)) {
        elapsenottouched[i] = 0;
        if (!(lasttouched & _BV(i))) {
          elapsetouched[i] = 0;
          Serial.print(i); Serial.println(" touched");
          if (currmode == GAME) {
            if (! sfx.playTrack(gamenotes[i]) ) {
              Serial.print("Failed to play track."); Serial.println(gamenotes[i]);
            }
            // light up the branch for the current try and the one to the left
            strips[i].fill(colors[i]);
            strips[i].show();
            int striptoleft;
            if (i == (TOUCHPADS - 1)) {
              striptoleft = 0;
            }
            else {
              striptoleft = i + 1;
            }
            strips[striptoleft].fill(colors[i]);
            strips[striptoleft].show();
            delay(600);
            // clear the active LEDs
            strips[i].clear();
            strips[i].show();
            strips[striptoleft].clear();
            strips[striptoleft].show();
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
          Serial.print(i); Serial.println(" released");
          if ((currmode == GAME) && (currstate == RESPONSE)) {          
            if (gamesteps[trycount][0] == i) {
              Serial.println("correct pad touched!");
              // turn off the branch for the current try
              
              if (trycount < gamecount) {
                trycount++; // increment try count if successful try
              }
              else {
                if (gamecount == ARRAY_SIZE(gamesteps) - 1) {
                  gamewin();
                }
                else {
                  // add another path, run prompt then try again
                  Serial.println("next prompt...");
                  gamecount++;
                  gamesteps[gamecount][0] = random(TOUCHPADS);
                  int speed = 400 / gamecount + 1;
                  run_prompt(speed);
                  trycount = 0;
                }
              }
            }
            else {
              Serial.println("wrong pad touched, sorry");
              gamefail();
            }
          }
          else {
            Serial.print("elapsetouched "); Serial.println(elapsetouched[i]);
            // go to game mode if any pad touched for 2 sec
            if (elapsetouched[i] > 2000) {
              gamestart();
            }
          }
          elapsenottouched[i] = 0;
        }
      }
    }

    // turn of pad lights if not in game
    if (currmode != GAME) {
      padstrip.fill(colors[WHITE]);
      padstrip.show();
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
      }
      else {
        if (elapsenottouched[0] % 3 == 0) {
          Serial.print(".");
        }
      }
    }
    
    // reset touch state
    lasttouched = currtouched;

    // what to do when thinking
    if (currmode == THINK) {
      if (elapsedThinking < random(THINKING_MAXDUR*1000/1.5, THINKING_MAXDUR*1000)) {
        // show next pulse step for each thought path
        for (int i=0; i < thought_qty; i++){
          pulse_step(i, pulse_count[i]);
        }
      }
      else{
        if (random(2) == 1) {
          elapsedLightning = 0;
          strike();
        }
        else {
          elapsedHeartbeat = 0;
          beat();
        }
      }
    }
    else {
      memset(pulse_count, 0, sizeof pulse_count);
    }

    // what to do when lightning
    if (currmode == LIGHTNING) {
      if (elapsedLightning < random(HEARTBEAT_MAXDUR*1000/1.5, HEARTBEAT_MAXDUR*1000)) {
        //Serial.print("Mode is lightning: "); Serial.println(elapsedLastStrike);        
        if (elapsedLastStrike > random(2000, 5000)) {
          //Serial.print("Strike wait is "); Serial.println(strikeWait);
          strike();
        }
      }
      else {
          think();
      }
    }

    // what to do when heart beating
    if (currmode == HEARTBEAT) {
      if (elapsedHeartbeat < random(HEARTBEAT_MAXDUR*1000/1.5, HEARTBEAT_MAXDUR*1000)) {
        //Serial.print("Mode is heartbeat: "); Serial.println(elapsedHeartbeat);
        if (elapsedLastBeat > random(2000, 5000)) {
          beat();
        }
      }
      else {
          think();
      }
    }
    
    // this slows everything down
    delay(30);
  
    // comment out this line for detailed data from the touch sensor!
    return;
    
    // debugging info, what
    Serial.print("\t\t\t\t\t\t\t\t\t\t\t\t\t 0x"); Serial.println(cap.touched(), HEX);
    Serial.print("Filt: ");
    for (uint8_t i=0; i<12; i++) {
      Serial.print(cap.filteredData(i)); Serial.print("\t");
    }
    Serial.println();
    Serial.print("Base: ");
    for (uint8_t i=0; i<12; i++) {
      Serial.print(cap.baselineData(i)); Serial.print("\t");
    }
    Serial.println();
    
    // put a delay so it isn't overwhelming
    delay(100);
  }
}

// Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
        return (color >> 16) & 0xFF;
    }

    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
        return (color >> 8) & 0xFF;
    }

    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
        return color & 0xFF;
    }

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
        WheelPos = 255 - WheelPos;
        if(WheelPos < 85)
        {
            return strips[0].Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        else if(WheelPos < 170)
        {
            WheelPos -= 85;
            return strips[0].Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        else
        {
            WheelPos -= 170;
            return strips[0].Color(WheelPos * 3, 255 - WheelPos * 3, 0);
        }
    }

    // Calculate 50% dimmed version of a color (used by ScannerUpdate)
    uint32_t DimColor(uint32_t color)
    {
        // Shift R, G and B components one bit to the right
        uint32_t dimColor = strips[0].Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
        return dimColor;
    }

void reset_touchtimeout() {
  for (int i=0; i < TOUCHPADS; i++) {
    elapsenottouched[i] = 0;
    elapsetouched[i] = 0;
  }
}

void print_array(int *arr) {
  Serial.print("Array of "); Serial.print(ARRAY_SIZE(arr)); Serial.print(": {"); 
  for (int i=0; i<ARRAY_SIZE(arr); i++) {
    Serial.print(arr[i]); Serial.print(",");
  }
  Serial.println("}");
}

// generates a new path an assigns to the target, assumes start is not a duplicate
void gen_path(int target, int start, int branch_max) {
  int currstrip = start;
  int nextstrip;
  int split_options [5][2]; // assume not more than 5 split options for a given strip
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
    //Serial.print("For strip "); Serial.print(currstrip); Serial.println(":");
    for (int j=0; j < ARRAY_SIZE(splits); j++) {
      if (splits[j][0] == currstrip) {
        //Serial.print("Split found at "); Serial.println(splits[j][2]);
        split_options[split_count][0] = splits[j][2]; // split branch
        split_options[split_count][1] = splits[j][1]; // last segment before split
        split_count++;
      }
    }
    // add one of the split strips to the path, or not if rando_split = split_count
    int rando_split = random(split_count + 1);
    int pixel = 0;
    //if (path_index > 0) {
    //  Serial.print(" -> ");
    //}
    //Serial.print(currstrip);
    // in the case where there is no avail split or just don't want to split
    if ((split_count == 0) || (split_count == rando_split)) {
      // add current strip up to the end and drop out of loop
      split_endpoint = ARRAY_SIZE(segments[currstrip]) - 1;
      end = true;
      //if (split_count == 0) Serial.println("No splits.");
      //else Serial.println("Ignoring splits.");
    }
    // in the case where there are splits and want to use one
    else {
      // use the split, add current strip up to skip
      split_endpoint = split_options[rando_split][1];
      nextstrip = split_options[rando_split][0];
      //Serial.println("Using a split.");
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
    //Serial.print(" (0.."); Serial.print(pixel); Serial.println(")");
    if (!end) {
      currstrip = nextstrip;
    }
    path_index++;
  }
  // add merge strip if needed
  for (int j = 0; j < ARRAY_SIZE(merges); j++) {
    if (merges[j][0] == currstrip) {
      nextstrip = merges[j][1];
      // determine the led start position based on the segment of the merge strip
      int striplast = 0;
      int startpixel = 0;
      for (int k = 0; k < ARRAY_SIZE(segments[nextstrip]); k++) {
          if (k < merges[j][2]) {
            startpixel += segments[nextstrip][k];
          }
          striplast += segments[nextstrip][k];
      }
      paths[target][path_index][0] = nextstrip;
      paths[target][path_index][1] = startpixel; // the pixel index at start of segment
      paths[target][path_index][2] = striplast - 1; // the pixel index at end of segment
      path_size += striplast - startpixel;
      //Serial.print(" -m> "); Serial.print(nextstrip);
      //Serial.print(" ("); Serial.print(startpixel); Serial.print(".."); Serial.print(branchsize-1); Serial.print(")");
    }
  }
  //Serial.print("GEN PATH: "); Serial.print(target); 
  //Serial.print(" strip "); Serial.print(start);
  //Serial.print(" size "); Serial.println(path_size);
  // print out path for debugging
  //for (int i = 0; i < ARRAY_SIZE(paths[target]); i++) {
    //Serial.print(paths[target][i][0]);
    //Serial.print("("); Serial.print(paths[target][i][2]); Serial.print(")");
    //Serial.print(",");
  //}
  //Serial.println(" Done.");
}

// game start animation
void gamestart() {
  Serial.println("Starting a new game...");
  if (! sfx.stop()) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "ACTIVATEWAV";
  if (! sfx.playTrack(soundfile) ) {
    Serial.print("Failed to play track."); Serial.println(soundfile);
  }
  for (int i = 0; i < STRIPS; i++) {
    strips[i].fill(colors[WHITE]);
    strips[i].show();
  }
  delay(1000);
  if (! sfx.stop()) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  fill_padlights();
  for (int i = 0; i < STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  delay(1000);
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < TOUCHPADS; i++) {
      if (! sfx.playTrack(gamenotes[i]) ) {
        Serial.print("Failed to play track."); Serial.println(gamenotes[i]);
      }
      strips[i].fill(colors[i]);
      strips[i].show();
      delay(100);
      if (! sfx.stop()) {
        //Serial.println("Failed to stop");
        sfx.reset();
      }
      strips[i].clear();
      strips[i].show();
    }
  }
  delay(1000);
  currmode = GAME;
  memset(gamesteps, 0, sizeof gamesteps);
  gamecount = 0;
  trycount = 0;
  gamesteps[gamecount][0] = random(TOUCHPADS);
  run_prompt(400);
}

// run through the prompt with current gamesteps
void run_prompt(int speed) {
  currstate = PROMPT;
  Serial.print("Prompt running for round "); Serial.print(gamecount); Serial.println(":");
  for (int i = 0; i < STRIPS; i++) {
      strips[i].clear();
      strips[i].show();
  }
  for (int i = 0; i <= gamecount; i++) {
    Serial.print(gamesteps[i][0]);
    if (!sfx.playTrack(gamenotes[gamesteps[i][0]])) {
      Serial.print("Failed to play track: "); Serial.println(gamenotes[gamesteps[i][0]]);
    }
    // fill the strip and the one to the left to cover both sides of touchpad
    strips[gamesteps[i][0]].fill(colors[gamesteps[i][0]]);
    strips[gamesteps[i][0]].show();
    int striptoleft;
    if (gamesteps[i][0] == (TOUCHPADS - 1)) {
      striptoleft = 0;
    }
    else {
      striptoleft = gamesteps[i][0] + 1;
    }
    strips[striptoleft].fill(colors[gamesteps[i][0]]);
    strips[striptoleft].show();
    //Serial.print("delay is "); Serial.println(speed);
    delay(speed);
    if (!sfx.stop()) {
      //Serial.println("Failed to stop");
      sfx.reset();
    }
    strips[gamesteps[i][0]].clear();
    strips[gamesteps[i][0]].show();
    strips[striptoleft].clear();
    strips[striptoleft].show();
  }
  reset_touchtimeout();
  currstate = RESPONSE;
}

// game win animation then go to next mode
void gamewin(mode nextmode) {  
  Serial.println("Game is won!");
  char soundfile[] = "WIN     WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int x = 0; x < 7 ; x++) {
    for (int i = 0; i < STRIPS; i++) {
      strips[i].fill(colors[GREEN]);
      strips[i].show();
    }
    delay(300);
    for (int i = 0; i < STRIPS; i++) {
      strips[i].clear();
      strips[i].show();
    }
    delay(50);
  }
  delay(2000);
  if (! sfx.stop()) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  currmode = nextmode;
}

// game fail animation then go to next mode
void gamefail(mode nextmode) {  
  Serial.println("Game failed.");
  char soundfile[20] = "LOSE    WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int i = 0; i < STRIPS; i++) {
    // all red and sad tone
    strips[i].fill(colors[RED]);
    strips[i].show();
  }
  delay(3000);
  if (! sfx.stop() ) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  for (int i=0; i<STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  delay(1000);
  // flash the correct branch
  for (int x = 0; x < 4 ; x++) {
    if (!sfx.playTrack(gamenotes[gamesteps[trycount][0]])) {
      Serial.print("Failed to play track: "); Serial.println(gamenotes[gamesteps[trycount][0]]);
    }
    strips[gamesteps[trycount][0]].fill(colors[gamesteps[trycount][0]]);
    strips[gamesteps[trycount][0]].show();
    int striptoleft;
    if (gamesteps[trycount][0] == (TOUCHPADS - 1)) {
      striptoleft = 0;
    }
    else {
      striptoleft = gamesteps[trycount][0] + 1;
    }
    strips[striptoleft].fill(colors[gamesteps[trycount][0]]);
    strips[striptoleft].show();
    delay(100);
    if (!sfx.stop()) {
      //Serial.println("Failed to stop");
      sfx.reset();
    }
    strips[gamesteps[trycount][0]].clear();
    strips[gamesteps[trycount][0]].show();
    strips[striptoleft].clear();
    strips[striptoleft].show();
    delay(10);
  }
  delay(2000);
  currmode = nextmode;
}

void gen_multipath(int count) {
  int strip_range = BASESTRIPS;
  Serial.println("Starting to think.");
  if (count > strip_range) {
    count = strip_range;
  }
  // make an array of unique starting strips, one for each thought
  int starting_strips[count];
  int strip_count = 0;
  while (strip_count < count) {
    int new_strip = random(strip_range);
    bool already_used = false;
    for (int i=0; i <= strip_count; i++) {
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
    //Serial.print("Path: ");Serial.print(i);Serial.print(" ");
    gen_path(i, currstrip);
  }
}

// fill a path
void fill_path(int path, uint32_t color) {
  for (int segment = 0; segment < ARRAY_SIZE(paths[path]); segment++) {
        // exclude empty paths
    if (paths[path][segment][2] != 0) {
      int first = paths[path][segment][1];
      int count = paths[path][segment][2] - paths[path][segment][1] + 1;  
      //Serial.print("fill: str "); Serial.print(paths[path][segment][0]);
      //Serial.print(" fir "); Serial.print(first); Serial.print(" cou "); Serial.println(count);
      strips[paths[path][segment][0]].fill(color, first, count);
      strips[paths[path][segment][0]].show();
    }
  }
}

// fill a strip and all its splits and merges
void fill_branch(int strip, uint32_t color) {
  strips[strip].fill(color);
  strips[strip].show();
  for (int j = 0; j < ARRAY_SIZE(merges); j++) {
    if (merges[j][0] == strip) {
      int nextstrip = merges[j][1];
      // determine the led start position based on the segment of the merge branch
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
  // fill the splits
  for (int i = 0; i < ARRAY_SIZE(splits); i++) {
    if (splits[i][0] == strip) {
      int currstrip = splits[i][2];
      strips[currstrip].fill(color);
      strips[currstrip].show();
      for (int j = 0; j < ARRAY_SIZE(merges); j++) {
        if (merges[j][0] == currstrip) {
          int nextstrip = merges[j][1];
          // determine the led start position based on the segment of the merge strip
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

// fill padlights with appropriate colors
void fill_padlights() {
  for (int i = 0; i < TOUCHPADS; i++) {
    //padstrip.fill(colors[i], padlights[i][0], 6);
    padstrip.setPixelColor((padlights[i][0])/6, colors[i]);
    //Serial.print("fill padlight "); Serial.print(i);Serial.print(" ");
    //Serial.print(" ");Serial.print(padlights[i][0]); Serial.println(" 6");
  }
  padstrip.show();
}

void clear_tree() {
  for (int i = 0; i < STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
}

// think animation
void think(int thoughts) {
  memset(paths, 0, sizeof paths);
  for (int i = 0; i < STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  gen_multipath(thoughts);
  currmode = THINK;
  elapsedThinking = 0;
}

void strike(int path_count, uint32_t color) {
  if (! sfx.stop()) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "STRIKE  WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  memset(paths, 0, sizeof paths);
  for (int i = 0; i < STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  elapsedMillis sinceStrike; // tbd use this to make a stutter effect
  gen_multipath(path_count);
  for (int i = 0; i < path_count; i++) {
    Serial.println("Strike!");
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
  //for (int i = 0; i < STRIPS; i++) {
  //  strips[i].clear();
  //  strips[i].show();
  //}
  currmode = LIGHTNING;
  elapsedLastStrike = 0;
}

// heart beats, speed is not used as audio fx are fixed duration
void beat(int speed, uint32_t color) {
  if (! sfx.stop()) {
    //Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "BEAT    WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int i = 0; i < STRIPS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  for (int j = 0; j < 2; j++) {
    elapsedMillis sincehalfbeat;
    for (int k = 0; k < 2; k++) {
      Serial.println("Beating!");
      for (int i = 0; i < STRIPS; i++) {
          strips[i].fill(color);
          strips[i].show();
      }
      uint nextcolor = color;
      sincehalfbeat = 0;
      while (sincehalfbeat < 300) {
        nextcolor = DimColor(nextcolor);
        for (int i = 0; i < STRIPS; i++) {
          strips[i].fill(nextcolor);
          strips[i].show();
        }
        delay(20);
      }
      //delay(10);
    }
    delay(150);
  }
  currmode = HEARTBEAT;
  elapsedLastBeat = 0;
}

// the thinking animation
void pulse_step(int path, int index, uint32_t color1, uint32_t color2) {
  if ((index == 0) && (path == 0)) {
    if (! sfx.stop() ) {
      //Serial.println("Failed to stop");
      sfx.reset();
    }
    char soundfile[] = "THINK   WAV";
    if (!sfx.playTrack(soundfile)) {
      Serial.print("Failed to play track: "); Serial.println(soundfile);
    }
  }
  Struct position;
  uint32_t color = color1;
  if (random(2) == 1) {
    color = color2;
  }
  position = get_path_position(path, index);
  // if not found then make a new path
  if (position.strip == -1) {
    Serial.print("Path "); Serial.print(path);
    Serial.print(" index "); Serial.print(index);
    Serial.println(" not found, making a new one.");
    pulse_count[path] = 0;
    gen_path(path);
    index = 0;
    position = get_path_position(path, index);
  }
  if (index == 0) {
    //Serial.print("I have a thought "); Serial.print(path); Serial.print(", st "); Serial.println(paths[path][0][0]);
    //test();
  }
  strips[position.strip].setPixelColor(position.pixel, color);
  strips[position.strip].show();
  //Serial.print("path "); Serial.print(path); 
  //Serial.print(" ("); Serial.print(index);
  //Serial.print(") strip "); Serial.print(position.strip); Serial.print(" pixel "); Serial.println(position.pixel);
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
    }
    else {
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
  }
  else {
    //Serial.print("Path "); Serial.print(path);
    //Serial.print(" reached end ("); Serial.print(path_size); Serial.print(") ");
    //Serial.println("Making a new path.");
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
  }
}

Struct get_path_position(int path, int index) {
  Struct position;
  position.strip = -1;
  position.pixel = 0;
  int counter = 0;
  for (int segment = 0; segment < ARRAY_SIZE(paths[path]); segment++) {
    for (int pixel = paths[path][segment][1]; pixel <= paths[path][segment][2]; pixel++) {
      if (counter == index) {
        position.strip = paths[path][segment][0];
        position.pixel = pixel;
      }
      counter++;
    }
  }
  return position;
}

// for gradual branch fill animation
void branch_step() {
  // tbd
}

void test() {
  Serial.println("Testing...");
  if (0) {
    Serial.println("Print all paths:");
    for (int i = 0; i < 3; i++) {
      gen_path(i, i);
    }
    Serial.println("**********");
    for (int i = 0; i < ARRAY_SIZE(paths); i++) {
      Serial.print("Path "); Serial.println(i);
      for (int j = 0; j < ARRAY_SIZE(paths[i]); j++) {
        Serial.print(j); Serial.print(": ");
        for (int k = 0; k < ARRAY_SIZE(paths[i][j]); k++) {
          Serial.print(paths[i][j][k]); Serial.print(",");
        }
        Serial.println("");
      }
    }
  }
  if (1) {
    Serial.println("Fill all strips.");
    uint color = colors[BLUE];
    //for (int i=0; i < STRIPS; i++) {
    //    strips[i].clear();
    //    strips[i].show();
    //}
    for (int i=0; i < STRIPS; i++) {
        strips[i].fill(color);
        strips[i].show();
    }
    for (int i = 0; i < TOUCHPADS; i++) {
      //padstrip.fill(colors[i], padlights[i][0], 6);
      padstrip.setPixelColor(i, colors[i]);
    }
    padstrip.show();
  }
}
