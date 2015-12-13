/*
 * Description: 44-key IR remote customization.
 * Author: Dean Montgomery
 * Version: 1.0
 * Date: Dec 12, 2015
 * .
 * WS28012B Addressable RGB lights
 * 44-key infrared remote for led strip.
 * enhance the basic functions of the 44-key remote for better effects.
 * 
*/

#include "IRLremote.h"
#include "FastLED.h"
#include "EEPROM.h"


#define PIN_LED 3
#define NUM_LEDS 46
CRGB    leds[NUM_LEDS];
uint8_t order[NUM_LEDS];

#define IRL_BLOCKING true
#define PIN_IR 2
uint8_t oldSREG = 0;

uint8_t IRProtocol = 0;
//uint16_t IRAddress = 0;
uint32_t IRCommand = 0;

#define PIN_RELAY 4
bool big_light = HIGH;

unsigned long currentMillis = millis(); // define here so it does not redefine in the loop.
long    previousMillis = 0;
#define MIN_INTERVAL 40  // fastest the led libraries can update without loosing access to PIR remote sensing.
long    interval = MIN_INTERVAL;

#define BRIGHTNESS 190            //MAX brightness value.
uint8_t brightness = BRIGHTNESS;         // 0...255  ( used to brighten and dim colors this will be a fraction of BRIGHTNESS. 255=100% brightness which is 190 )
uint8_t bright[NUM_LEDS];         // used to track random taper off brightness.
uint8_t pause = 0;
uint8_t r = 0; //red
uint8_t g = 0; //green
uint8_t b = 0; //blue
uint8_t h = 0; //hue
uint8_t s = 0; //saturation
uint8_t v = 0; //value
uint8_t i = 0; // iterator
uint8_t x = 0; // variable.
uint8_t y = 0; // variable.
uint16_t z = 3; // for noise function.
uint16_t scale = 30; // for noise function.
uint8_t spd = 2; // speed of noise function
enum Rgb { RED, GREEN, BLUE };
#define SLIDE_VALUE 110
#define SLIDE_RGB 111
uint8_t slide = SLIDE_VALUE;
uint8_t last_white = 0;
CHSV water[NUM_LEDS];

uint8_t stage = 0; // variable to track stage/steps within an effect
uint32_t LastIRCommand = 0;
// effects
uint8_t effect = 0;
#define FADE7 1
#define FADE3 2
#define JUMP3 3
#define JUMP7 4
#define BRIGHT 5
#define DIM 6
#define FADE7B 7
#define FADE7C 8
#define RAIN 9
#define AURORA 10

#define DIMMING 55
struct REMOTE{
  uint32_t bright;   uint32_t dim;        uint32_t pause;     uint32_t power;
  uint32_t red1;     uint32_t green1;     uint32_t blue1;     uint32_t white1; 
  uint32_t red2;     uint32_t green2;     uint32_t blue2;     uint32_t white2; 
  uint32_t red3;     uint32_t green3;     uint32_t blue3;     uint32_t white3; 
  uint32_t red4;     uint32_t green4;     uint32_t blue4;     uint32_t white4; 
  uint32_t red5;     uint32_t green5;     uint32_t blue5;     uint32_t white5; 
  uint32_t red_up;   uint32_t green_up;   uint32_t blue_up;   uint32_t quick; 
  uint32_t red_down; uint32_t green_down; uint32_t blue_down; uint32_t slow; 
  uint32_t diy1;     uint32_t diy2;       uint32_t diy3;      uint32_t autom; 
  uint32_t diy4;     uint32_t diy5;       uint32_t diy6;      uint32_t flash; 
  uint32_t jump3;    uint32_t jump7;      uint32_t fade3;     uint32_t fade7;
};

// TODO: add scripts to detect and store values.  flash color that represents button - press button 3-4 times - store in eeprom.
// These are the codes for my remote, to decode your remote uncomment the "Serial" command in setup() and  "Serial.println(IRCommand);" command at the beginning of getButton() sections below.
const REMOTE remote = {
  41820,41565,48705,48960,
  42840,42585,47685,47940,
  43860,43605,46665,46920,
  44880,44625,45645,45900,
  58140,57885,57630,57375,
  59160,58905,58650,58395,
  60180,59925,59670,59415,
  61200,60945,60690,60435,
  62220,61965,61710,61455,
  63240,62985,62730,62475,
  64260,64005,63750,63495
};

