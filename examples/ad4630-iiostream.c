/*
 * libiio - AD4630-24 IIO streaming example
 *
 * Based on AD7768 example
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
#define CHANNEL_NUMBER 0  // AD4630-24 has 2 channels
#define MODES_REG 0x20

/* Modes Register Bit Description
 *   OUT_DATA_MD: 0 = 24-bit data
 *                1 = 16-bit data, 8-bit CM
 *                2 = 24-bit data, 8-bit CM
 *                3 = 30-bit avg data, 1 OR bit, 1 SYNC bit (Needs additional config)
 *                4 = 32-bit test pattern
 *
 *   DDR_MD: 0 = SDR Mode
 *           1 = DDR Mode
 *
 *   CLK_MD: 0 = SPI clocking mode.
 *           1 = echo clock mode.
 *           2 = master clock mode.
 *           3 = invalid setting
 *
 *   LANE_MD: 0 = one lane per channel.
 *            1 = two lanes per channel.
 *            2 = four lanes per channel.
 *            3 = Unsupported
 **/
#define OUT_DATA_MD 1
#define DDR_MD 0
#define CLK_MD 0
#define LANE_MD 2

#if (OUT_DATA_MD == 1 || OUT_DATA_MD == 2)
#define VCOM_ENABLE // Enable capture of VCOM data
#elif (OUT_DATA_MD == 3)
#define OR_SYNC_ENABLE // Enable capture of OR and SYNC bits
#endif

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

/* read attribute: long long int */
static long long rd_ch_lli(struct iio_channel *chn, const char* what)
{
	long long val;

	errchk(iio_channel_attr_read_longlong(chn, what, &val), what);

	printf("\t %s: %lld\n", what, val);
	return val;
}

#if 0
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

/* returns AD4630-24 device */
static struct iio_device* get_ad4630(struct iio_context *ctx)
{
	struct iio_device *dev =  iio_context_find_device(ctx, "ad4630");
	IIO_ENSURE(dev && "No ad463x found");
	return dev;
}

/* finds AD4630-24 streaming IIO devices */
static bool get_ad4630_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
	switch (d) {
	case RX: *dev = iio_context_find_device(ctx, "ad4630");  return *dev != NULL;
	default: IIO_ENSURE(0); return false;
	}
}

/* finds AD4630-24 streaming IIO channels */
static bool get_ad4630_stream_ch(__notused struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), false);
	return *chn != NULL;
}

