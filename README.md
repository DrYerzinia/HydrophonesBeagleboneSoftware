# Architecture of Hydrophones Software

The program flow is roughly as follows:

hydrophone:(reset_pru,trigger_adc_record)->adc_record:(trigger_pru)->pru0adc:(configure_adc,loop_spi_read_adc)->adc_record:(write_2s_sample_data_to_file)->hydrophone:(read_sample_data_and_process)->repeat

All the C parts of this code must be compiled on a beaglebone setup with TI's PRU C compiler tools.

The top level code is in a pythonscript located at:

script/hydrophone.py

This script calls adc_record which is compiled from src/adc_record.c which will read 2 seconds of sample data from the PRU and dump it into a file in the working directory called test.dat.  This file is then read into the python script.  This data stream contains two interwoven channels of uint16 samples.  The channels are seperated and a matched filter is applied to channel A using an FFT Convolution against a constant tone of the pinger frequency lasting a few seconds.  The largest peaks on the signal are then found.  From here we attempt to find a point along the envelope of the signal between zero signal, where the filters ring causing an apparent 0 deg phase difference, and high up on the signal where reflections cause shifts in the phase creating errors.  Once the "sweet spot" is located we use a hilbert tranform of the signal in that area to determine the phase of the signal.

src/adc_record.c

This program triggers pru0 to start conversion by writing a character to its character driver and recieves sample data from the character driver and writes it out to a file.  Once 2 seconds of sample data are recorded it terminates.

src/pru0adc.c

This is compiled into the firmware that runs on the PRU 0 of the beagle bone.  This code configures the ADC to whatever parameters are defined at the top of the file.  Then it goes into an infinte loop where it fills a buffer with samples from the ADC and then forwards it to the main ARM cpu using the rpmsg framework.  In the main loop it waits for the EOC to go low indicating sample data is ready.  Then two samples of 16 bits each are read in using a loop unroll for consistent fast timing.  A fake read from an volitile variable is used to prevent optimization of the loop unroll from screwing up timing.  This is in a larger loop which takes the set of samples and puts them in a buffer that will be forwarded to the ARM cpu when full using rpmsg.

# Future Developments

1. We want to quantify how much CPU power is required to shuttle the data from the PRU to the ARM processor using the rpmsg framework.  It has been observed that packets are dropped when running dual channel at ~210ksps.  We want to compare this to using direct shared memory access.  The quantification can be done by writing a Userspace program to attach to the shared memory segment or read from the rpmsg character driver and just write to a circular buffer overwriting it self.  This will simulate passing data to the signal processing algorithm.  Then CPU load can be measured to determine which method is best.

2. We want to transition our python script for processing the signals in blocks from a file to running continusly on an input stream of data.  This way we dont have to sample for 2 secodns. process the sample, and then return results.  We can instead compute and return results in real time.  It would also be preferable to convert the python script to C code if possible to accelerate the processing.
