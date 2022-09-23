
#include <elapsedMillis.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_SPIDevice.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Soundboard.h>

// Tree Modes
enum mode { THINK, LIGHTNING, HEARTBEAT, GAME, TEST };
mode defmode = HEARTBEAT;

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

#define ARRAY_SIZE(array) ((sizeof(array))/(sizeof(array[0])))

// physical limits
#define BRANCHES 27
#define BASEBRANCHES 5
#define TOUCHPADS 5
#define BRIGHTNESS 50

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

// Choose any two pins that can be used with SoftwareSerial to RX & TX
#define SFX_TX 1
#define SFX_RX 0

// Connect to the RST pin on the Sound Board
#define SFX_RST 2

char gamenotes[TOUCHPADS][20] = {
  "TONE0   WAV",
  "TONE1   WAV",
  "TONE2   WAV",
  "TONE3   WAV",
  "TONE4   WAV"
};

// Game states
enum gamestate { PROMPT, RESPONSE };

// ###########################
// START OF SYNAPTIC TREE CODE
// ###########################

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

// segment sizes
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
// clockwise (G is an exception where it has not
// also, * means that it needs to be added to the tree segment

// **** update to add back in A0 thru A4 in the first five items

int segments[][6] = {
  {60, 72, 66, 66},     // branch 0  (A0, B0, D2, I1, N0)   add back A0
  {72, 30, 66, 30, 90}, // branch 1  (A1, B3, C6, F4, G0*, Q0)    add back A1
  {72, 18, 66, 66, 66}, // branch 2  (A2, B4, C0, E2, J1, O1)    add back A2
  {72, 30, 30, 66, 66}, // branch 3  (A3, B5, C3, F0, K0, P0)    add back A3
  {72, 30, 60, 66, 66}, // branch 4  (A4, B6, C4, F2, G4, R2)    add back A4
  {66, 66, 66, 66},         // branch 5  (B1, D3, I2, N1)
  {60, 30, 66, 30},         // branch 6  (B2*, C5, F3, G6*, Q0-merge)
  {60, 66, 66, 72},         // branch 7  (B7*, D5, I4, N2)
  {66, 30, 66, 66},         // branch 8  (B8, D0, H0, M0)
  {24, 72, 72},             // branch 9  (C1, E3, J2*, O1-merge)
  {30, 66, 66, 72},         // branch 10 (C2*, E4, J3, O2)
  {30, 66, 72},             // branch 11 (C7*, E5, J4*, O0-merge)
  {24, 36, 90},             // branch 12 (C8, E0, L0)
  {42, 66, 72},             // branch 13 (D1, H1, M1)
  {36, 66, 72},             // branch 14 (D6, H2, M2)
  {30, 72},                 // branch 15 (D7, I0*, N0-merge)
  {36, 84},                 // branch 16 (E1, L1)
  {42, 90},                 // branch 17 (E6, L2)
  {24, 66, 66},             // branch 18 (E7, J0, O0)
  {36, 66, 66},             // branch 19 (F1, K1, P1)
  {42, 66, 72},             // branch 20 (F6, K2, P2)
  {66, 72, 66},             // branch 21 (F5, G2, R1)
  {30, 72},                 // branch 22 (F7, G3*, R1-merge)
  {36, 90},                 // branch 23 (G1, Q1)
  {36, 84},                 // branch 24 (G5, Q2)
  {30, 72},                 // branch 25 (G7, R0)
  {66, 66}                  // branch 26 (D4, I3*, N1-merge)
};

// source branch number, the segment number before split, and which branch it splits to
// there can be multiple cases per same source branch

// uncomment items to add back in and bump up segment number before split on A level items

