#include "FastSPI_LED2.h"
#include "Streaming.h"

// Game of Life parameters

// Loop time of main loop in ms (20 default -> 50 Hz)
#define LOOP_TIME 20
// Time between GoL steps in ms (5000 default -> one step every 5 s)
#define UPDATE_TIME 5000
// How long to wait with seeding a new field after a stall or period-2 oscillations (ms)
#define SEED_DELAY 15000
// Hue is linearly interpolated between randomised values
// the speed of the interpolation is 255 * HUE_SLOWDOWN * LOOP_TIME ~= 5s * HUE_SLOWDOWN
#define HUE_SLOWDOWN 10

//random colour parameters (min/max for each h,s,v)
#define H_MIN 0
#define H_MAX 255
#define S_MIN 180
#define S_MAX 255
#define V_MIN 180
#define V_MAX 255


// End of GoL parameters, do not change below


#define ROWS 10
#define COLS 10
#define NUM_LEDS (ROWS*COLS)

// Init/define led arrays
CRGB leds[NUM_LEDS];    //current colours
CRGB newleds[NUM_LEDS]; //new colours

// Return 0-based led index for 0-based x and y grid co-ordinate values
// The LED string snakes through the grid, so every row the positive x direction switches
uint8_t xy2i(uint8_t x, uint8_t y){
  if(y%2){
    // It's an uneven row: x should be reversed---or subtracted from this line's
    //last index, which is the next line's first minus 1
    return (COLS*(y+1) - 1) - x;
  } else {
    // Even row: simply do COLS*y + x
    return COLS*y + x;
  }
}

// currently-alive cells
boolean alive[ROWS][COLS];
// cells alive in next step
boolean next[ROWS][COLS];
// keep previous state in memory to detect period-1 oscillations
boolean prev[ROWS][COLS];

uint8_t getNeighbour(int8_t y, int8_t x){
  if( y<0 || y>=ROWS || x<0 || x>=COLS) return 0;
  if(alive[y][x]) return 1;
  return 0;
}

void GoLseed(){
  // seed initial cells
  for(uint8_t y=0; y!=ROWS; ++y){
    for(uint8_t x=0; x!=COLS; ++x){
      alive[y][x] = random(0,10)>5;
    }
  }
}

// Hue, saturation and value definition need to be here
uint8_t hue, saturation, value;

// Flag and time are used to seed a new game with a delay
boolean seedFlag = false;
unsigned long seedTime = 0; 
void GoLstep(){
  //execute a GoL-step
  
  // Loop through all cells and determine their next state
  for(uint8_t y=0; y!=ROWS; ++y){
    for(uint8_t x=0; x!=COLS; ++x){
      // First reset the next array
      next[y][x] = false;
      // Then calculate number of neighbours (n)
      uint8_t n = 0;
      // loop through all neighbouring cells
      for(int8_t dy=-1; dy!=2; ++dy){
        for(int8_t dx=-1; dx!=2; ++dx){
          if( dx==0 && dy==0 ) continue; //do not count itself
          n += getNeighbour((int8_t)y+dy,(int8_t)x+dx);
        }
      }
      // Make a decision based on number of alive neighbours
      if(n==3){
        // keeps living or is born next round
        next[y][x] = true;
      } else if(alive[y][x] && n==2){
        //stays alive with 2 neighbours
        next[y][x] = true;
      }
      // All other cases: die or stay dead
    }
  }
  // Try to find a stall, or period-1 oscillation
  boolean same_prev = true;
  boolean same_alive = true;
  for(uint8_t y=0; y!=ROWS; ++y){
    for(uint8_t x=0; x!=COLS; ++x ){
      if(next[y][x] != prev[y][x]) same_prev = false;
      if(next[y][x] != alive[y][x]) same_alive = false;
    }
  }
  if((same_prev || same_alive) && !seedFlag){
    seedFlag = true;
    seedTime = millis();
  }
  
  // Now copy current to prev and next to current (alive)
  memcpy(prev,alive,sizeof(alive));
  memcpy(alive,next,sizeof(next));
  
  // Finally, update the newleds array
  for(uint8_t y=0; y!=ROWS; ++y){
    for(uint8_t x=0; x!=COLS; ++x){
      if(alive[y][x]){
        newleds[xy2i(x,y)].setHSV(hue, saturation, value);
      } else {
        newleds[xy2i(x,y)] = CRGB::Black;
      }
    }
  }
}

//all living cells have a slowly changing colour
uint8_t from_hue, to_hue, from_sat, to_sat, from_val, to_val; 
void generateColour(){
  to_hue = random(H_MIN,H_MAX);
  to_sat = random(S_MIN,S_MAX);
  to_val = random(V_MIN,V_MAX);
}
uint8_t interpolate(uint8_t from, uint8_t to, uint8_t frac){
  //interpolate between FROM and TO, based on FRAC
  return (uint8_t) (( (uint16_t)from * (255-frac) + (uint16_t)to * frac ) / 255);
}
uint16_t hue_i = 0;
void sendColors(){
  // interpolate between from_hue and to_hue
  if(++hue_i == HUE_SLOWDOWN*255){
    //select a new hue
    from_hue = to_hue;
    from_sat = to_sat;
    from_val = to_val;
    generateColour();
    
    hue_i = 0;
  }
  hue = interpolate(from_hue,to_hue,hue_i/HUE_SLOWDOWN);
  saturation = interpolate(from_sat,to_sat,hue_i/HUE_SLOWDOWN);
  value = interpolate(from_val,to_val,hue_i/HUE_SLOWDOWN);
  
  // Update the hue for the ALIVE leds
  for(uint8_t y=0; y!=ROWS; ++y){
    for(uint8_t x=0; x!=COLS; ++x){
      if(alive[y][x]){
        newleds[xy2i(x,y)].setHSV(hue, saturation, value);
      } else {
        newleds[xy2i(x,y)] = CRGB::Black;
      }
    }
  }
  
  for(uint8_t i_led=0; i_led!=NUM_LEDS; ++i_led){
    for(uint8_t i=0; i!=3; ++i)
      leds[i_led][i] = (uint8_t) (((uint16_t) leds[i_led][i]*24 + newleds[i_led][i])/25);
  }
  
  LEDS.show();
}



void setup(){
  LEDS.addLeds<WS2801, RGB>(leds, NUM_LEDS);
  // Limit the brightness somewhat (scale is 0-255)
  LEDS.setBrightness(127);
  
  // Initialise the leds to black (off)
  for(uint8_t i=0; i<NUM_LEDS; ++i){
    leds[i] = CRGB::Black;
  }
  LEDS.show();
  
  
  randomSeed(analogRead(A3));
  // generate initial hues and saturation
  generateColour();
  // copy these to from_X
  from_hue = to_hue; from_val = to_val; from_sat = to_sat;
  // generate new TO-colour
  generateColour();
  
  // seed the first cells
  GoLseed();
}


unsigned long lastLoopTime = 0;
unsigned long lastUpdateTime = 0;
void loop(){
  if(seedFlag && millis()-seedTime > SEED_DELAY){
    seedFlag = false;
    GoLseed();
  }
  if(millis()-lastUpdateTime > UPDATE_TIME){
    GoLstep();
    lastUpdateTime = millis();
  }
  sendColors();
  
  while(millis()-lastLoopTime < LOOP_TIME){
    //wait
  }
  lastLoopTime = millis();
}






