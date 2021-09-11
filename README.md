![Logo](https://github.com/cprevallet/siliconsneaker/blob/main/icons/siliconsneaker.svg?raw=true)
# SiliconSneaker
Generate graphs and a map for runs taken from Garmin watch fit files.  
The program is GPL licensed, open-source, and cross-platform (Windows 10 and Debian Linux).  

![Screenshot](https://github.com/cprevallet/siliconsneaker/blob/main/screenshot/siliconsneaker.png?raw=true)

# Running the program.
A Garmin running watch must first be plugged directly into a computer USB port with the appropriate charging/data connector.  
Fenix: https://buy.garmin.com/en-US/US/p/107531#overview  
Forerunner: https://buy.garmin.com/en-US/US/p/620328#devices  
Once plugged in, the Garmin watch should mount itself as a USB device and present a file system to the host PC.  Individual activities (runs) may be viewed when FIT files are opened from within the program.
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

## Features
- Pace, cadence, heartrate, altitude, and split graphs are provided. Values provided are watch dependent.
- The map provides a GPS generated path and a heat map based on deviation from average speed.
- The graphs support the ability to zoom and pan the trends.
- The ability to switch unit systems is provided.
- In progress values are provided by a slider widget which will be reflected in the graph and on the map.
- Text values for measurements are provided near the slider.
- Overall summary values are provided. Values are watch dependent.
- Extensive tooltips are also provided.

# Building from source
## Install build time dependencies.
```
apt install build-essential libc6-dev libgtk-3-dev libglib2.0-dev librsvg2-dev libcairo2-dev libplplot-dev libosmgpsmap-1.0-dev golang-1.15-go  
```

## Get source files.
```
git clone https://github.com/cprevallet/siliconsneaker  
go get github.com/tormoder/fit@v0.10.0  
```

## Build executables.
```
make clean  
make  
```

## Install run-time dependencies
```
apt install libosmgpsmap-1.0-1 libgtk-3-0 libplplot17 librsvg2-2  
```

# Packaging for Windows
```
apt install nsis  
```
Read: https://www.gtk.org/docs/installations/windows  
```
nsis 'SiliconSneaker Windows x64 Setup.nsi'   
```
