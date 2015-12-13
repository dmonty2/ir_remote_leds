# 44key_remote_leds
44 Key Infrared Remote with Addressable LED lights controlled by an Arduino.

The 44 Key Infrared Remote is very common on ebay and ali-express however it is designed for non-addressable LEDs.  I wanted to use this remote to control addressable LEDs.  It has duplicated most of the standard behaviour of the original controller. The DIY buttons have the following logic:
* Press DIY#
* use up/down RGB arrows
* Press DIY# again to save to EEPROM

The arduino version has with the following additions:
* FADE7 will cycle between some addressable RGB patterns:  Solid change, Moving Rainbow, Northern Lights(Aurora Borealis).
* W button cycles between 19 different white values
* My 2nd white button under 'W'. is set to manage a relay module for a 110V light instead of showing another white.
* DIY up/down colours buttons have much finer control of levels.  The OEM chip jumps several values on each button press.
* Brightness works on the fade and jump.
* Holding the button down will 
* Flash is disabled - and yes, this is a much desired feature.
* Auto button is currently an attempt at a water/rain ripple effect - this is a work in progress.
* Power button turns off.  Any button turns it on again.  I'm lazy and don't want to have to press 2 buttons to get things going.  I also want to save some wear on the power button.

NOTE: 
In some addressable modes you will have to lightly touch buttons several times as the timing gets tight between listening for IR commands and rendering the animations.  This is why I used IRLremote.  The 44 key remote sends the code once then a separate code for 'repeat'  so if the arduino is not able to decode the initial send then it just starts repeating the last button pressed.

LEDs look best when they are  behind a diffuser or hidden under a lip.  The colour blend is much nicer when you do not see the led pixel but rather the light reflecting off a surface. 

This uses the following 2 libraries
* https://github.com/NicoHood/IRLremote
* https://github.com/FastLED/FastLED

TODO:
* Continue work on the rain/water effects.
* Put in an auto-off timer 5 hours or so.
* Leave flash button disabled!
* If 'repeat' command is detected after a delay then ignore the repeat and assume that the initial code was missed. Temporarily slow/pause the animation so the next button press will have a better chance of being captured.  'Press and hold once then, tap to execute new command'.