// storing diy in eeprom
struct DIY {
  uint8_t r; uint8_t g; uint8_t b;
};
//Track start points in eeprom index
static uint16_t eeprom_addr[] = {0, 3, 6, 9, 12, 15};

struct DIY_BUTTON {
  uint8_t button;
  bool dirty;
};
DIY_BUTTON lastdiy = { 0, false };
/*
struct DIYSTORE {
  DIY diy1; DIY diy2; DIY diy3; // stored at 0,  3,  6
  DIY diy4; DIY diy5; DIY diy6; // stored at 9, 12, 15
};
 size of EEPROM: 1024 bytes
 size of 44 key remote: 176 bytes
 size of DIY: 3 bytes
 size of DIYSTORE: 18 bytes
 */

void setup()
{
  delay (3000);
  //Serial.begin(115200);
  // pin change mask registers decide which pins are enabled as triggers
  //PCMSK |= (1 << PCINT);
  // PCICR: Pin Change Interrupt Control Register - enables interrupt vectors
  //PCICR |= (1 << PCIE);
  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setDither( 0 );  // Stops flikering in animations.
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);

  attachInterrupt(digitalPinToInterrupt(PIN_IR), IRLinterrupt<IR_NEC>, CHANGE);
  setColor("pw", 0, 0, 0);

  /*
  int sz = EEPROM.length();
  Serial.print (" size of EEPROM: ");
  Serial.println(sz);
  sz = sizeof(remote);
  Serial.print (" size of 44 key remote: ");
  Serial.println(sz);
  sz = sizeof(diy);
  Serial.print (" size of DIY: ");
  Serial.println(sz);
  sz = sizeof(diystore);
  Serial.print (" size of DIYSTORE: ");
  Serial.println(sz);
*/
}

void loop()
{
  currentMillis = millis();
  getButton();
  
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    if (effect >= 1){
      update_effect(); 
      FastLED.show();
    }
  }
}

