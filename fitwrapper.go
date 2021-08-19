// Compile with
// go build -buildmode=c-archive fitwrapper.go

package main

import (
 "C"
 "fmt"
 "unsafe"
 "os"
 "io/ioutil"
 "github.com/tormoder/fit"
 "bytes"
)

/* Create a "Go slice backed by a C array" for floats
 * ref: https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
 */
func malloc_float_slice(size int) (p unsafe.Pointer, float_slice []C.float ) {
  p = C.malloc(C.size_t(size) * C.size_t(unsafe.Sizeof(C.float(0))))
  float_slice = (*[1<<30 - 1]C.float)(p)[:size:size]
  return p, float_slice
}

/* Create a "Go slice backed by a C array" for floats
 * ref: https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
 */
func malloc_long_slice(size int) (p unsafe.Pointer, long_slice []C.long) {
  p= C.malloc(C.size_t(size) * C.size_t(unsafe.Sizeof(C.long(0))))
  long_slice = (*[1<<30 - 1]C.long)(p)[:size:size]
  return p, long_slice
}

/* Find the structure containing sensor data and 
 * events from active sessions.
 */
func open_fit_file(fit_filename string) (af *fit.ActivityFile) {
  infile, err := os.Open(fit_filename) // For read access.
	if err != nil {
		panic("Can't open input file. Aborting.")
	}
  /* fBytes is an in-memory array of bytes read from the file. */
	fBytes, err := ioutil.ReadAll(infile)
	if err != nil {
		panic("Can't read input file. Aborting.")
	}
	// Decode the FIT file data
	fit, err := fit.Decode(bytes.NewReader(fBytes))
	if err != nil {
		fmt.Println(err)
		return
	}
	af, err = fit.Activity()
	if err != nil {
		panic("Not an activity fit file. Aborting.")
	}
  return af
}

func make_arrays(af *fit.ActivityFile, recSize int, lapSize int) (
                                    pRecTimestamp unsafe.Pointer,
                                    pRecDistance unsafe.Pointer,
                                    pRecSpeed unsafe.Pointer,
                                    pRecAltitude unsafe.Pointer,
                                    pRecCadence unsafe.Pointer,
                                    pRecHeartRate unsafe.Pointer,
                                    pRecLat unsafe.Pointer,
                                    pRecLong unsafe.Pointer,
                                    nRecs int,
                                    pLapTimestamp unsafe.Pointer,
                                    pLapTotalDistance unsafe.Pointer,
                                    pLapStartPositionLat unsafe.Pointer,
                                    pLapStartPositionLong unsafe.Pointer,
                                    pLapEndPositionLat unsafe.Pointer,
                                    pLapEndPositionLong unsafe.Pointer,
                                    pLapTotalCalories unsafe.Pointer,
                                    pLapTotalElapsedTime unsafe.Pointer,
                                    pLapTotalTimerTime unsafe.Pointer,
                                    nLaps int) {

  /* Allocate the *C.float array (on the GO side so that it doesn't
   * get garbage collected.
   */
  pRecTimestamp, RecTimestamps := malloc_long_slice(recSize)
  pRecDistance,  RecDistances  := malloc_float_slice(recSize)
  pRecSpeed,     RecSpeeds     := malloc_float_slice(recSize)
  pRecAltitude,  RecAltitudes  := malloc_float_slice(recSize)
  pRecCadence,   RecCadences   := malloc_float_slice(recSize)
  pRecHeartRate, RecHeartRates := malloc_float_slice(recSize)
  pRecLat,       RecLats       := malloc_float_slice(recSize)
  pRecLong,      RecLongs      := malloc_float_slice(recSize)

  pLapTimestamp,         LapTimestamps         := malloc_long_slice(lapSize)
  pLapTotalDistance,     LapTotalDistances     := malloc_float_slice(lapSize)
  pLapStartPositionLat,  LapStartPositionLats  := malloc_float_slice(lapSize)
  pLapStartPositionLong, LapStartPositionLongs := malloc_float_slice(lapSize)
  pLapEndPositionLat,    LapEndPositionLats    := malloc_float_slice(lapSize)
  pLapEndPositionLong,   LapEndPositionLongs   := malloc_float_slice(lapSize)
  pLapTotalCalories,     LapTotalCaloriess     := malloc_float_slice(lapSize)
  pLapTotalElapsedTime,  LapTotalElapsedTimes  := malloc_float_slice(lapSize)
  pLapTotalTimerTime,    LapTotalTimerTimes    := malloc_float_slice(lapSize)

  nRecs = 0
  for idx, item := range af.Records {
    RecTimestamps[idx] = C.long(item.Timestamp.Unix())
    RecDistances[idx] =  C.float(item.GetDistanceScaled())
    RecSpeeds[idx] =  C.float(item.GetSpeedScaled())
    RecAltitudes[idx] =  C.float(item.GetAltitudeScaled())
    RecCadences[idx] =  C.float(item.Cadence)
    RecHeartRates[idx] =  C.float(item.HeartRate)
    RecLats[idx] =  C.float(item.PositionLat.Degrees())
    RecLongs[idx] =  C.float(item.PositionLong.Degrees())
    nRecs = nRecs + 1
  }
  nLaps = 0
  for idx, item := range af.Laps {
    LapTimestamps[idx] =  C.long(item.Timestamp.Unix())
    LapTotalDistances[idx] =  C.float(item.GetTotalDistanceScaled())
    LapStartPositionLats[idx] =  C.float(item.StartPositionLat.Degrees())
    LapStartPositionLongs[idx] =  C.float(item.StartPositionLong.Degrees())
    LapEndPositionLats[idx] =  C.float(item.EndPositionLat.Degrees())
    LapEndPositionLongs[idx] =  C.float(item.EndPositionLong.Degrees())
    LapTotalCaloriess[idx] =  C.float(item.TotalCalories)
    LapTotalElapsedTimes[idx] =  C.float(item.TotalElapsedTime)
    LapTotalTimerTimes[idx] =  C.float(item.TotalTimerTime)
    nLaps = nLaps + 1
  }
  return  pRecTimestamp,
          pRecDistance,
          pRecSpeed,
          pRecAltitude,
          pRecCadence,
          pRecHeartRate,
          pRecLat,
          pRecLong,
          nRecs,
          pLapTimestamp,
          pLapTotalDistance,
          pLapStartPositionLat,
          pLapStartPositionLong,
          pLapEndPositionLat,
          pLapEndPositionLong,
          pLapTotalCalories,
          pLapTotalElapsedTime,
          pLapTotalTimerTime,
          nLaps

}

