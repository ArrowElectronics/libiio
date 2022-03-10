# libiio Examples

The libiio is a cross platform library for interfacing with Linux IIO devices. 

These are some primitive examples of using the libiio library. 

These are only build if the -DWITH_EXAMPLES=ON is provided to the standard Cmake.

While the libiio library is tested on many operating systems, these examples are only tested on Linux.

More examples using all languages supported by libiio are encouraged to be added here.
When adding a new example, please update this list. 

## ad9361-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD9361 found on the AD-FMCOMMS2-EBZ, AD-FMCOMMS3-EBZ, and the ADRV9361-Z7035 RF SOM.
It takes the uri as the only argument. for example : `./ad9361-iiostream usb:3.32.5`

## ad9371-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD9371.
It takes the uri as the only argument. for example : `./ad9371-iiostream ip:192.168.2.1`

## adrv9009-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the ADRV9009.
It takes the uri as the only argument. for example : `./adrv9009-iiostream ip:192.168.2.1`

## ad7768-1-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD7768-1.
It takes the uri as the only argument. for example : `./ad7768-1-iiostream ip:192.168.2.1`

## ad7768-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD7768.
It takes the uri as the only argument. for example : `./ad7768-iiostream ip:192.168.2.1`

## ad4020-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD4020.
It takes the uri as the only argument. for example : `./ad4020-iiostream ip:192.168.2.1`

## ad7606b-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD7606B.
It takes the uri as the only argument. for example : `./ad7606b-iiostream ip:192.168.2.1`

## ad7380-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD7380.
It takes the uri as the only argument. for example : `./ad7380-iiostream ip:192.168.2.1`

## ad7381-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD7381.
It takes the uri as the only argument. for example : `./ad7381-iiostream ip:192.168.2.1`

## adrv9002-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the ADRV9002.
It takes the uri as the only argument. for example : `./adrv9002-iiostream ip:192.168.2.1`

## ad4696-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD4696.
It takes the uri as the only argument. for example : `./ad4696-iiostream ip:192.168.2.1`

## ad4630-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO functionality on the AD4630.
It takes the uri as the only argument. for example : `./ad4630-iiostream ip:192.168.2.1`

## dummy-iiostream
  * Language : C

This example libiio program is meant to exercise the features of IIO present in the sample dummy IIO device in the linux kernel.
For buffered access it relies on the hrtimer trigger but could be modified to use the sysfs trigger. 
No hardware should be required to run this program.

## iio-monitor
  * Language : C
  * Requirements : Curses Development Kit (libcdk5-dev); pthreads; ncurses; libiio

A Curses based application which implements real time monitoring of IIO non-buffer samples.
