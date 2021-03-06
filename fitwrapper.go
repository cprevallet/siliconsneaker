/*
 * This program provides a GTK graphical user interface to PLPlot graphical
 * plotting routines in order to plot running files stored in the Garmin
 * (Dynastream) FIT format.
 *
 * License:
 *
 * Permission is granted to copy, use, and distribute for any commercial
 * or noncommercial purpose in accordance with the requirements of
 * version 2.0 of the GNU General Public license.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * On Debian systems, the complete text of the GNU General
 * Public License can be found in `/usr/share/common-licenses/GPL-2'.
 *
 * - Craig S. Prevallet, July, 2020
 */


// Compile with
// go build -buildmode=c-archive fitwrapper.go

package main

import (
	"C"
	"bytes"
	//"fmt"
	"github.com/tormoder/fit"
  "math"
	"io/ioutil"
	"os"
	"unsafe"
)

/* Create a "Go slice backed by a C array" for floats
 * ref: https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
 */
func malloc_float_slice(size int) (p unsafe.Pointer, float_slice []C.float) {
	p = C.malloc(C.size_t(size) * C.size_t(unsafe.Sizeof(C.float(0))))
	float_slice = (*[1<<30 - 1]C.float)(p)[:size:size]
	return p, float_slice
}

/* Create a "Go slice backed by a C array" for floats
 * ref: https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
 */
func malloc_long_slice(size int) (p unsafe.Pointer, long_slice []C.long) {
	p = C.malloc(C.size_t(size) * C.size_t(unsafe.Sizeof(C.long(0))))
	long_slice = (*[1<<30 - 1]C.long)(p)[:size:size]
	return p, long_slice
}

/* Find the structure containing sensor data and
 * events from active sessions.
 */
