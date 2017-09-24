#!/usr/bin/python2.7

import struct
import numpy as np
from scipy import signal
from subprocess import check_output
import traceback

fs = 208333.333

freq = 40000.0

duration = 0.002
mfilt = np.sin(2*np.pi*np.arange(fs*duration)*freq/fs)

file = open("/dev/ttyS0", "w")

count = 0

def filt(data):
  return signal.fftconvolve(data, mfilt, mode='same') # apply matched filter to signal

def calculate_aoa(file_name):

  data = np.fromfile(file_name, dtype=np.int16).reshape(-1, 2)

	# Knock off begining of file which containes errors as filters initalize
  chan_a = data[:, 0].astype(np.float32)[100:]
  chan_b = data[:, 1].astype(np.float32)[100:]

  # only filter one channel we will filter the second with pulses of the other channels
  chan_a_flt = filt(chan_a)

  try:

    peak = np.argmax(chan_a_flt)

    thresh = 100000
    peak_a = chan_a_flt[peak]
    if peak_a < thresh:
      return

    chan_a_hil = signal.hilbert(chan_a_flt[peak-2500:peak+50])
    chan_a_pls = np.angle(chan_a_hil)
    chan_a_env = np.abs(chan_a_hil)
    chan_b_flt = filt(chan_b[peak-2500:peak+50])
    chan_b_hil = signal.hilbert(chan_b_flt)
    chan_b_pls = np.angle(chan_b_hil)

    peak_b = chan_b_flt[np.argmax(chan_b_flt)]
    if peak_b < thresh:
      return

    speed_sound_water = 1484.0 # m/s
    lamb = speed_sound_water/freq

    amax = np.argmax(chan_a_env > 50000)

    dphase = chan_a_pls[amax]-chan_b_pls[amax]
    #print dphase

    if(dphase != 0.0):

      try:

        # Fix domain
        while(dphase > np.pi):
          dphase = dphase - 2.0*np.pi
        while(dphase < -np.pi):
          dphase = dphase + 2.0*np.pi

        max_pdif = 2.0*np.pi*0.018/lamb
        if np.abs(dphase) > max_pdif:
          print "Impossible phase difference"
          file.write("<Imp>\n")
          file.flush()


        else:

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

while True:
  try:
    count = count + 1
    check_output(["timeout", "10", "./get_sample.sh"])
    calculate_aoa("test.dat")
    file.write("<File:%d>\n" % count)
    file.flush()
    check_output(["mv", "test.dat", "test%d.dat" % count])

  except:
    print "Fail"
    traceback.print_exc()

