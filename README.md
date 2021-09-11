# SiliconSneaker
Generate a graph and a map for runs taken from Garmin watch fit files.

# Running the program.
A Garmin running watch must first be plugged directly into a computer USB port with the appropriate charging/data connector.  
Fenix: https://buy.garmin.com/en-US/US/p/107531#overview  
Forerunner: https://buy.garmin.com/en-US/US/p/620328#devices  
Once plugged in, the Garmin watch should mount itself as a USB device and present a file system to the host PC.  From this file system a FIT file may be opened from the command line or from within the program.
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