/* finds AD4630-24 IIO configuration channel with id chid */
static bool get_channel(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
	switch (d) {
	case RX: *chn = iio_device_find_channel(get_ad4630(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
	default: IIO_ENSURE(0); return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad4630_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	// Configure channels
	printf("* Acquiring AD4630 channel %d\n", chid);
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
	uint32_t reg_val = 0;

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

	printf("* Acquiring AD4630-24 streaming devices\n");
	IIO_ENSURE(get_ad4630_stream_dev(ctx, RX, &rx) && "No rx dev found");

        /* Set modes register to desired mode */
	iio_device_reg_write(rx, MODES_REG, ((LANE_MD << 6) | (CLK_MD << 4) |
						(DDR_MD << 3) | OUT_DATA_MD ));
	iio_device_reg_read(rx, MODES_REG, &reg_val);
	printf("* Modes Register 0x20  = 0x%x\n", reg_val);

	printf("* Configuring AD4630-24 for streaming\n");
	IIO_ENSURE(cfg_ad4630_streaming_ch(ctx, &rxcfg, RX, CHANNEL_NUMBER) && "RX port not found");

	printf("* Initializing AD4630-24 IIO streaming channels\n");
	IIO_ENSURE(get_ad4630_stream_ch(ctx, RX, rx, CHANNEL_NUMBER, &rxchan) && "RX chan not found");

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
		uint8_t or_sync = 0;
		ssize_t nbytes_rx, bytes;
		long int sample;
		long long int vcom_data;

		// Refill RX buffer
		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) { printf("Error refilling buf %d\n",(int) nbytes_rx); shutdown(); }

#ifdef VCOM_ENABLE
		// Print VCOM data if enabled
		rd_ch_lli(rxchan, "common_mode_voltage");
#endif
		// Get data format from any one channel
		const struct iio_data_format *fmt = iio_channel_get_data_format(rxchan);
		unsigned long int sample_size = fmt->length / 8 * fmt->repeat;
		printf("Fmt length = %u, fmt repeat = %u, sample size = %lu\n", fmt->length, fmt->repeat, sample_size);

		// Print each captured sample
		buf = malloc(sample_size * BUFFER_LENGTH);
		if (!buf) {
			perror("trying to allocate memory for buffer\n");
			shutdown();
		}

		bytes = iio_channel_read_raw(rxchan, rxbuf, buf, sample_size * BUFFER_LENGTH);
		printf("%s \n", iio_channel_get_id(rxchan));

		if (!CHANNEL_NUMBER) {
			for (sample = 0; sample < bytes / sample_size; ++sample) {
				/* Format the data according to the mode selected */
#if !(OUT_DATA_MD)
				((int32_t *)buf)[sample] = (int32_t)((int64_t *)buf)[sample] >> 8;
#elif (OUT_DATA_MD == 1)
				((int32_t *)buf)[sample] = ((int32_t)((int64_t *)buf)[sample]) >> 16;
#elif (OUT_DATA_MD == 2)
				((int32_t *)buf)[sample] = (int32_t)((int64_t *)buf)[sample] >> 8;
#elif (OUT_DATA_MD == 3)
				or_sync = ((int32_t)((int64_t *)buf)[sample]) & 0x3;
				((int32_t *)buf)[sample] = ((int32_t)((int64_t *)buf)[sample]) >> 2;
#elif (OUT_DATA_MD == 4)
				((int32_t *)buf)[sample] = (int32_t)((int64_t *)buf)[sample];
#endif


#ifndef OR_SYNC_ENABLE
				printf("Buffer Sample: %ld\tCH0: 0x%x\n", sample, ((int32_t *)buf)[sample]);
#else
				/* Print Channel 0 OR_SYNC data */
				printf("Buffer Sample: %ld\tCH0: 0x%x\tOR_SYNC: 0x%x\n", sample, ((int32_t *)buf)[sample], or_sync);
#endif
			}
		}
		else {
			for (sample = 0; sample < bytes / sample_size; ++sample) {

				/* Format the data according to the mode selected */
#if !(OUT_DATA_MD)
				((int32_t *)buf)[sample] = (int32_t)(((int64_t *)buf)[sample] >> 32) >> 8;
#elif (OUT_DATA_MD == 1)
				((int32_t *)buf)[sample] = ((int32_t)(((int64_t *)buf)[sample] >> 32)) >> 16;
#elif (OUT_DATA_MD == 2)
				((int32_t *)buf)[sample] = (int32_t)(((int64_t *)buf)[sample] >> 32) >> 8;
#elif (OUT_DATA_MD == 3)
				or_sync = ((int32_t)(((int64_t *)buf)[sample] >> 32)) & 0x3;
				((int32_t *)buf)[sample] = (int32_t)(((int64_t *)buf)[sample] >> 32) >> 2;
#elif (OUT_DATA_MD == 4)
				((int32_t *)buf)[sample] = (int32_t)(((int64_t *)buf)[sample] >> 32);
#endif

#ifndef OR_SYNC_ENABLE
				printf("Buffer Sample: %ld\tCH1: 0x%x\n", sample, ((int32_t *)buf)[sample]);
#else
				/* Print Channel 1 OR_SYNC data */
				printf("Buffer Sample: %ld\tCH1: 0x%x\tOR_SYNC: 0x%x\n", sample, ((int32_t *)buf)[sample], or_sync);
#endif
			}
		}

		free(buf);
		printf("\n");
	}

	/* Set 0x20 back to linux default value */
	reg_val = 0;
	iio_device_reg_write(rx, MODES_REG, 0x82);
	iio_device_reg_read(rx, MODES_REG, &reg_val);
	printf("* Modes Register 0x20  = 0x%x\n", reg_val);

	shutdown();

	return 0;
}

