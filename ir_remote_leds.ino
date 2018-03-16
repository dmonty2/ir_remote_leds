/*
 * Description: FastLED + 44-key IR remote + MySensors
 * Author: Dean Montgomery
 * Version: 1.3
 * Date: Dec 12, 2015
 * Update: Jan 7, 2015 - add shutdown timer
 * Update: Feb 20, 2017 - Update support for newer FastLED and IRLremote libraries.
 * Update: Mar 7, 2017 - add support for MySensors mesh network. Various small updates.
 * .
 * WS28012B Addressable RGB lights
 * 44-key infrared remote for led strip.
 * enhance the basic functions of the 44-key remote for better effects.
 * 
*/

#include "IRLremote.h"
#include "FastLED.h"
#include "EEPROM.h"

//#define MY_DEBUG 
//#define MY_PARENT_NODE_ID 0
#define MY_RF24_IRQ_PIN 3
#define MY_RADIO_NRF24
#include <MySensors.h>

// === Variables to be set ===
#define NUM_LEDS 46        // Number of leds in the strip
#define PIN_IR 2           // Pin for infrared remote - must be a pin that supports inturrupt
#define PIN_LED 5          // Arduino pin for LEDs
#define PIN_RELAY 7        // Arduino pin for relay switch
#define BRIGHTNESS 200     // MAX brightness value.
CNec IRLremote;            // Remote controll type - see IRLremote library examples.

// === Remote setup ===
struct REMOTE{
  uint16_t bright;   uint16_t dim;        uint16_t pause;     uint16_t power;
  uint16_t red1;     uint16_t green1;     uint16_t blue1;     uint16_t white1; 
  uint16_t red2;     uint16_t green2;     uint16_t blue2;     uint16_t white2; 
  uint16_t red3;     uint16_t green3;     uint16_t blue3;     uint16_t white3; 
  uint16_t red4;     uint16_t green4;     uint16_t blue4;     uint16_t white4; 
  uint16_t red5;     uint16_t green5;     uint16_t blue5;     uint16_t white5; 
  uint16_t red_up;   uint16_t green_up;   uint16_t blue_up;   uint16_t quick; 
  uint16_t red_down; uint16_t green_down; uint16_t blue_down; uint16_t slow; 
  uint16_t diy1;     uint16_t diy2;       uint16_t diy3;      uint16_t autom; 
  uint16_t diy4;     uint16_t diy5;       uint16_t diy6;      uint16_t flash; 
  uint16_t jump3;    uint16_t jump7;      uint16_t fade3;     uint16_t fade7;
};

// These are the codes for my remote. Use IRLremote Receive example to decode your remote.
const REMOTE remote = {
  0x5C,0x5D,0x41,0x40,
  0x58,0x59,0x45,0x44,
  0x54,0x55,0x49,0x48,
  0x50,0x51,0x4D,0x4C,
  0x1C,0x1D,0x1E,0x1F,
  0x18,0x19,0x1A,0x1B,
  0x14,0x15,0x16,0x17,
  0x10,0x11,0x12,0x13,
  0x0C,0x0D,0x0E,0x0F,
  0x08,0x09,0x0A,0x0B,
  0x04,0x05,0x06,0x07
};

// === Application variables ==
CRGB leds[NUM_LEDS];
CRGB c1;
CRGB c2;
CRGB trans;
bool big_light = HIGH;      // Track light attached to relay switch. Power off on boot.
bool led_light = HIGH;      // Track led light on/off.  Send power off on boot.
unsigned long currentMillis = millis(); // define here so it does not redefine in the loop.
unsigned long previousMillis = 0;
unsigned long previousDebounce = 0;
unsigned long previousOffMillis = 0; // countdown power off timer
//static long offintrvl = 14400000; // 1000mills * 60sec * 60min * 4hour = 14400000;
static long offintrvl = 30000;
#define DEBOUNCE 500
#define MIN_intrvl 40  // fastest the led libraries can update without loosing access to PIR remote sensing.
long    intrvl = MIN_intrvl;
long    slowdown = 2;
uint8_t brightness = BRIGHTNESS;   // 0...255  ( used to brighten and dim colors this will be a fraction of BRIGHTNESS. 255=100% brightness which is 190 )
uint8_t brightness2 = brightness;  // used to animate brightness change.
uint8_t gBright = 0;
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
uint8_t scale = 30; // for noise function.
uint8_t spd = 2; // speed of noise function
enum Rgb { RED, GREEN, BLUE };
#define SLIDE_VALUE 110
#define SLIDE_RGB 111
uint8_t slide = SLIDE_VALUE;
uint8_t last_white = 0;