void getButton(){
  // temporary disable interrupts
  oldSREG = SREG;
  cli();
  
  if (IRProtocol) {
    //Serial.println(IRCommand);  // NOTE: uncomment this if you need to decode your remote
    if(IRCommand == 65535){
      IRCommand = LastIRCommand;
    } else {
      LastIRCommand = IRCommand;
    }
    if(IRCommand == remote.bright){
      //Serial.println("bright");
      // TODO: ... LastIRCommand = bright
      if ( effect == AURORA ){
        scale = qadd8(scale, 2);
      } else if ( slide == SLIDE_RGB ){
        r = leds[1].r;
        g = leds[1].g;
        b = leds[1].b;
        if ( r + 1 < 253 && g + 1 < 253 && b + 1 < 253 ){
          r += 2; g += 2; b += 2;
          fill_solid(leds, NUM_LEDS, CRGB(r,g,b));
        }
        effect = 0;
        FastLED.show();
      } else {
        if ( brightness <= 250 ){
          brightness += 5;
        }
        if ( effect == 0 ){
          effect = BRIGHT;
        }
      }
      //Serial.println(brightness);
    }
    if(IRCommand == remote.dim){
      //Serial.println("dim");
      if ( effect == AURORA ){
        scale = qsub8(scale, 2);
      } else if ( slide == SLIDE_RGB ){
        for ( i = 0; i < NUM_LEDS; i++ ){
          leds[i].nscale8_video(240);
        }
        r = leds[1].r;
        g = leds[1].g;
        b = leds[1].b;
        //fill_solid(leds, NUM_LEDS, CRGB(r,g,b));
        effect = 0;
        FastLED.show();
      } else {
        if ( brightness >= 10 ) {
          brightness -= 5;
        } else if ( brightness >= 1 ){
          brightness -= 1;
        }
        if ( effect == 0 ){
          effect = DIM;
        }
      }
      
      //Serial.println(brightness);
    }
    if(IRCommand == remote.pause){
        //Serial.println("pause");
        if ( pause == 0 ){
          pause = effect;
          effect = 0;
        } else {
          effect = pause;
          pause = 0;
        }
    }
    if(IRCommand == remote.power){
        setColor("pw", 0, 0, 0);
    }
    if(IRCommand == remote.red1){
        setColor("r1", 0, 255, brightness);
    }
    if(IRCommand == remote.green1){
        setColor("g1", 96, 255, brightness);
    }
    if(IRCommand == remote.blue1){
        setColor("b1", 160, 255, brightness);
    }
    if(IRCommand == remote.white1){
        setColor("w1", 0, 0, brightness);
    }
    if(IRCommand == remote.red2){
        setColor("r2", 16, 255, brightness);
    }
    if(IRCommand == remote.green2){
        setColor("g2", 112, 255, brightness);
    }
    if(IRCommand == remote.blue2){
        setColor("b2", 176, 255, brightness);
    }
    if(IRCommand == remote.white2){
        //setColor("w2", 16, 50, brightness);
        if (big_light == LOW) {
          digitalWrite(PIN_RELAY, HIGH);
          big_light = HIGH;
        } else {
          digitalWrite(PIN_RELAY, LOW);
          big_light = LOW;
        }
    }
    if(IRCommand == remote.red3){
        setColor("r3", 32, 255, brightness);
    }
    if(IRCommand == remote.green3){
        setColor("g3", 120, 255, brightness);
    }
    if(IRCommand == remote.blue3){
        setColor("b3", 192, 255, brightness);
    }
    if(IRCommand == remote.white3){
        setColor("w3", 216, 80, brightness);
    }
    if(IRCommand == remote.red4){
        setColor("r4", 48, 255, brightness);
    }
    if(IRCommand == remote.green4){
        setColor("g4", 128, 255, brightness);
    }
    if(IRCommand == remote.blue4){
        setColor("b4", 208, 255, brightness);
    }
    if(IRCommand == remote.white4){
        setColor("w4", 96, 70, brightness);
    }
    if(IRCommand == remote.red5){
        setColor("r5", 64, 255, brightness);
    }
    if(IRCommand == remote.green5){
        setColor("g5", 136, 255, brightness);
    }
    if(IRCommand == remote.blue5){
        setColor("b5", 224, 255, brightness);
    }
    if(IRCommand == remote.white5){
        setColor("w5", 64, 60, brightness);
    }
    if(IRCommand == remote.red_up){
      colorUpDown(RED, 1); 
    }
    if(IRCommand == remote.green_up){
      colorUpDown(GREEN, 1);
    }
    if(IRCommand == remote.blue_up){
      colorUpDown(BLUE, 1);
    }
    if(IRCommand == remote.quick){
      if (effect == AURORA){
        spd = qadd8(spd, 1);
      } else {
        if ( interval >= 101 ){
          interval -= 100;
        } else if ( interval >= MIN_INTERVAL + 10 && interval <= 100 ){
          interval -= 10;
        } else {
          interval = MIN_INTERVAL;
        }
      }
    }
    if(IRCommand == remote.red_down){
      colorUpDown(RED, -1);
    }
    if(IRCommand == remote.green_down){
      colorUpDown(GREEN, -1);
    }
    if(IRCommand == remote.blue_down){
      colorUpDown(BLUE, -1);
    }
    if(IRCommand == remote.slow){
      if (effect == AURORA){
        spd = qsub8(spd, 1);
      } else {
        //Serial.println("slow");
        if (interval >= 100){
          interval += 100;
        } else {
          interval += 15;
        }
      }
    }
    if(IRCommand == remote.diy1){
      updateDiy(1);
    }
    if(IRCommand == remote.diy2){
      updateDiy(2);
    }
    if(IRCommand == remote.diy3){
      updateDiy(3);
    }
    if(IRCommand == remote.autom){
      effect = RAIN;
      slide = SLIDE_VALUE;
      for (i = 0; i < NUM_LEDS; i++ ){
        bright[i] = brightness;
        water[i] = CHSV(h,s,v);
      }
      interval = MIN_INTERVAL+20;
      //Serial.println("auto");
    }
    if(IRCommand == remote.diy4){
      updateDiy(4);
    }
    if(IRCommand == remote.diy5){
      updateDiy(5);
    }
    if(IRCommand == remote.diy6){
      updateDiy(6);
    }
    if(IRCommand == remote.flash){
        //Serial.println("flash");
    }
    if(IRCommand == remote.jump3){
        //Serial.println("jump 3");
        stage = 1;
        interval = 1200;
        h=0; s=255; v=0;
        effect = JUMP3;
        slide = SLIDE_VALUE;
    }
    if(IRCommand == remote.jump7){
        //Serial.println("jump 7");
        stage = 1;
        interval = 1700;
        h=0; s=255; v=0;
        effect = JUMP7;
        slide = SLIDE_VALUE;
    }
    if(IRCommand == remote.fade3){
        //Serial.println("fade 3");
        stage = 1;
        interval = MIN_INTERVAL;
        h=0; s=255; v=0;
        effect = FADE3;
        slide = SLIDE_VALUE;
    }
    if(IRCommand == remote.fade7){
        // multiple clicks chooses next effect.
        interval = 65;
        if (effect == FADE7){
          effect = FADE7B; 
        } else if (effect == FADE7B) {
          effect = AURORA;
          interval = 55;
          spd = 2;
          scale = 10;
        } else {
          effect = FADE7;
        }
        stage = 1;
        slide = SLIDE_VALUE;
        h=0; s=255; v=brightness;
    }
    if(IRCommand == 65535){
        //Serial.println("repeat");
    }
    IRProtocol = 0;
  }
  SREG = oldSREG;
}

