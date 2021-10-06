// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libiio - Library for interfacing industrial I/O (IIO) devices
 *
 * Copyright (C) 2021 Analog Devices, Inc.
 * Author: Nuno Sá <nuno.sa@analog.com>
 */
#include <iio.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#define ARGS(fmt, ...)	__VA_ARGS__
#define FMT(fmt, ...)	fmt
#define error(...) \
	printf("%s, %d: ERROR: " FMT(__VA_ARGS__, 0)"%s", __func__, __LINE__, ARGS(__VA_ARGS__, ""))

#define info(...) \
	printf("%s, %d: INFO: " FMT(__VA_ARGS__, 0)"%s", __func__, __LINE__, ARGS(__VA_ARGS__, ""))

/* helper macros */
#define GHZ(x) ((long long)(x * 1000000000.0 + .5))
#define MHZ(x) ((long long)(x * 1000000.0 + .5))

static bool stop = false;
static struct iio_context *ctx = NULL;
static struct iio_buffer *rxbuf = NULL;
static struct iio_buffer *txbuf = NULL;
static struct iio_channel *rx_chan[2] = { NULL, NULL };
static struct iio_channel *tx_chan[2] = { NULL, NULL };

enum {
	I_CHAN,
	Q_CHAN
};

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>

BOOL WINAPI sig_handler(DWORD dwCtrlType)
{
	/* Runs in its own thread */
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		stop = true;
		return true;
	default:
		return false;
	}
}

static int register_signals(void)
{
	if (!SetConsoleCtrlHandler(sig_handler, TRUE))
		return -1;

	return 0;
}
#else
static void sig_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		info("Exit....\n");
		stop = true;
	}
}

static int register_signals(void)
{
	struct sigaction sa = {0};
	sigset_t mask = {0};

	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sigemptyset(&mask);

	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		error("sigaction: %s\n", strerror(errno));
		return -1;
	}

	if (sigaction(SIGINT, &sa, NULL) < 0) {
		error("sigaction: %s\n", strerror(errno));
		return -1;
	}

	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	/* make sure these signals are unblocked */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
		error("sigprocmask: %s", strerror(errno));
		return -1;
	}

	return 0;
}
#endif

static int configure_trx_lo(void)
{
	struct iio_device *phy;
	struct iio_channel *chan;
	int ret;
	long long val;

	phy = iio_context_find_device(ctx, "adrv9002-phy");
	if (!phy) {
		error("Could not find adrv9002_phy\n");
		return -ENODEV;
	}

	chan = iio_device_find_channel(phy, "voltage0", true);
	if (!chan) {
		error("Could not find TX voltage0 channel\n");
		return -ENODEV;
	}

	chan = iio_device_find_channel(phy, "voltage1", true);
	if (!chan) {
		error("Could not find TX voltage1 channel\n");
		return -ENODEV;
	}

	/* printout some useful info */
	ret = iio_channel_attr_read_longlong(chan, "rf_bandwidth", &val);
	if (ret)
		return ret;

	info("adrv9002 bandwidth: %lld\n", val);

	ret = iio_channel_attr_read_longlong(chan, "sampling_frequency", &val);
	if (ret)
		return ret;

	info("adrv9002 sampling_frequency: %lld\n", val);

	/* set the LO to 2.4GHz */
	val = GHZ(2.4);

	chan = iio_device_find_channel(phy, "altvoltage2", true);
	if (!chan) {
		error("Could not find TX LO channel\n");
		return -ENODEV;
	}

	ret = iio_channel_attr_write_longlong(chan, "TX1_LO_frequency", val);
	if (ret)
		return ret;

	chan = iio_device_find_channel(phy, "altvoltage0", true);
	if (!chan) {
		error("Could not find RX LO channel\n");
		return -ENODEV;
	}

	ret = iio_channel_attr_write_longlong(chan, "RX1_LO_frequency", val);
	if (ret)
		return ret;
}

static void cleanup(void)
{
	int c;

	if (rxbuf)
		iio_buffer_destroy(rxbuf);

	if (txbuf)
		iio_buffer_destroy(txbuf);

	for (c = 0; c < 2; c++) {
		if (rx_chan[c])
			iio_channel_disable(rx_chan[c]);

		if (tx_chan[c])
			iio_channel_disable(tx_chan[c]);
	}

	iio_context_destroy(ctx);
}

static int stream_channels_get_enable(const struct iio_device *dev, struct iio_channel **chan,
				      bool tx)
{
	int c;
	const char * const channels[] = {
		"voltage0_i", "voltage0_q", "voltage0", "voltage1"
	};

	for (c = 0; c < 2; c++) {
		const char *str = channels[tx * 2 + c];

		chan[c] = iio_device_find_channel(dev, str, tx);
		if (!chan[c]) {
			error("Could not find %s channel tx=%d\n", str, tx);
			return -ENODEV;
		}

		iio_channel_enable(chan[c]);
	}

	return 0;
}