uint8_t stage = 0; // variable to track stage/steps within an effect
uint16_t IRCommand = 0;
uint16_t LastIRCommand = 0;
uint8_t dir = 0;
// effects
uint8_t effect = 0;
#define NO_EFFECT 0
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
#define WIPE 11
#define AUTOM 12
#define ADJUST_BRIGHTNESS 55

// MySensors
MyMessage msg_ALL (0,0);  // initate and re-use to save memory.
#define LONG_WAIT 750                    // long wait between signals
#define SHORT_WAIT 50                    // short wait between signals
#define ID_S_LIGHT_RELAY 1
#define ID_S_RGB_LIGHT 2
// There be dragons - MySensors already uses EEPROM and me lazy.
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
void presentation()
{
  sendSketchInfo("Fireplace Mantel", "1.0");
  wait(LONG_WAIT);

  present(ID_S_LIGHT_RELAY, S_BINARY,"FPMantelLight", true);
  wait(LONG_WAIT);
  present(ID_S_RGB_LIGHT, S_RGB_LIGHT, "FPMantelRGB", true );
  wait(LONG_WAIT);
}

void setup()
{
  delay (3000);
  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setDither( 0 );  // Stops flikering in animations. Helps infrared and MySensor inturrupts.
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);
  if (!IRLremote.begin(PIN_IR)){
    //Serial.println(F("You did not choose a valid pin."));
  }
  led_off();
}

void loop()
{
  currentMillis = millis();
  if (IRLremote.available()){
    getButton();
  }
  if (!IRLremote.receiving()) {
    if(currentMillis - previousMillis > intrvl) {
      previousMillis = currentMillis;
      if (effect >= 1){
        update_effect(); 
        FastLED.show();
        if ( slowdown >= 2 ){
          slowdown--;
        }
      }
    }
    if(currentMillis - previousOffMillis > offintrvl) {
      if(led_light == 1){
        led_off();
      }
      if(big_light == 1){
        relay_off();
      }
      previousOffMillis = currentMillis;
      big_light=0;
      wait(LONG_WAIT);
      digitalWrite(PIN_RELAY, !big_light);
      prepMsg(ID_S_LIGHT_RELAY,V_STATUS);
      send(msg_ALL.set(newLight == 1 ? "1" : "0" ));
      wait(SHORT_WAIT);
    }
  }
}

void receive(const MyMessage &message){
  // NOTE: server requests ACK which client auto-sends - no need to send 2nd ACK.
  previousOffMillis = currentMillis; // reset shutdown timer on received message.
  switch (message.type) {  
    case V_RGB:
      if(message.sensor==ID_S_RGB_LIGHT){
        // MySensors/Domoticz sends 2 messages: 1st V_RGB then 2nd V_PERCENTAGE.
        // Set the effect to WIPE but wait a bit to ensure we receive and set V_PERCENTAGE before applying the WIPE.
        if ( effect != WIPE ) { // Don't run again if already running.
          String hexstring = message.getString(); //here goes the hex color code coming from through MySensors (like FF9A00)
          long number = (long) strtol( &hexstring[0], NULL, 16);
          intrvl = LONG_WAIT;
          previousMillis = millis();
          brightness2 = brightness;
          effect = WIPE;
          stage = 1;
          r = (uint8_t) (number >> 16);
          g = (uint8_t) (number >> 8 & 0xFF);
          b = (uint8_t) (number & 0xFF);
        }
      }
      break;
    case V_PERCENTAGE:
      if(message.sensor==ID_S_RGB_LIGHT) {
        int requested_level = message.getInt();
        brightness2 = (uint8_t)(round(2.55*requested_level));
        brightness2 = (brightness2 > BRIGHTNESS ? BRIGHTNESS : brightness2);
        if ( effect == 0 ){
          effect = ADJUST_BRIGHTNESS;
          stage = 1;
        }
      }
      break;
    case V_STATUS:
      if (message.sensor==ID_S_RGB_LIGHT) {
        prepMsg(ID_S_RGB_LIGHT,V_STATUS);
        if (message.getInt() == 0) {
          led_off();
        } else {
          setColor(0, 0, brightness);
        }
      } else if (message.sensor==ID_S_LIGHT_RELAY){
        bool newLight = message.getBool();
        if (newLight){
          relay_off();
        } else {
          relay_on();
        }
      }
      break;
  }
}

