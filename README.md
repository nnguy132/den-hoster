##All code was pretty much pulled from progmem's Switch-Fightstick
https://github.com/progmem/Switch-Fightstick

##And inspiration from shinyquagsire23's Splatoon printer
https://github.com/shinyquagsire23/Switch-Fightstick

TESTED ON ARDUINO UNO R3

##Install instructions:
Pretty much the same as shinyquagsire23's install instructions

##Setup:
1) On line 94 in Joystick.c change the frame of which your shiny is at (assuming you are at frame 1)

2) Run make file

3) Flash the hex file to the teensy/arduino

4) Setup the VS's glitch as seen in the video below
https://youtu.be/UKzyL2bPEnI

5) Open up the date and time in options > systems

6) Open the date and time and set time to the 1st of Jan

7) Close then reopen the date

8) Hover over the OK button

9) Plug in the Arduino/teensy

## The switch will stop 4 frames out from the desired shiny to allow to reset for the correct species