int splits[][3] = {
 // {1, 0, 5},   // branch A1 --> B1
 // {1, 0, 6},   // branch A1 --> B2*
 // {4, 0, 7},   // branch A4 --> B7*
 // {4, 0, 8},   // branch A4 --> B8
  {3, 0, 9},   // branch A3 --> C1
  {3, 0, 10},  // branch A3 --> C2*
  {1, 0, 11},  // branch A1 --> C7*
  {1, 0, 12},  // branch A1 --> C8
  {0, 0, 13},  // branch A0 --> D1
  {6, 0, 14},  // branch B2* --> D4
  {4, 0, 14},  // branch A4 --> D6
  {13, 1, 15}, // branch D1 --> D7
  {14, 1, 15}, // branch D6 --> D7
  {2, 1, 16},  // branch A2 --> E1
  {11, 2, 17}, // branch C7* --> E6
  {16, 2, 18}, // branch E1 --> E7
  {17, 2, 18}, // branch E6 --> E7
  {4, 1, 19},  // branch A4 --> F1
  {10, 2, 20}, // branch C2* --> F6
  {11, 2, 20}, // branch C7* --> F6
  {10, 2, 21}, // branch C2* --> F5
  {11, 2, 21}, // branch C7* --> F5
  {19, 2, 22}, // branch F1 --> F7
  {20, 2, 22}, // branch F6 --> F7
  {21, 3, 23}, // branch F5 --> G1
  {4, 2, 24},  // branch A4 --> G5
  {23, 3, 25}, // branch G1 --> G7
  {24, 3, 25}  // branch G5 --> G7
};

// starting branch number, branch and segment number that it merges into

// bump back up segment number that merges to on items that merge to A level branches

int merges[][3] = {
  {26, 5, 3},  // branch 26  (D4, I3*, N1-merge)
  {9, 2, 4},   // branch 9  (C1, E2, J2*, O1-merge)
  {11, 18, 2}, // branch 11 (C7*, E5, J4*, O0-merge)
  {15, 0, 3},  // branch 15 (D7, I0*, N0-merge)
  {22, 21, 2}, // branch 22 (F7, G3*, R1-merge)
  {6, 1, 4}    // branch 6 (B2*, C5, F3, G6*, Q0-merge)
};

// pin for audio tones
int tonepin = 54;

// Keeps track of the last pins touched
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
int gamesteps[20][5]; // when you reach this limit you win the game
int gamecount = 0;
int trycount = 0;
mode currmode = defmode;
gamestate defstate = PROMPT;
gamestate currstate = defstate;
int timeoutcount = 0;
int thought_qty = random(2,5);
// keep track of current pulse step for each path
int pulse_count[10] = {0,0,0,0,0,0,0,0,0,0};
int strikeWait = 3000;

Adafruit_NeoPixel branches[BRANCHES];

// pixel colors are BGR
#if (defmode == TEST) 
  uint colors[] = {
    branches[0].Color(255,0,0),   // red
    branches[0].Color(0,255,0),   // green
    branches[0].Color(0,0,255),   // blue
    branches[0].Color(255,164,0), // orange
    branches[0].Color(255,0,255),  // purple
    branches[0].Color(255,255,255),   // white
    branches[0].Color(0,0,0)       // blank    
  };
#else 
  uint colors[] = {
    branches[0].Color(0,0,255),   // red
    branches[0].Color(0,255,0),   // green
    branches[0].Color(255,0,0),   // blue
    branches[0].Color(0,164,255), // orange
    branches[0].Color(255,0,255),  // purple
    branches[0].Color(255,255,255),   // white
    branches[0].Color(0,0,0)       // blank 
  };  
#endif

int paths[5][5][3];
uint16_t veins[1][400][2];
struct pathPosition {
  int branch, pixel;
};
typedef struct pathPosition Struct;
void run_prompt(int speed = 1000);
void gamewin(mode nextmode = defmode);
void gamefail(mode nextmode = defmode);
void gen_path(int target, int start = random(5), int branch_max = 5);
Struct get_path_position(int path, int index);
void pulse_step(int path, int index, uint32_t color1 = colors[BLUE], uint32_t color2 = colors[PURPLE]);
void fill_path(int path, uint32_t color);
void think(int thoughts = 3);
void strike(int paths = 1, uint32_t color = colors[WHITE]);
void beat(int speed = 100, uint32_t color = colors[RED]);
void test();

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();
 
uint gametones[] = {
  NOTE_E1,
  NOTE_G1,
  NOTE_B1,
  NOTE_D1,
  NOTE_F1
};

Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);