void IREvent(uint8_t protocol, uint16_t address, uint32_t command) {
  // called when directly received a valid IR signal.
  // do not use Serial inside, it can crash your program!

  // dont update value if blocking is enabled
  if (IRL_BLOCKING && !IRProtocol) {
    // update the values to the newest valid input
    IRProtocol = protocol;
    //IRAddress = address;
    IRCommand = command;
  }
}

void update_effect(){
  if ( effect == FADE7 ){
    fade7();
  } else if ( effect == AURORA ){
    aurora();
  } else if ( effect == RAIN ){
    rain();
  } else if ( effect == FADE3 ){
    fade3();
  } else if ( effect == JUMP3 ){
    jump3();
  } else if ( effect == JUMP7 ){
    jump7();
  } else if ( effect == FADE7B ){
    fade7b();
  } else if ( effect == FADE7C ){
    fade7c();
  } else if ( effect == BRIGHT ){
    v = brightness;
    fill_solid(leds, NUM_LEDS, CHSV(h,s,v));
    effect = 0;
  } else if ( effect == DIM ){
    v = brightness;
    fill_solid(leds, NUM_LEDS, CHSV(h,s,v));
    effect = 0;
  }
}

void setColor(String str, uint8_t hue, uint8_t sat, uint8_t value){
  //"red 1", 0, 255, brightness);
  effect = 0;
  //Serial.print(str[0]);
  //Serial.println(str[1]);
  h = hue; s = sat; v = value;
  if ( hue == 0 && sat == 0 && value != 0 ){
    const CRGB whites[] = {
      Candle,                  Tungsten40W,          Tungsten100W,          Halogen,
      CarbonArc,               HighNoonSun,          DirectSunlight,        OvercastSky, 
      ClearBlueSky,            WarmFluorescent,      StandardFluorescent,   CoolWhiteFluorescent,
      FullSpectrumFluorescent, GrowLightFluorescent, BlackLightFluorescent, MercuryVapor,
      SodiumVapor,             MetalHalide,          HighPressureSodium
    };
    //CRGB wt = whites[last_white];
    //wt.nscale8_video(BRIGHTNESS); // dim to max brightness
    fill_solid(leds, NUM_LEDS, whites[last_white]);
    if (last_white++ >= 18){
      last_white = 0;
    }
    slide = SLIDE_RGB;
  } else {
    slide = SLIDE_VALUE;
    fill_solid(leds, NUM_LEDS, CHSV(h,s,v));
  }
  FastLED.show();
}

