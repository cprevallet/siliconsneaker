# SiliconSneaker
Generate a graph and a map for runs taken from Garmin watch fit files.

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

# Running
```
siliconsneaker -h
Usage: ./siliconsneaker [OPTION]...[FILENAME]
 -f  open filename
 -m  use metric units
 -h  print program help
 -v  print program version
```

# Packaging for Windows
```
apt install nsis  
```
Read: https://www.gtk.org/docs/installations/windows  
```
nsis 'SiliconSneaker Windows x64 Setup.nsi'   
```