// Initialize everything and prepare to start
void setup()
{
  // setup uart for soundboard
  Serial1.begin(9600);

  // setup soundboard
  if (!sfx.reset()) {
    Serial.println("Adafruit Sound board not found.");
    while (1);
  }
  Serial.println("SFX board found!");
  delay(1000);

  // startup sound
  char soundfile[] = "STARTUP WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.println("Failed to play track."); Serial.println(soundfile);
  }

  randomSeed(analogRead(A0));

  Serial.begin(115200);
  while (!Serial) { // needed to keep leonardo/micro from starting too fast!
    delay(10);
  }
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 touch sensor not found.");
    while (1);
  }
  Serial.println("Touch sensor found!");
  
  // setup all branches
  for (int i=0; i < BRANCHES; i++) {
    int branchpixels = 0;
    bool hassplits = false;
    for (int j=0; j < sizeof(segments[i])/sizeof(segments[i][0]); j++) {
      branchpixels += segments[i][j];
    }
    branches[i] = Adafruit_NeoPixel(branchpixels, ledpins[i], NEO_GRB + NEO_KHZ800);
    branches[i].setBrightness(BRIGHTNESS);
    branches[i].begin();
    branches[i].clear();
    branches[i].show();
    Serial.print("Branch "); Serial.print(i);
    Serial.print(": "); Serial.print(branchpixels);
    Serial.print("p (pin "); Serial.print(ledpins[i]); Serial.println(")");
  }
  delay(4000);
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  Serial.println("Setup is done.");
  switch (defmode) {
    case THINK:
      think(thought_qty);
      break;
    case LIGHTNING:
      strike();
      break;
    case HEARTBEAT:
      beat();
      break;
    default:
      test();
  }
}