void colorUpDown(int color, int8_t val){
  effect = 0;
  r = leds[1].r; g = leds[1].g; b = leds[1].b;
  uint8_t z[] = { r, g, b };
  if ( z[color] + val >= 1 && z[color] + val <= 250 ){
    if ( color == RED   ) { r += val; }
    if ( color == GREEN ) { g += val; }
    if ( color == BLUE  ) { b += val; }
    lastdiy.dirty = true;
    fill_solid(leds, NUM_LEDS, CRGB( r, g, b ));
  }
  slide = SLIDE_RGB;
  FastLED.show();
  //Serial.print("updown ");
  //Serial.print(color);
  //Serial.print(" ");
  //Serial.println(val);
}
      

void updateDiy(uint8_t num){
  effect = 0;
  //Serial.print("diy ");
  //Serial.println(num);
  if ( lastdiy.button == num && lastdiy.dirty == true){
    // avoid 100,000 write cycle limit by only writing what is needed.
    EEPROM.update(eeprom_addr[num], r);
    EEPROM.update(eeprom_addr[num] + 1, g);
    EEPROM.update(eeprom_addr[num] + 2, b);
    lastdiy.dirty = false;
  } else {
    lastdiy.button = num;
    lastdiy.dirty = false;
    DIY diy;
    EEPROM.get(eeprom_addr[num], diy);
    // default was 255 so make unset value a dull grey 
    if ( diy.r > BRIGHTNESS ) diy.r = BRIGHTNESS/3;
    if ( diy.g > BRIGHTNESS ) diy.g = BRIGHTNESS/3;
    if ( diy.b > BRIGHTNESS ) diy.b = BRIGHTNESS/3;
    r = diy.r;
    g = diy.g;
    b = diy.b;
    fill_solid(leds, NUM_LEDS, CRGB( r, g, b ));
  }
  slide = SLIDE_RGB;
  FastLED.show();
}

void fade3(){
  if ( stage == 1 ) {
    h=HUE_RED;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v++ ));
    if (v >= brightness ) { stage = 2; }  
  } else if ( stage == 2 ) {
    h=HUE_RED;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v-- ));
    if (v <= 1 ) { stage = 3; }  
  } else if ( stage == 3 ) {
    h=HUE_GREEN;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v++ ));
    if (v >= brightness ) { stage = 4; }  
  } else if ( stage == 4 ) {
    h=HUE_GREEN;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v-- ));
    if (v <= 1 ) { stage = 5; }  
  } else if ( stage == 5 ) {
    h=HUE_BLUE;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v++ ));
    if (v >= brightness ) { stage = 6; }  
  } else if ( stage == 6 ) {
    h=HUE_BLUE;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v-- ));
    if (v <= 1 ) { stage = 1; }  
  }
}

void fade7(){
  for ( i = 0; i <=NUM_LEDS; i++ ){
    leds[i] = CHSV(h+(i*3), s, brightness);
  }
  h++;
}

void fade7b(){
  if ( stage == 1 ) {
    fill_solid(leds, NUM_LEDS, CHSV( h++, s, brightness ));
    if ( h >= 254 ) { stage = 2; }
  } else if ( stage == 2 ) {
    h = 0;
    fill_solid(leds, NUM_LEDS, CHSV( h, s-=2, brightness ));
    if ( s <= 1 ) { s=0; stage = 3; }
  } else if ( stage == 3 ) {
    fill_solid(leds, NUM_LEDS, CHSV( h, s+=2, brightness ));
    if ( s >= 253  ) { s=255; stage = 1; }
  }
}

void fade7c(){
  for ( i = 0; i <=NUM_LEDS; i++ ){
    leds[i] = CHSV(h+(i*3), s, brightness);
  } 
  h++;
}

void jump3(){
  s=255;
  v=brightness;
  //Serial.println(stage);
  if ( stage == 1 ) {
    h=HUE_RED;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v ));
    stage = 2; 
  } else if ( stage == 2 ) {
    h=HUE_GREEN;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v ));
    stage = 3; 
  } else if ( stage == 3 ) {
    h=HUE_BLUE;
    fill_solid(leds, NUM_LEDS, CHSV( h, s, v ));
    stage = 1; 
  }
}


