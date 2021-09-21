![Logo](https://github.com/cprevallet/siliconsneaker/blob/main/icons/siliconsneaker.svg?raw=true)
# SiliconSneaker
Display graphs, a map, and summary information for runs generated by a Garmin-brand GPS watch.  
The program is GPL licensed, open-source, and cross-platform (Windows and multiple Linux distributions).  

![Screenshot](https://github.com/cprevallet/siliconsneaker/blob/main/screenshot/siliconsneaker.png?raw=true)
![Screenshot](https://github.com/cprevallet/siliconsneaker/blob/main/screenshot/siliconsneaker_windows.png?raw=true)

# Why?
![AdvocacyVideo](https://youtu.be/JY-fb4hMTX8)

# Installing
Download compiled binaries here:
https://github.com/cprevallet/siliconsneaker/releases/tag/v1.0

## Linux
Download the AppImage file from the website.
```
sudo chmod +x SiliconSneaker-1.0-x86_64.AppImage
```

## Windows
Download the .exe file from the website.
Run the file. A wizard will then guide you through the rest of the installation.

## Debian/Ubuntu Linux
Download the .deb file from the website
```
sudo apt install -f /path/to/package/siliconsneaker_1.0_amd64.deb
```
-f switch will install required dependencies.

# Running
A Garmin running watch must first be plugged directly into a computer USB port with the appropriate charging/data connector.  
Fenix: https://buy.garmin.com/en-US/US/p/107531#overview  
Forerunner: https://buy.garmin.com/en-US/US/p/620328#devices  
Once plugged in, the Garmin watch should mount itself as a USB device and present a file system to the host PC.  Individual activities (runs) may be viewed when FIT files are opened from within the program.

## Linux
Start from the command line or from your desktop.
```
siliconsneaker
```
There are a few command line options available as well.
```
siliconsneaker -h
Usage: ./siliconsneaker [OPTION]...[FILENAME]
 -f  open filename
 -m  use metric units
 -h  print program help
 -v  print program version
```

## Windows
Click on SiliconSneaker in the start menu.  Menu items are provided for both the English or Metric unit systems.

## Features
- Pace, cadence, heartrate, altitude, and split graphs are provided. Values provided are watch dependent.
- The map provides a GPS generated path and a heat map based on deviation from average speed.
- The graphs support the ability to zoom and pan the trends.
- The ability to switch unit systems is provided.
- In progress values are provided by a slider widget which will be reflected in the graph and on the map.
- Text values for measurements are provided near the slider.
- Overall summary values are provided. Values are watch dependent.
- Extensive tooltips are also provided.

# Building from source on Debian Linux
## Install build-time dependencies
```
apt install build-essential debhelper libc6-dev libgtk-3-dev libglib2.0-dev librsvg2-dev libcairo2-dev libplplot-dev libosmgpsmap-1.0-dev golang-1.15-go  
export GOROOT=/usr/lib/go-1.15/
export PATH=$PATH:$GOROOT/bin
```

## Get source files
```
git clone https://github.com/cprevallet/siliconsneaker  
```

## Build executables
```
make clean  
make  
```

## Install run-time dependencies and application
```
apt install libosmgpsmap-1.0-1 libgtk-3-0 libplplot17 librsvg2-2  
make install
```

# How to package for all platforms
Instructions on how to package may be found here:
![PackageInstructions](https://github.com/cprevallet/siliconsneaker/blob/main/deploy/package_instructions.txt?raw=true)