/* Export the function to C via CGO with // notation. */

//export parse_fit_file
func parse_fit_file(fname *C.char, recSize int, lapSize int) (
         C.size_t, *C.long,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.long,
         C.size_t, *C.long,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.size_t, *C.float,
         C.long,
         C.long,
         C.long,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.float,
         C.long) {
  /* Open an activity file. */
  filename := C.GoString(fname)
  af := open_fit_file(filename)
  /* Convert the records to arrays (for items that are time based). */
  pRecTimestamp,
  pRecDistance,
  pRecSpeed,
  pRecAltitude,
  pRecCadence,
  pRecHeartRate,
  pRecLat,
  pRecLong,
  nRecs,
  pLapTimestamp,
  pLapTotalDistance,
  pLapStartPositionLat,
  pLapStartPositionLong,
  pLapEndPositionLat,
  pLapEndPositionLong,
  pLapTotalCalories,
  pLapTotalElapsedTime,
  pLapTotalTimerTime,
  nLaps :=  make_arrays(af, recSize, lapSize)
  /* Find the local time zone value. */
  //tOffset := C.long(af.Activity.LocalTimestamp.Unix()) - C.long(af.Activity.Timestamp.Unix())
  //fmt.Println(af.Activity.LocalTimestamp);
  //fmt.Println(af.Activity.Timestamp);

  return C.size_t(recSize), (*C.long)(pRecTimestamp),
         C.size_t(recSize), (*C.float)(pRecDistance),
         C.size_t(recSize), (*C.float)(pRecSpeed),
         C.size_t(recSize), (*C.float)(pRecAltitude),
         C.size_t(recSize), (*C.float)(pRecCadence),
         C.size_t(recSize), (*C.float)(pRecHeartRate),
         C.size_t(recSize), (*C.float)(pRecLat),
         C.size_t(recSize), (*C.float)(pRecLong),
         C.long(nRecs),
         C.size_t(lapSize), (*C.long)(pLapTimestamp),
         C.size_t(lapSize), (*C.float)(pLapTotalDistance),
         C.size_t(lapSize), (*C.float)(pLapStartPositionLat),
         C.size_t(lapSize), (*C.float)(pLapStartPositionLong),
         C.size_t(lapSize), (*C.float)(pLapEndPositionLat),
         C.size_t(lapSize), (*C.float)(pLapEndPositionLong),
         C.size_t(lapSize), (*C.float)(pLapTotalCalories),
         C.size_t(lapSize), (*C.float)(pLapTotalElapsedTime),
         C.size_t(lapSize), (*C.float)(pLapTotalTimerTime),
         C.long(nLaps),
         C.long(af.Sessions[0].Timestamp.Unix()),
         C.long(af.Sessions[0].StartTime.Unix()),
         C.float(af.Sessions[0].StartPositionLat.Degrees()),
         C.float(af.Sessions[0].StartPositionLong.Degrees()),
         C.float(af.Sessions[0].TotalElapsedTime),
         C.float(af.Sessions[0].TotalTimerTime),
         C.float(af.Sessions[0].GetTotalDistanceScaled()),
         C.float(af.Sessions[0].NecLat.Degrees()),
         C.float(af.Sessions[0].NecLong.Degrees()),
         C.float(af.Sessions[0].SwcLat.Degrees()),
         C.float(af.Sessions[0].SwcLong.Degrees()),
         C.float(af.Sessions[0].TotalWork),
         C.float(af.Sessions[0].TotalMovingTime),
         C.float(af.Sessions[0].AvgLapTime),
         C.float(af.Sessions[0].TotalCalories),
         C.float(af.Sessions[0].AvgSpeed),
         C.float(af.Sessions[0].MaxSpeed),
         C.float(af.Sessions[0].TotalAscent),
         C.float(af.Sessions[0].TotalDescent),
         C.float(af.Sessions[0].AvgAltitude),
         C.float(af.Sessions[0].MaxAltitude),
         C.float(af.Sessions[0].MinAltitude),
         C.float(af.Sessions[0].AvgHeartRate),
         C.float(af.Sessions[0].MaxHeartRate),
         C.float(af.Sessions[0].MinHeartRate),
         C.float(af.Sessions[0].AvgCadence),
         C.float(af.Sessions[0].MaxCadence),
         C.float(af.Sessions[0].AvgTemperature),
         C.float(af.Sessions[0].MaxTemperature),
         C.float(af.Sessions[0].TotalAnaerobicTrainingEffect),
         C.long(af.Activity.LocalTimestamp.Unix())
}

/* Dummy function (required for cgo) */
func main() {}