void getButton(){
  auto data = IRLremote.read();
  IRCommand = data.command;
  previousOffMillis = currentMillis; // reset shutdown timer on button press.
  if(IRCommand == 0x00){
    slowdown = 3;
    IRCommand = LastIRCommand;
  } else {
    LastIRCommand = IRCommand;
  }
  if ( IRCommand == remote.bright ||
       IRCommand == remote.dim ||
       IRCommand == remote.red_up  ||
       IRCommand == remote.red_down  ||
       IRCommand == remote.green_up  ||
       IRCommand == remote.green_down  ||
       IRCommand == remote.blue_up  ||
       IRCommand == remote.blue_down  ||
       IRCommand == remote.quick  ||
       IRCommand == remote.slow ){
    true; // allow fast button repeat.  All other buttons are debounced.
  } else if ( currentMillis - previousDebounce > DEBOUNCE ){ 
    previousDebounce = currentMillis;  // this function is allowed to run.
  } else {
    return; // debounced out of this function
  }

  
  if(IRCommand == remote.bright){
    if ( effect == AURORA ){
      scale = qadd8(scale, 2);
    } else if ( slide == SLIDE_RGB ){
      slowdown = 3;
      if (brightness <= BRIGHTNESS){
        brightness = qadd8(brightness, 5);
      }
      FastLED.setBrightness(brightness);
      effect = 0;
      FastLED.show();
    } else {
      slowdown = 3;
      if ( brightness <= 250 ){
        brightness += 5;
      }
      if ( effect == 0 ){
        effect = BRIGHT;
      }
    }
  }
  if(IRCommand == remote.dim){
    //Serial.println("dim");
    if ( effect == AURORA ){
      scale = qsub8(scale, 2);
      slowdown = 2;
    } else if ( slide == SLIDE_RGB ){
      slowdown = 2;
      brightness = qsub8(brightness, 5);
      FastLED.setBrightness(brightness);
      FastLED.show();
    } else {
      slowdown = 2;
      if ( brightness >= 10 ) {
        brightness -= 5;
      } else if ( brightness >= 1 ){
        brightness -= 1;
      }
      if ( effect == 0 ){
        effect = DIM;
      }
    }
  }
  if(IRCommand == remote.pause){
      if ( pause == 0 ){
        pause = effect;
        effect = 0;
      } else {
        effect = pause;
        pause = 0;
      }
  }
  if(IRCommand == remote.power){
      led_off();
      relay_off();
  }
  if(IRCommand == remote.red1){
      setColor(0, 255, brightness);
  }
  if(IRCommand == remote.green1){
      setColor(96, 255, brightness);
  }
  if(IRCommand == remote.blue1){
      setColor(160, 255, brightness);
  }
  if(IRCommand == remote.white1){
      setColor(0, 0, brightness);
  }
  if(IRCommand == remote.red2){
      setColor(16, 255, brightness);
  }
  if(IRCommand == remote.green2){
      setColor(112, 255, brightness);
  }
  if(IRCommand == remote.blue2){
      setColor(176, 255, brightness);
  }
  if(IRCommand == remote.white2){
      setColor(16, 50, brightness);
  }
  if(IRCommand == remote.red3){
      setColor(32, 255, brightness);
  }
  if(IRCommand == remote.green3){
      setColor(120, 255, brightness);
  }
  if(IRCommand == remote.blue3){
      setColor(192, 255, brightness);
  }
  if(IRCommand == remote.white3){
      setColor(216, 80, brightness);
  }
  if(IRCommand == remote.red4){
      setColor(48, 255, brightness);
  }
  if(IRCommand == remote.green4){
      setColor(128, 255, brightness);
  }
  if(IRCommand == remote.blue4){
      setColor(208, 255, brightness);
  }
  if(IRCommand == remote.white4){
      setColor(96, 70, brightness);
  }
  if(IRCommand == remote.red5){
      setColor(64, 255, brightness);
  }
  if(IRCommand == remote.green5){
      setColor(136, 255, brightness);
  }
  if(IRCommand == remote.blue5){
      setColor(224, 255, brightness);
  }
  if(IRCommand == remote.white5){
      setColor(64, 60, brightness);
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
      slowdown = 2;
    if (effect == AURORA){
      spd = qadd8(spd, 1);
    } else {
      if ( intrvl >= 101 ){
        intrvl -= 100;
      } else if ( intrvl >= MIN_intrvl + 10 && intrvl <= 100 ){
        intrvl -= 10;
      } else {
        intrvl = MIN_intrvl;
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
      slowdown = 2;
    if (effect == AURORA){
      spd = qsub8(spd, 1);
    } else {
      //Serial.println("slow");
      if (intrvl >= 100){
        intrvl += 100;
      } else {
        intrvl += 15;
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
    intrvl = 100;
    effect = AUTOM;
    x=0;
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
    flash();
  }
  if(IRCommand == remote.jump3){
      //Serial.println("jump 3");
      stage = 1;
      intrvl = 1200;
      h=0; s=255; v=0;
      effect = JUMP3;
      slide = SLIDE_VALUE;
      sendRGB();
  }
  if(IRCommand == remote.jump7){
      //Serial.println("jump 7");
      stage = 1;
      intrvl = 1700;
      h=0; s=255; v=0;
      effect = JUMP7;
      slide = SLIDE_VALUE;
      sendRGB();
  }
  if(IRCommand == remote.fade3){
      //Serial.println("fade 3");
      stage = 1;
      intrvl = MIN_intrvl;
      h=0; s=255; v=0;
      effect = FADE3;
      slide = SLIDE_VALUE;
      sendRGB();
  }
  if(IRCommand == remote.fade7){
      led_light = 0;
      slowdown = 10;
      // multiple clicks chooses next effect.
      intrvl = 65;
      if (effect == FADE7){
        effect = FADE7B; 
      } else if (effect == FADE7B) {
        effect = AURORA;
        intrvl = 55;
        spd = 2;
        scale = 10;
      } else {
        effect = FADE7;
      }
      stage = 1;
      slide = SLIDE_VALUE;
      h=0; s=255; v=brightness;
      sendRGB();
  }
}

void update_effect(){
  if ( effect == FADE7 ){
    fade7();
  } else if ( effect == AURORA ){
    aurora();
  } else if ( effect == RAIN ){
    //rain();
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
  } else if ( effect == WIPE ){
    colorWipe();
  } else if ( effect == ADJUST_BRIGHTNESS ){
    adjBrightness();
  } else if ( effect == AUTOM ){
    autom();
  }
}

void led_off(){
  if (led_light == 1){
    for ( i = 0; i < NUM_LEDS; i++ ){
      led[i] = RGB(0,0,0);
      FastLED.show();
      wait(50);
    }
  }
  sprintf(hex,"%02X%02X%02X",0,0,0);
  prepMsg(ID_S_RGB_LIGHT,V_RGB);
  send(msg_ALL.set(hex));
  wait(LONG_WAIT);
  send(msg_ALL.set("0"));
  wait(SHORT_WAIT);
  led_light = 0;
  intrvl = 500;
  effect = NO_EFFECT;
  FastLED.clear();
  FastLED.show();
}

void relay_off(){
  set_relay(0);
}

void relay_on(){
  set_relay(1);
}

void set_relay(bool new_state){
  digitalWrite(PIN_RELAY, !new_state);
  big_light=new_state;
  prepMsg(ID_S_LIGHT_RELAY,V_STATUS);
  send(msg_ALL.set(new_state == 1 ? "1" : "0" ));
  wait(SHORT_WAIT);
}

void setColor(uint8_t hue, uint8_t sat, uint8_t value){
  //"red 1", 0, 255, brightness);
  //effect = NO_EFFECT;
  c1=leds[1];
  led_light=1;
  h = hue; s = sat; v = value;
  if ( hue == 0 && sat == 0 && value != 0 ){
    const CRGB whites[] = {
      Candle,                  HighPressureSodium, Tungsten40W,           Tungsten100W,
      SodiumVapor,             Halogen,            WarmFluorescent,       GrowLightFluorescent, 
      FullSpectrumFluorescent, CarbonArc,          HighNoonSun,           UncorrectedTemperature,
      StandardFluorescent,     MetalHalide,        MercuryVapor,          CoolWhiteFluorescent,
      OvercastSky,             ClearBlueSky,       BlackLightFluorescent
    };    
    c2=whites[last_white];
    r = c2.r;
    g = c2.g;
    b = c2.b;
    if (last_white++ >= 18){
      last_white = 0;
    }
  } else {
    hsv2rgb_rainbow( CHSV(h,s,v), c2);
  }
  sendRGB();
  effect = WIPE;
  brightness2 = brightness;
  stage = 1;
  r=c2.r;
  g=c2.g;
  b=c2.b;
}

void sendRGB () {
  led_light = 1;
  prepMsg(ID_S_RGB_LIGHT,V_STATUS);
  char hex[7] = {0};
  sprintf(hex,"%02X%02X%02X",leds[0].r,leds[0].g,leds[0].b);
  prepMsg(ID_S_RGB_LIGHT,V_RGB);
  send(msg_ALL.set(hex));
  wait(LONG_WAIT);
  send(msg_ALL.set("1"));
  wait(SHORT_WAIT);
}

void colorUpDown(int color, int8_t val){
  effect = 0;
  r = leds[1].r; g = leds[1].g; b = leds[1].b;
  uint8_t m[] = { r, g, b };
  if ( m[color] + val >= 1 && m[color] + val <= 250 ){
    if ( color == RED   ) { r += val; }
    if ( color == GREEN ) { g += val; }
    if ( color == BLUE  ) { b += val; }
    lastdiy.dirty = true;
    fill_solid(leds, NUM_LEDS, CRGB( r, g, b ));
  }
  slide = SLIDE_RGB;
  FastLED.show();
}
      

void updateDiy(uint8_t num){
  effect = 0;
  c1=leds[1];
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
  }
  effect = WIPE;
  stage = 1;
  slide = SLIDE_RGB;
  sendRGB();
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
  for ( i = 0; i < NUM_LEDS; i++ ){
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
  for ( i = 0; i < NUM_LEDS; i++ ){
    leds[i] = CHSV(h+(i*3), s, brightness);
  } 
  h++;
}

void jump3(){
  s=255;
  v=brightness;
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
  // This is controlling a light on a relay
  big_light = !big_light;
  digitalWrite(PIN_RELAY, !big_light);
  prepMsg(ID_S_LIGHT_RELAY,V_STATUS);
  send(msg_ALL.set(big_light==1?"1":"0"));
  wait(SHORT_WAIT);
}

void autom(){
  y = x;
  for ( i = 0; i < NUM_LEDS; i++ ){
    x+=2; 
    leds[i] = blend(c1,c2,triwave8(x));
  }
  x = y + 2;
}

void aurora(){
  for ( i = 0; i < NUM_LEDS; i++ ){
    v = inoise8(i*3*scale, z);
    h = inoise8(i*scale, z);
    h = qsub8(h,16);
    h = qadd8(h,scale8(h,39));
    leds[i] = CHSV(h,255,v);
  }
  z += spd; // this is speed.  
}

void adjBrightness(){
  if (stage == 1) {
    intrvl = 10;
    stage = 2;
  } else if ( stage == 2) {
    if ( brightness < brightness2 ){
      brightness = qadd8(brightness, 2);
      if (brightness >= brightness2){
        effect = 0;
        intrvl = MIN_intrvl;
      }
    } else {
      brightness = qsub8(brightness, 2);
      if (brightness <= brightness2){
        effect = 0;
        intrvl = MIN_intrvl;
      }
    }
    FastLED.setBrightness(brightness);
  } else {
    effect = 0;
  }
}

void colorWipe() {
  if ( stage == 1 ){
    gBright = 0;
    intrvl = 3;
    dir = random8(1,10);
    c1 = leds[1];
    //fill_solid(leds, NUM_LEDS, c1);
    c2 = CRGB(r,g,b);
    c2.nscale8_video(brightness2);
    i = 0;
    stage = 2;
  } else if ( stage == 2 ){
    if (gBright < 255){
      // fade in one led at a time
      if ( i < NUM_LEDS) {
        trans = blend(c1,c2,gBright);
        if (dir >= 5){
          leds[i] = trans;
        } else {
          leds[NUM_LEDS - 1 - i] = trans;
        }
      }
      gBright = qadd8(gBright, 40);
    } else {
      // set led to last color.
      if ( i < NUM_LEDS ) {
        if (dir >= 5){
          leds[i] = c2;
        } else {
          leds[NUM_LEDS - 1 - i] = c2;
        }
        i++;  //next led.
        //c1 = leds[i];
      } else {
        // end animation.
        intrvl = MIN_intrvl;
        //effect = next_effect;
        effect = 0;
        stage = 1;
        fill_solid(leds, NUM_LEDS, c2);
      }
      gBright = 0;
    }
  }
}

// wrapper to re_use MyMessage class.
void prepMsg(uint8_t sensor_id, uint8_t sensor_type){
  msg_ALL.clear();
  msg_ALL.setType(sensor_type);
  msg_ALL.setSensor(sensor_id);
}