static int configure_tx_dds(long long freq_val, double scale_val, uint16_t channel)
{
	struct iio_device *tx;
	struct iio_channel *chan_i;
	struct iio_channel *chan_q;
	int ret;

	tx = iio_context_find_device(ctx, "axi-adrv9002-tx-lpc");
	if (!tx) {
		error("Could not find axi-adrv9002-tx-lpc\n");
		return -ENODEV;
	}

	if (channel == 0) {
		// Find TX1_I_F1 channel attributes for given channel
		chan_i = iio_device_find_channel(tx, "altvoltage0", true);
		if (!chan_i) {
			error("Could not find TX altvoltage0 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val);
		if (ret)
		return ret;

		// print written values
		ret = iio_channel_attr_read_longlong(chan_i, "frequency", &freq_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage0 frequency: %lld\n", freq_val);

		ret = iio_channel_attr_read_double(chan_i, "scale", &scale_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage0 scale: %lf\n", scale_val);

		// Find TX1_Q_F1 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage2", true);
		if (!chan_q) {
			error("Could not find TX altvoltage2 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val);
		if (ret)
			return ret;

		// print written values
		ret = iio_channel_attr_read_longlong(chan_q, "frequency", &freq_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage2 frequency: %lld\n", freq_val);

		ret = iio_channel_attr_read_double(chan_q, "scale", &scale_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage2 scale: %lf\n", scale_val);
	}
	else {
		// Find TX2_I_F1 channel attributes
		chan_i = iio_device_find_channel(tx, "altvoltage4", true);
		if (!chan_i) {
			error("Could not find TX altvoltage4 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val);
		if (ret)
		return ret;

		// print written values
		ret = iio_channel_attr_read_longlong(chan_i, "frequency", &freq_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage4 frequency: %lld\n", freq_val);

		ret = iio_channel_attr_read_double(chan_i, "scale", &scale_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage4 scale: %lf\n", scale_val);

		// Find TX2_Q_F1 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage6", true);
		if (!chan_q) {
			error("Could not find TX altvoltage6 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val);
		if (ret)
			return ret;

		// print written values
		ret = iio_channel_attr_read_longlong(chan_q, "frequency", &freq_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage6 frequency: %lld\n", freq_val);

		ret = iio_channel_attr_read_double(chan_q, "scale", &scale_val);
		if (ret)
			return ret;

		info("adrv9002 altvoltage6 scale: %lf\n", scale_val);
	}

		iio_channel_enable(chan_i);
		iio_channel_enable(chan_q);

	return 0;
}


static void stream(void)
{
	const struct iio_channel *rx_i_chan = rx_chan[I_CHAN];
	ssize_t nrx = 0;
	const bool print_val = false;

	info("* Starting IO streaming (press CTRL+C to cancel)\n");
	sleep(5);
	while (!stop) {
		ssize_t nbytes_rx, nbytes_tx;
		int16_t *p_dat, *p_end;
		ptrdiff_t p_inc;

		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) {
			error("Error refilling buf %zd\n", nbytes_rx);
			return;
		}

		info("Buffer refilled\n");

		/* READ: Get pointers to RX buf and read IQ from RX buf port 0 */
		p_inc = iio_buffer_step(rxbuf);
		p_end = iio_buffer_end(rxbuf);
		for (p_dat = iio_buffer_first(rxbuf, rx_i_chan); p_dat < p_end;
		     p_dat += p_inc / sizeof(*p_dat)) {
			/* Example: swap I and Q */
			int16_t i = p_dat[0];
			int16_t q = p_dat[1];

			p_dat[0] = q;
			p_dat[1] = i;

			if (print_val)
				printf("q_data = %d\t\ti_data = %d\n", p_dat[0], p_dat[1]);
		}

		info("Refilling RX buffer in 5 seconds. Press Ctrl+C to exit...\n");
		sleep(5);
	}

#if 0
	/* Sample counter increment and status output */
	nrx += nbytes_rx / rx_sample;
	info("\tRX %8.2f MSmp\n", nrx / 1e6);
#endif
}

int main(int argc, char **argv)
{
	struct iio_device *tx;
	struct iio_device *rx;
	struct iio_channel *chan;
	int ret;

	if (register_signals() < 0)
		return EXIT_FAILURE;

        if (argc == 1) {
                ctx = iio_create_default_context();
        }
        else if (argc == 2) {
                ctx = iio_create_context_from_uri(argv[1]);
        }
	if (!ctx) {
		error("Could not create IIO context\n");
		return EXIT_FAILURE;
	}

	/* Configure the local oscillator */
	ret = configure_trx_lo();
	if (ret)
		goto clean;

	/* Configure DDS to generate single tone waveform
	 * Generate a single tone waveform with arguments as:
	 * 	long long freq_val: Frequency Value
	 *	double scale_val: Scale Value between 0 and 1 for the
	 *		      amplitude of waveform (1 being full scale swing)
	 *	uint16_t channel: TX channel number to enable
	 */
	ret = configure_tx_dds(5000, 0.4, 0);
	if (ret)
		goto clean;

	rx = iio_context_find_device(ctx, "axi-adrv9002-rx-lpc");
	if (!rx) {
		ret = EXIT_FAILURE;
		goto clean;
	}

	ret = stream_channels_get_enable(rx, rx_chan, false);
	if (ret)
		goto clean;

	info("* Creating non-cyclic IIO buffers with 1 MiS\n");
	rxbuf = iio_device_create_buffer(rx, 1024 * 1024, false);
	if (!rxbuf) {
		error("Could not create RX buffer: %s\n", strerror(errno));
		ret = EXIT_FAILURE;
		goto clean;
	}

	stream();

clean:
	cleanup();
	return ret;
}
