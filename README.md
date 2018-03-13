# 44key_remote_leds
44 Key Infrared Remote with Addressable WS2812B LED lights controlled by an Arduino and MySensors.

The 44 Key Infrared Remote is very common on ebay and ali-express however it is designed for non-addressable LEDs.  I wanted to use this remote to control addressable LEDs.  It has duplicated most of the standard behaviour of the original controller. The DIY buttons have the following logic:
* Press DIY#
* use up/down RGB arrows
* Press DIY# again to save to EEPROM

The arduino version has with the following additions using 44 key infrared remote:
* FADE7 will cycle between some addressable RGB patterns:  Solid change, Moving Rainbow, Northern Lights(Aurora Borealis).
* W (white) button cycles between FastLED's 19 different white values: warm-to-cool
* Flash button manages a relay module for a 110V light.
* DIY up/down colours buttons have much finer control of levels.  The OEM chip jumps several values on each button press.
* Brightness works on solid color, fade and jump.
* Holding bright/dim fast/slow up/down buttons down works.  This saves wear on the buttons. 
* Auto: choose color1, choose color2, press 'AUTO' and the two colors will animate.
* Power button turns off.  Any button turns it on again.  I'm lazy and don't want to have to press 2 buttons to get things going.  I also want to save some wear on the power button.
* Lights will auto-power off after 4 hours.

MySensors:
* The controller sends 2 messages to set a color: V_RGB then V_PERCENTAGE.  The program waits for both messages then animates the transition. 
* I've only implemented support for the color picker/fader in Domoticz (+MyCroft domoticz skill for voice activation).  Other animations are only through the 44 key IR remote.

LEDs look best when they are behind a diffuser or hidden under a lip.  The colour blend is much nicer when you do not see the led pixel but rather the light reflecting off a surface. 

This uses the following 2 libraries
* https://github.com/NicoHood/IRLremote
* https://github.com/FastLED/FastLED
* https://github.com/mysensors/MySensors

The code is all non-blocking in order for both the IR Remote and MySensors to respond to commands.  Various effects and transitions are handled by:
* intervl - how fast to refresh/animate. ( interfaced with quick and slow buttons )
* effect - which effect function we are using.
* stage - within an effect there can be multiple stages.
