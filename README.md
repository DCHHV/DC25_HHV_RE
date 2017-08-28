# DC25 HHV Reverse Engineering Challenge

## Info
DC25 Hardware Hacking Village Reverse Engineering Challenge

This repo contains all of the sources, eagle files, and gerbers for the DEF CON 25 Hardware Hacking Village Reverse Engineering Challenge.

Do not go any further than this if you do not wish to see spoilers.  The eagle/ folder has all of the PCB files in it, the gerbers or .brd file can be sent off to a fab house to create a PCB if you wish to re-create the challenge yourself.  The .hex file in the top folder is the binary blob that can be programmed in to the microcontroller.

Writeup available in docs/writeup.txt

BOM available in eagle/DC25_HHV_RE-A-BOM.txt

## Instructions

Once you have a kit in-hand, (if you don't yet, go to the HHV and pick one up!) there are two steps to be aware of for this challenge.

The first challenge, is to reverse engineer the schematic of the device itself.  Grab a piece of paper, draw up the schematic, and take it to the HHV.  A correct schematic will earn you a small prize (while available).

The meat of the challenge is the function of the device.  It's locked, find the passcode to unlock it; when unlocked the green LED will remain solid.  There are multiple ways to accomplish this task.  If you are stuck or just not sure how to proceed, come on down to the HHV and ask for help.  Once the device has been unlocked, make your way back to the HHV again to collect a larger prize (while available).

This challenge is meant to be fun for all levels of experience, but is designed to be an introduction to reverse engineering and a demonstration in how insecure some devices really are.