func open_fit_file(fit_filename string) (af *fit.ActivityFile) {
	infile, err := os.Open(fit_filename) // For read access.
	if err != nil {
    return nil
	}
	/* fBytes is an in-memory array of bytes read from the file. */
	fBytes, err := ioutil.ReadAll(infile)
	if err != nil {
    return nil
	}
	// Decode the FIT file data
	fit, err := fit.Decode(bytes.NewReader(fBytes))
	if err != nil {
		return nil
	}
	af, err = fit.Activity()
	if err != nil {
		return nil
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
	pRecDistance, RecDistances := malloc_float_slice(recSize)
	pRecSpeed, RecSpeeds := malloc_float_slice(recSize)
	pRecAltitude, RecAltitudes := malloc_float_slice(recSize)
	pRecCadence, RecCadences := malloc_float_slice(recSize)
	pRecHeartRate, RecHeartRates := malloc_float_slice(recSize)
	pRecLat, RecLats := malloc_float_slice(recSize)
	pRecLong, RecLongs := malloc_float_slice(recSize)

	pLapTimestamp, LapTimestamps := malloc_long_slice(lapSize)
	pLapTotalDistance, LapTotalDistances := malloc_float_slice(lapSize)
	pLapStartPositionLat, LapStartPositionLats := malloc_float_slice(lapSize)
	pLapStartPositionLong, LapStartPositionLongs := malloc_float_slice(lapSize)
	pLapEndPositionLat, LapEndPositionLats := malloc_float_slice(lapSize)
	pLapEndPositionLong, LapEndPositionLongs := malloc_float_slice(lapSize)
	pLapTotalCalories, LapTotalCaloriess := malloc_float_slice(lapSize)
	pLapTotalElapsedTime, LapTotalElapsedTimes := malloc_float_slice(lapSize)
	pLapTotalTimerTime, LapTotalTimerTimes := malloc_float_slice(lapSize)

	nRecs = 0
	for idx, item := range af.Records {
		RecTimestamps[idx] = C.long(item.Timestamp.Unix())
		RecDistances[idx] = C.float(item.GetDistanceScaled())
		RecSpeeds[idx] = C.float(item.GetSpeedScaled())
		RecAltitudes[idx] = C.float(item.GetAltitudeScaled())
		RecCadences[idx] = C.float(item.Cadence)
		RecHeartRates[idx] = C.float(item.HeartRate)
		RecLats[idx] = C.float(item.PositionLat.Degrees())
		RecLongs[idx] = C.float(item.PositionLong.Degrees())
		nRecs = nRecs + 1
	}
	nLaps = 0
	for idx, item := range af.Laps {
		LapTimestamps[idx] = C.long(item.Timestamp.Unix())
		LapTotalDistances[idx] = C.float(item.GetTotalDistanceScaled())
		LapStartPositionLats[idx] = C.float(item.StartPositionLat.Degrees())
		LapStartPositionLongs[idx] = C.float(item.StartPositionLong.Degrees())
		LapEndPositionLats[idx] = C.float(item.EndPositionLat.Degrees())
		LapEndPositionLongs[idx] = C.float(item.EndPositionLong.Degrees())
		LapTotalCaloriess[idx] = C.float(item.TotalCalories)
		LapTotalElapsedTimes[idx] = C.float(item.GetTotalElapsedTimeScaled())
		LapTotalTimerTimes[idx] = C.float(item.GetTotalTimerTimeScaled())
		nLaps = nLaps + 1
	}
	return pRecTimestamp,
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

/* Return a floating point representation or NaN */
func num_cvt(sessionval interface{}) (C.float) {
  switch sessionval.(type) {
    case string:
      return C.float(math.NaN())
    case float32:
      return C.float(sessionval.(float32))
    case float64:
      return C.float(sessionval.(float64))
    case uint8:
      if (sessionval.(uint8) < math.MaxUint8) {
        return C.float(sessionval.(uint8))
      } else {
        break;
      }
    case uint16:
      if (sessionval.(uint16) < math.MaxUint16) {
        return C.float(sessionval.(uint16))
      } else {
        break;
      }
    case uint32:
      if (sessionval.(uint32) < math.MaxUint32) {
        return C.float(sessionval.(uint32))
      } else {
        break;
      }
    /*
    case uint64:
      if (sessionval != math.MaxUint64) {
        return C.float(sessionval.(uint64))
      } else {
        break;
      }
    */
    case int8:
      if (sessionval.(int8) < math.MaxInt8) {
        return C.float(sessionval.(int8))
      } else {
        break;
      }
    case int16:
      if (sessionval.(int16) < math.MaxInt16) {
        return C.float(sessionval.(int16))
      } else {
        break;
      }
    case int32:
      if (sessionval.(int32) < math.MaxInt32) {
        return C.float(sessionval.(int32))
      } else {
        break;
      }
    case int64:
      if (sessionval.(int64) < math.MaxInt64) {
        return C.float(sessionval.(int64))
      } else {
        break;
      }
  }
  return C.float(math.NaN())
}

/* Export the function to C via CGO with // notation. */

//export parse_fit_file
func parse_fit_file(fname *C.char, recSize int, lapSize int) (
  C.long,
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
  if af != nil {
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
      nLaps := make_arrays(af, recSize, lapSize)
      /* Find the local time zone offset from UTC. */
      _, tzOffset := af.Activity.LocalTimestamp.Zone()
      /* Successful read, return values and error = 0. */
      return 0,  //success!!
      C.size_t(recSize), (*C.long)(pRecTimestamp),
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
      num_cvt(af.Sessions[0].StartPositionLat.Degrees()),
      num_cvt(af.Sessions[0].StartPositionLong.Degrees()),
      num_cvt(af.Sessions[0].GetTotalElapsedTimeScaled()),
      num_cvt(af.Sessions[0].GetTotalTimerTimeScaled()),
      num_cvt(af.Sessions[0].GetTotalDistanceScaled()),
      num_cvt(af.Sessions[0].NecLat.Degrees()),
      num_cvt(af.Sessions[0].NecLong.Degrees()),
      num_cvt(af.Sessions[0].SwcLat.Degrees()),
      num_cvt(af.Sessions[0].SwcLong.Degrees()),
      num_cvt(af.Sessions[0].TotalWork),
      num_cvt(af.Sessions[0].GetTotalMovingTimeScaled()),
      num_cvt(af.Sessions[0].GetAvgLapTimeScaled()),
      num_cvt(af.Sessions[0].TotalCalories),
      num_cvt(af.Sessions[0].GetAvgSpeedScaled()),
      num_cvt(af.Sessions[0].GetMaxSpeedScaled()),
      num_cvt(af.Sessions[0].TotalAscent),
      num_cvt(af.Sessions[0].TotalDescent),
      num_cvt(af.Sessions[0].GetAvgAltitudeScaled()),
      num_cvt(af.Sessions[0].GetMaxAltitudeScaled()),
      num_cvt(af.Sessions[0].GetMinAltitudeScaled()),
      num_cvt(af.Sessions[0].AvgHeartRate),
      num_cvt(af.Sessions[0].MaxHeartRate),
      num_cvt(af.Sessions[0].MinHeartRate),
      num_cvt(af.Sessions[0].AvgCadence),
      num_cvt(af.Sessions[0].MaxCadence),
      num_cvt(af.Sessions[0].AvgTemperature),
      num_cvt(af.Sessions[0].MaxTemperature),
      num_cvt(af.Sessions[0].TotalAnaerobicTrainingEffect),
      C.long(tzOffset)

  } else {
      /* Failed read.  Return garbage values and error = 1. */
      return 1,  //failed!
      C.size_t(recSize), (*C.long)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.size_t(recSize), (*C.float)(nil),
      C.long(0),
      C.size_t(lapSize), (*C.long)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.size_t(lapSize), (*C.float)(nil),
      C.long(0),
      C.long(0),
      C.long(0),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.float(math.NaN()),
      C.long(0)
  }

}

/* Dummy function (required for cgo) */
func main() {}