// Main loop
void loop()
{
    // Get the currently touched pads
    currtouched = cap.touched();
    //timeoutcount = 0; // reset timeout count
    for (uint8_t i = 0; i < TOUCHPADS; i++) {
      // it if *is* touched and *wasnt* touched before
      if (currtouched & _BV(i)) {
        if (!(lasttouched & _BV(i))) {
          elapsetouched[i] = 0;
          Serial.print(i); Serial.println(" touched");
          if (currmode == GAME) {
            if (! sfx.playTrack(gamenotes[i]) ) {
              Serial.print("Failed to play track."); Serial.println(gamenotes[i]);
            }
            // light up the branch for the current try
            branches[i].fill(colors[i]);
            branches[i].show();
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
            if (! sfx.stop() ) {
              Serial.println("Failed to stop");
              sfx.reset();
            }
            if (gamesteps[trycount][0] == i) {
              Serial.println("correct pad touched!");
              // turn off the branch for the current try
              branches[i].clear();
              branches[i].show();
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
                  int speed = 1000 / gamecount + 500;
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
            // go to game mode if any pad touched for 3 sec
            if (elapsetouched[i] > 2000) {
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
        if (elapsenottouched[i] > 3000) {
          timeoutcount++;
        }
      }
      if (timeoutcount >= TOUCHPADS) {
        Serial.println("All touchpads timed out.");
        gamefail();    
      }  
      else {
        if (elapsenottouched[0]%300 == 0) {
          Serial.print(".");
        }
      }
    }
  
    // reset touch state
    lasttouched = currtouched;

    if (currmode == THINK) {
      if (elapsedThinking > 10000) {
      //if (0) {
        if (random(2) == 1) {
        //if (0) {
          elapsedLightning = 0;
          currmode = LIGHTNING;
          strike();
        }
        else {
          elapsedHeartbeat = 0;
          currmode = HEARTBEAT;
          beat();
        }
      }
      else{
        // show next pulse step for each thought path
        for (int i=0; i < thought_qty; i++){
          pulse_step(i, pulse_count[i]);
        }
      }
    }
    else {
      memset(pulse_count, 0, sizeof pulse_count);
    }

    if (currmode == LIGHTNING) {
      if (elapsedLightning < 5000) {
        Serial.println("mode is lightning"); Serial.print(elapsedLastStrike);        
        if (elapsedLastStrike > strikeWait) {
          Serial.print("Strike wait is "); Serial.println(strikeWait);
          strike();
          strikeWait = random(1000,5000);
        }
      }
      else {
          think(random(5));
      }
    }

    if (currmode == HEARTBEAT) {
      if (elapsedHeartbeat < 5000) {
        Serial.print("mode is heartbeat: "); Serial.println(elapsedHeartbeat);
        if (elapsedLastBeat > 4000) {
          beat();
        }
      }
      else {
          think(random(5));
      }
    }
  
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
            return branches[0].Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        else if(WheelPos < 170)
        {
            WheelPos -= 85;
            return branches[0].Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        else
        {
            WheelPos -= 170;
            return branches[0].Color(WheelPos * 3, 255 - WheelPos * 3, 0);
        }
    }

    // Calculate 50% dimmed version of a color (used by ScannerUpdate)
    uint32_t DimColor(uint32_t color)
    {
        // Shift R, G and B components one bit to the right
        uint32_t dimColor = branches[0].Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
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
  int currbranch = start;
  int nextbranch;
  int split_options [5][2]; // assume not more than 5 split options for a given branch
  int path_index = 0;
  int temp_endpoint;
  int split_count;
  bool end = false;
  //Serial.print("GEN PATH "); Serial.print(target); 
  //Serial.print(" starting branch "); Serial.print(currbranch); 
  //Serial.println(": ");
  while (!end) {
    // get the available splits for the current branch in the path
    split_count = 0;
    for (int j=0; j < ARRAY_SIZE(splits); j++) {
      if (splits[j][0] == currbranch) {
        split_options[split_count][0] = splits[j][2]; // split branch
        split_options[split_count][1] = splits[j][1]; // last segment before split
        split_count++;
      }
    }
    // add one of the split branches to the path, or not
    int temp = random(split_count + 1);
    int pixel = 0;
    //if (path_index > 0) {
    //  Serial.print(" -> ");
    //}
    //Serial.print(currbranch);
    // in the case where there is no avail split or just don't want to split
    if ((split_count == 0) || (split_count == temp)) {
      // add current branch up to the end and drop out of loop
      temp_endpoint = ARRAY_SIZE(segments[currbranch]);
      end = true;
      //if (split_count == 0) Serial.println("No splits.");
      //else Serial.println("Ignoring splits.");
    }
    // in the case where there are splits and want to use one
    else {
      // pick a split, add current branch up to skip
      temp_endpoint = split_options[temp][1] + 1;
      nextbranch = split_options[temp][0];
      //Serial.println("Using a split.");
    }
    for (int x = 0; x <= temp_endpoint; x++) {
      for (int y = 0; y < segments[currbranch][x]; y++) {
        pixel++;
      }
    }
    paths[target][path_index][0] = currbranch;
    paths[target][path_index][1] = 0;
    paths[target][path_index][2] = pixel;
    //Serial.print(" (0.."); Serial.print(pixel); Serial.println(")");
    if (!end) {
      currbranch = nextbranch;
    }
    path_index++;
  }
  // add merge branch if needed
  for (int j = 0; j < ARRAY_SIZE(merges); j++) {
    if (merges[j][0] == currbranch) {
      nextbranch = merges[j][1];
      // determine the led start position based on the segment of the merge branch
      int branchsize = 0;
      int startpixel = 0;
      for (int k = 0; k < ARRAY_SIZE(segments[nextbranch]); k++) {
          if (k < merges[j][2]) {
            startpixel += segments[nextbranch][k];
          }
          branchsize += segments[nextbranch][k];
      }
      paths[target][path_index][0] = nextbranch;
      paths[target][path_index][1] = startpixel;
      paths[target][path_index][2] = branchsize-1;
      //Serial.print(" -m> "); Serial.print(nextbranch);
      //Serial.print(" ("); Serial.print(startpixel); Serial.print(".."); Serial.print(branchsize-1); Serial.print(")");
    }
  }
  for (int i = 0; i < ARRAY_SIZE(paths[target]); i++) {
    //Serial.print(paths[target][i][0]);
    //Serial.print("("); Serial.print(paths[target][i][2]); Serial.print(")");
    //Serial.print(",");
  }
  //Serial.println(" Done.");
}

// game start animation
void gamestart() {
  Serial.println("Starting a new game...");
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "ACTIVATEWAV";
  if (! sfx.playTrack(soundfile) ) {
    Serial.print("Failed to play track."); Serial.println(soundfile);
  }
  for (int i=0; i<BRANCHES; i++) {
    branches[i].fill(colors[WHITE]);
    branches[i].show();
  }
  delay(1000);
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  for (int i=0; i<BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  delay(1000);
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < TOUCHPADS; i++) {
      if (! sfx.playTrack(gamenotes[i]) ) {
        Serial.print("Failed to play track."); Serial.println(gamenotes[i]);
      }
      branches[i].fill(colors[i]);
      branches[i].show();
      delay(100);
      if (! sfx.stop()) {
        Serial.println("Failed to stop");
        sfx.reset();
      }
      branches[i].clear();
      branches[i].show();
    }
  }
  delay(1000);
  currmode = GAME;
  memset(gamesteps, 0, sizeof gamesteps);
  gamecount = 0;
  trycount = 0;
  gamesteps[gamecount][0] = random(TOUCHPADS);
  run_prompt(1000);
}

// run through the prompt with current gamesteps
void run_prompt(int speed) {
  currstate = PROMPT;
  Serial.print("Prompt running for round "); Serial.print(gamecount); Serial.println(":");
  for (int i = 0; i < BRANCHES; i++) {
      branches[i].clear();
      branches[i].show();
  }
  for (int i=0; i<=gamecount; i++) {
    Serial.print(gamesteps[i][0]);
    if (!sfx.playTrack(gamenotes[gamesteps[i][0]])) {
      Serial.print("Failed to play track: "); Serial.println(gamenotes[gamesteps[i][0]]);
    }
    branches[gamesteps[i][0]].fill(colors[gamesteps[i][0]]);
    branches[gamesteps[i][0]].show();
    delay(speed);
    if (!sfx.stop()) {
      Serial.println("Failed to stop");
      sfx.reset();
    }
    branches[gamesteps[i][0]].clear();
    branches[gamesteps[i][0]].show();
    delay(10);
  }
  reset_touchtimeout();
  currstate = RESPONSE;
}

// game win animation then go to next mode
void gamewin(mode nextmode) {  
  Serial.println("Game is won!");
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "WIN     WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int x=0; x<4 ; x++) {
    for (int i=0; i<BRANCHES; i++) {
      branches[i].fill(colors[GREEN]);
      branches[i].show();
    }
    delay(500);
    if (! sfx.stop() ) Serial.println("Failed to stop");
    for (int i=0; i<BRANCHES; i++) {
      branches[i].clear();
      branches[i].show();
    }
    delay(10);
  }
  delay(2000);
  currmode = nextmode;
}

// game fail animation then go to next mode
void gamefail(mode nextmode) {  
  Serial.println("Game failed.");
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[20] = "FAIL    WAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int i = 0; i < BRANCHES; i++) {
    // all red and sad tone
    branches[i].fill(colors[RED]);
    branches[i].show();
  }
  delay(3000);
  if (! sfx.stop() ) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  for (int i=0; i<BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  delay(1000);
  // flash the correct branch
  for (int x=0; x<4 ; x++) {
    if (!sfx.playTrack(gamenotes[gamesteps[trycount][0]])) {
      Serial.print("Failed to play track: "); Serial.println(gamenotes[gamesteps[trycount][0]]);
    }
    branches[gamesteps[trycount][0]].fill(colors[gamesteps[trycount][0]]);
    branches[gamesteps[trycount][0]].show();
    delay(100);
    if (! sfx.stop() ) {
      Serial.println("Failed to stop");
      sfx.reset();
    }
    branches[gamesteps[trycount][0]].clear();
    branches[gamesteps[trycount][0]].show();
    delay(10);
  }
  delay(2000);
  currmode = nextmode;
}

// think animation
void think(int thoughts) {
  memset(paths, 0, sizeof paths);
  for (int i=0; i < BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  int branch_range = 5;
  Serial.println("Starting to think.");
  if (thoughts > branch_range) {
    thoughts = branch_range;
  }
  // make an array of unique starting branches, one for each thought
  int starting_branches[thoughts];
  int branch_count = 0;
  while (branch_count < thoughts) {
    int new_branch = random(branch_range);
    bool already_used = false;
    for (int i=0; i <= branch_count; i++) {
      if (starting_branches[i] == new_branch) {
        already_used = true;
      }
    }
    if (!already_used) {
      starting_branches[branch_count] = new_branch;
      branch_count++;
    }
  }
  // generate a new path for each thought
  for (int i = 0; i < thoughts; i++) {
    int currbranch = starting_branches[i];
    //Serial.print("Path: ");Serial.print(i);Serial.print(" ");
    gen_path(i, currbranch);
  }
  currmode = THINK;
  elapsedThinking = 0;
}

void fill_path(int path, uint32_t color) {
  for (int segment=0; segment < ARRAY_SIZE(paths[path]); segment++) {
    int first = paths[path][segment][1];
    int count = paths[path][segment][2] - paths[path][segment][1] + 1;  
    branches[paths[path][segment][0]].fill(color, first, count);
    branches[paths[path][segment][0]].show();
  }
}

void strike(int path_count, uint32_t color) {
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "LIGHTNINWAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  memset(paths, 0, sizeof paths);
  for (int i=0; i < BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  elapsedMillis sinceStrike;
  for (int i=0; i < path_count; i++) {
    Serial.println("Strike! ");
    gen_path(0);
    fill_path(0, color);
    delay(100);
    //while (sinceStrike < 2000) {
      for (int i=0; i<10; i++) {
        color = DimColor(color);
        fill_path(0, color);
        delay(10);
      }
    //}
  }
  for (int i=0; i < BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  elapsedLastStrike = 0;
}

void beat(int speed, uint32_t color) {
  if (! sfx.stop()) {
    Serial.println("Failed to stop");
    sfx.reset();
  }
  char soundfile[] = "HEARTBEAWAV";
  if (!sfx.playTrack(soundfile)) {
    Serial.print("Failed to play track: "); Serial.println(soundfile);
  }
  for (int i=0; i < BRANCHES; i++) {
    branches[i].clear();
    branches[i].show();
  }
  for (int j = 0; j < 2; j++) {
    uint nextcolor = color;
    elapsedMillis sincehalfbeat;
    for (int k = 0; k < 2; k++) {
      Serial.println("Beating!");
      for (int i=0; i < BRANCHES; i++) {
          branches[i].fill(color);
          branches[i].show();
      }
      sincehalfbeat = 0;
      while (sincehalfbeat < speed) {
        //Serial.print("dim beat ");Serial.println(sincehalfbeat);
        nextcolor = DimColor(nextcolor);
        for (int i=0; i < BRANCHES; i++) {
          branches[i].fill(nextcolor);
          branches[i].show();
        }
      }
    }
  }
  //for (int x = 0; x < 5; x++) {
  //  nextcolor = DimColor(nextcolor);
  //  for (int i=0; i < BRANCHES; i++) {
  //    branches[i].fill(DimColor(nextcolor));
  //    branches[i].show();
  //  }
  //  delay(1000);
  //}
  elapsedLastBeat = 0;
}

// the thinking animation
void pulse_step(int path, int index, uint32_t color1, uint32_t color2) {
  if (index == 0) {
    if (! sfx.stop() ) {
      Serial.println("Failed to stop");
      sfx.reset();
    }
    Serial.println("I have a thought!");
    char soundfile[] = "THINKINGWAV";
    if (!sfx.playTrack(soundfile)) {
      Serial.print("Failed to play track: "); Serial.println(soundfile);
    }
  }

  Struct position;
  uint32_t color = color1;
  if (random(2) == 1) {
  //if (0) {
    color = color2;
  }
  position = get_path_position(path, index);
  // if not found then make a new path
  if (position.branch == -1) {
    Serial.print("Path "); Serial.print(path);
    Serial.print(" index "); Serial.print(index);
    Serial.println(" not found, making a new one.");
    gen_path(path);
    index = 0;
    pulse_count[path] = 0;
    position = get_path_position(path, index);
  }
  branches[position.branch].setPixelColor(position.pixel, color);
  branches[position.branch].show();
  //Serial.print("path "); Serial.print(path); Serial.print(" branch "); Serial.print(position.branch); Serial.print(" pixel "); Serial.println(position.pixel);
  // set tail length
  int tail_length = 10;
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
      prevcolor = branches[position.branch].getPixelColor(position.pixel);
    }
    position = get_path_position(path, i - 1);
    branches[position.branch].setPixelColor(position.pixel, DimColor(prevcolor));
    //branches[position.branch].show();
  }
  int path_size = 0;
  for (int i = 0; i < ARRAY_SIZE(paths[path]); i++) {
    path_size += paths[path][i][2] - paths[path][i][1];
  }
  // check if at end of path, if so then reset to beginning
  if (pulse_count[path] < path_size) {
    pulse_count[path]++;
  }
  else {
    //Serial.print("Path "); Serial.print(path);
    //Serial.print(" reached end ("); Serial.print(path_size); Serial.print(")");
    //Serial.println("Making a new path.");
    fill_path(path, colors[BLANK]);
    pulse_count[path] = 0;
    gen_path(path);
    pulse_count[path] = 0;
  }
  //delay(200);
}

Struct get_path_position(int path, int index) {
  Struct position;
  position.branch = -1;
  position.pixel = 0;
  int counter = 0;
  for (int j=0; j < ARRAY_SIZE(paths[path]); j++) {
    for (int k=paths[path][j][1]; k <= paths[path][j][2]; k++) {
      if (counter == index) {
        position.branch = paths[path][j][0];
        position.pixel = k;
      }
      counter++;
    }
  }
  return position;
}

void test() {
  Serial.println("testing...");
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