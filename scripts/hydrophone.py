#!/usr/bin/python2.7

import struct
import numpy as np
from scipy import signal
from subprocess import check_output
import traceback 

# Current sampling frequency of ADC
fs = 208333.333

# Configured pinger frequency
# TODO make settable over UART interface
freq = 40000.0

# Matched Filter
# TODO needs to be updated when pinger freq is updated
duration = 0.002 # Length of tone 2ms
mfilt = np.sin(2*np.pi*np.arange(fs*duration)*freq/fs)

# UART0 to output Direction of Arrival information on
file = open("/dev/ttyS0", "w")

# filters blocks of samples
def filt(data):
  return signal.fftconvolve(data, mfilt, mode='same') # apply matched filter to signal

# reads a file and estimates angle of arrival of pinger signal from it
def calculate_aoa(file_name):

  # reads 2 channel interleaved int16 little endian data from file
  data = np.fromfile(file_name, dtype=np.int16).reshape(-1, 2)

	# Knock off begining of file which containes errors as (ADC Chip) filters initalize
  chan_a = data[:, 0].astype(np.float32)[100:]
  chan_b = data[:, 1].astype(np.float32)[100:]

  # only filter one channel we will filter the second with pulses of the other channels
  chan_a_flt = filt(chan_a)

  try:

    # Find the peak of the filtered channel a signal where we think we heard a pinger
    peak = np.argmax(chan_a_flt)

    # magic threshold number, if its above 100000 its good enough
    thresh = 100000
    peak_a = chan_a_flt[peak]
    if peak_a < thresh:
      return

    # Do a hilbert transform to get the evelope and angle of the signal around the pulse for both channels
    chan_a_hil = signal.hilbert(chan_a_flt[peak-2500:peak+50])
    chan_a_pls = np.angle(chan_a_hil)
    chan_a_env = np.abs(chan_a_hil)
    chan_b_flt = filt(chan_b[peak-2500:peak+50])
    chan_b_hil = signal.hilbert(chan_b_flt)
    chan_b_pls = np.angle(chan_b_hil)

    # Confirm the signal is strong enough on channel B as well
    peak_b = chan_b_flt[np.argmax(chan_b_flt)]
    if peak_b < thresh:
      return

    # Calculate wavelength for Direction of Arrival Calculation
    speed_sound_water = 1484.0 # m/s
    lamb = speed_sound_water/freq

    # Find the first part of the signal to be below 1/5 (Magic Number) peak amplitude
    # moving from the peak backwards in time
    amax = 2500-np.argmax(np.flip(chan_a_env,0)<(peak_a/5))
    if(amax < 0 or amax >= 2500):
      return

    # Take the phase difference here
    dphase = chan_a_pls[amax]-chan_b_pls[amax]

    # Generaly if the phase difference is exactly 0 something went wrong
    if(dphase != 0.0):

      try:

        # Fix domain
        while(dphase > np.pi):
          dphase = dphase - 2.0*np.pi
        while(dphase < -np.pi):
          dphase = dphase + 2.0*np.pi

        # Check to make sure this is a phase difference that would be possible
        # given the element spacing
        max_pdif = 2.0*np.pi*0.018/lamb
        if np.abs(dphase) > max_pdif:
          print "Impossible phase difference"
          file.write("<Imp>\n")
          file.flush()


        else:

          # Calculate the angle of arrival and print it to UART0 so we know where to go
          theta = np.arcsin((lamb*dphase)/(2*np.pi*0.018)) # 0.018 m element spacing
          if not np.isnan(theta):
            print "AoA: ", np.rad2deg(theta)
            file.write("<AoA:%.2f>\n" % np.rad2deg(theta))
            file.flush()

      except:
        print "AoA error"
        traceback.print_exc()

  except:
    print "AoA error"
    traceback.print_exc()

# Loop forever while calculating Angle of Arrival of pinger signal
while True:
  try:
    count = count + 1
    check_output(["timeout", "10", "./get_sample.sh"]) # Get 2s of sample data from the ADC
    calculate_aoa("test.dat") # Calculate the AoA of the sample data
    # For debug we are saving off the file and putting this string in the
    # bag file so we can correlate the subs orientation and the AoA generated
    # from the signals for debug
    file.write("<File:%d>\n" % count)
    file.flush()
    check_output(["mv", "test.dat", "test%d.dat" % count])

  except:
    print "Fail"
    traceback.print_exc()

