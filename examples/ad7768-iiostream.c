/*
 * libiio - AD7768 IIO streaming example
 *
 * Based on AD9361 example:
 * Copyright (C) 2014 IABG mbH
 * Author: Michael Feilen <feilen_at_iabg.de>
 *
 * Based on DummyIIO example:
 * Copyright (c) 2016, DAQRI. All rights reserved.
 * Author: Lucas Magasweran <lucas.magasweran@daqri.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <iio.h>

/* helper macros */
#define BUFFER_LENGTH 400
#define CHANNEL_NUMBER 4  // AD7768 has eight channels

#define IIO_ENSURE(expr) { \
	if (!(expr)) { \
		(void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
		(void) abort(); \
	} \
}

/* RX is input, TX is output */
enum iodev { RX, TX };

/* common RX streaming params */
struct stream_cfg {
	// Empty as no channel to configure for write
};

/* static scratch mem for strings */
static char tmpstr[64];

/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rxchan = NULL;
static struct iio_buffer  *rxbuf = NULL;

static bool stop;

/* cleanup and exit */
static void shutdown()
{
	printf("* Destroying buffers\n");
	if (rxbuf) { iio_buffer_destroy(rxbuf); }

	printf("* Disabling streaming channels\n");
	if (rxchan) { iio_channel_disable(rxchan); }

	printf("* Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(0);
}

static void handle_sig(int sig)
{
	printf("Waiting for process to finish... Got signal %d\n", sig);
	stop = true;
}

/* check return value of attr_read function */
static void errchk(int v, const char* what) {
	if (v < 0) { fprintf(stderr, "Error %d reading channel \"%s\"\nvalue may not be supported.\n", v, what); shutdown(); }
}

#if 0
/* read attribute: long long int */
static long long rd_ch_lli(struct iio_channel *chn, const char* what)
{
	long long val;

	errchk(iio_channel_attr_read_longlong(chn, what, &val), what);

	printf("\t %s: %lld\n", what, val);
	return val;
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	errchk(iio_channel_attr_write(chn, what, str), what);
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}
#endif

/*
 helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
	snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
	return tmpstr;
}

/* returns AD7768 device */
static struct iio_device* get_ad7768(struct iio_context *ctx)
{
	struct iio_device *dev =  iio_context_find_device(ctx, "axi-ad7768-adc");
	IIO_ENSURE(dev && "No axi-ad7768-adc found");
	return dev;
}

/* finds AD7768 streaming IIO devices */
static bool get_ad7768_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
	switch (d) {
	case RX: *dev = iio_context_find_device(ctx, "axi-ad7768-adc");  return *dev != NULL;
	default: IIO_ENSURE(0); return false;
	}
}

/* finds AD7768 streaming IIO channels */
static bool get_ad7768_stream_ch(__notused struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), false);
	return *chn != NULL;
}

/* finds AD7768 IIO configuration channel with id chid */
static bool get_channel(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
	switch (d) {
	case RX: *chn = iio_device_find_channel(get_ad7768(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
	default: IIO_ENSURE(0); return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad7768_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	// Configure channels
	printf("* Acquiring AD7768 channel %d\n", chid);
	if (!get_channel(ctx, type, chid, &chn)) {      return false; }

	return true;
}

/* simple configuration and streaming */
/* usage:
 * Default context, assuming local IIO devices, i.e., this script is run on ADALM-Pluto for example
 $./a.out
 * URI context, find out the uri by typing `iio_info -s` at the command line of the host PC
 $./a.out usb:x.x.x
 */
int main (int argc, char **argv)
{
	// Streaming devices
	struct iio_device *rx;

	// RX and TX sample counters
	size_t nrx = 0;

	// Stream configurations
	struct stream_cfg rxcfg;

	// Listen to ctrl+c and IIO_ENSURE
	signal(SIGINT, handle_sig);

	unsigned int index;

	printf("* Acquiring IIO context\n");
	if (argc == 1) {
		IIO_ENSURE((ctx = iio_create_default_context()) && "No context");
	}
	else if (argc == 2) {
		IIO_ENSURE((ctx = iio_create_context_from_uri(argv[1])) && "No context");
	}
	IIO_ENSURE(iio_context_get_devices_count(ctx) > 0 && "No devices");

	printf("* Acquiring AD7768 streaming devices\n");
	IIO_ENSURE(get_ad7768_stream_dev(ctx, RX, &rx) && "No rx dev found");

	printf("* Configuring AD7768 for streaming\n");
	IIO_ENSURE(cfg_ad7768_streaming_ch(ctx, &rxcfg, RX, CHANNEL_NUMBER) && "RX port not found");

	printf("* Initializing AD7768 IIO streaming channels\n");
	IIO_ENSURE(get_ad7768_stream_ch(ctx, RX, rx, CHANNEL_NUMBER, &rxchan) && "RX chan not found");

	printf("* Enabling IIO streaming channels\n");
	iio_channel_enable(rxchan);

	printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
	rxbuf = iio_device_create_buffer(rx, BUFFER_LENGTH, false);
	if (!rxbuf) {
		perror("Could not create RX buffer");
		shutdown();
	}

	printf("* Starting IO streaming (press CTRL+C to cancel)\n");
	while (!stop)
	{
		uint8_t *buf;
		ssize_t nbytes_rx, bytes, sample;

		// Refill RX buffer
		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) { printf("Error refilling buf %d\n",(int) nbytes_rx); shutdown(); }

		// Get data format from any one channel
		const struct iio_data_format *fmt = iio_channel_get_data_format(rxchan);
		size_t sample_size = fmt->length / 8 * fmt->repeat;
		printf("Fmt length = %u, fmt repeat = %u, sample size = %u\n", fmt->length, fmt->repeat, sample_size);

		// Print each captured sample
		buf = malloc(sample_size * BUFFER_LENGTH);
		if (!buf) {
			perror("trying to allocate memory for buffer\n");
			shutdown();
		}

		bytes = iio_channel_read_raw(rxchan, rxbuf, buf, sample_size * BUFFER_LENGTH);
		printf("%s ", iio_channel_get_id(rxchan));
		for (sample = 0; sample < bytes / sample_size; ++sample)
			printf("%" PRId32 " ", ((int32_t *)buf)[sample] << 8);

		free(buf);
		printf("\n");
	}

	shutdown();

	return 0;
}