void jump7(){
  if ( stage == 1 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_RED, 255, brightness ));
    stage = 2; 
  } else if ( stage == 2 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_ORANGE, 255, brightness ));
    stage = 3; 
  } else if ( stage == 3 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_YELLOW, 255, brightness ));
    stage = 4; 
  } else if ( stage == 4 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_GREEN, 255, brightness ));
    stage = 5; 
  } else if ( stage == 5 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_AQUA, 255, brightness ));
    stage = 6; 
  } else if ( stage == 6 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_BLUE, 255, brightness ));
    stage = 7; 
  } else if ( stage == 7 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_PURPLE, 255, brightness ));
    stage = 8; 
  } else if ( stage == 8 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_PINK, 255, brightness ));
    stage = 9; 
  } else if ( stage == 9 ) {
    fill_solid(leds, NUM_LEDS, CHSV( HUE_PINK, 0, brightness ));
    stage = 1; 
  }
}

void flash(){
  // TODO
  
}

void autom(){
  //TODO
}

void aurora(){
  for ( i = 0; i <=NUM_LEDS; i++ ){
    v = inoise8(i*3*scale, z);
    h = inoise8(i*scale, z);
    h = qsub8(h,16);
    h = qadd8(h,scale8(h,39));
    leds[i] = CHSV(h,255,v);
  }
  z += spd; // this is speed.  
}
void rain(){
  // this is nice ripple.
  x = 0;
  for ( i = 0; i <=NUM_LEDS; i++ ){
    // todo small sin within large sin wave
    v = triwave8(y + (i * 30) + scale8(triwave8(i*43), 20));
    if ( v > x && qsub8(v, x) > 150 ) {
      s = random8(100,150);
      v = qadd8(v, random8(10,20));
    } else if ( random8() < 50 ) {
      s = 150;
      v = qadd8(v, random8(5));
    } else {
      s = 255;
    }
    //v = cubicwave8(brightness + (i * 30));
    leds[i]            = CHSV(h, s, v);
    //leds[NUM_LEDS - i] = CHSV(h, s, triwave8(brightness + (i * 20)));
  }
  y+=random8(10,20);
  /*
  brightness+=random8(10,20);
  for ( i = 0; i <=NUM_LEDS; i++ ){
    v = sin8(brightness + (i * 30));
    leds[i]            = CHSV(h, s, v);
    //if ( i > NUM_LEDS/2 - 10 && i < NUM_LEDS/2 + 10 && random8() < 60 ){
    if (random8() < 60 ){
      leds[i] = CHSV(h, 160, qadd8(v,5));
    }
    //leds[NUM_LEDS - i] = CHSV(h, s, triwave8(brightness + (i * 20)));
  }
  brightness+=random8(10,20);
  */

  // This was attempt 2
  /*
  for ( i = 0; i <= NUM_LEDS; i++ ){
    //water[i].hue = random8(qsub8(water[i].hue, 3), qadd8(h, 3));
    //water[i].sat = random8(qsub8(water[i].sat, 2), qadd8(water[i].sat, 3));
    if ( i < NUM_LEDS ){
      water[i].sat = random8(qsub8(water[i+1].sat, 2), qadd8(water[i+1].sat, 4));
      water[i].val = random8(qsub8(water[i+1].val, 2), qadd8(water[i+1].val, 3));
    } else {
      water[i].sat = random8(qsub8(water[i].sat, 2), qadd8(water[i].sat, 5));
      water[i].val = random8(qsub8(water[i].val, 2), qadd8(water[i].val, 3));
    }
    leds[i] = water[i];
  }
  */

  // This was a different attempt
  /*
  h++; 
  // dim them all
  for (i = 0; i < NUM_LEDS; i++ ){
    bright[i] = qsub8( bright[i], 1);
  }
  // add random drops of light
  if ( random8(0, 255) > 250 ){
    uint8_t brt = random8(brightness - 5, brightness);
    x = random8(NUM_LEDS-1);
    bright[i] = brt;
    uint8_t z = random8(2,3);
    for ( uint8_t y = 1; y <= z; y++){
      if ( (x - y) >= 0 ) {
        bright[x - y] = qsub8(brt, 2);
      }
      if ( (x + y) < NUM_LEDS ){
        bright[x + y] = qsub8(brt, 2);
      }
    }
    if ( slide == SLIDE_VALUE ){
      for ( i = 0; i < NUM_LEDS; i++){
        leds[i] =  CHSV (h, s, bright[i]); 
      }
    }
  }
  */
  //Serial.println(bright[i]);
  
}



