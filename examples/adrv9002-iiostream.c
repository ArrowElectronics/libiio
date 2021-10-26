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
#define DAC_MODE_REGISTER 0x0418

/** TX DAC Output Mode Selector
 *  Select internal data sources. Supported values are:
 *  - 0 (0x00): internal tone (DDS)
 *  - 1 (0x01): pattern (SED)
 *  - 2 (0x02): input data (DMA Buffer)
 *  - 3 (0x03): 0x00 (Standby)
 *  - 6 (0x06): pn7 (standard O.150)
 *  - 7 (0x07): pn15 (standard O.150)
 *  - 10 (0x0A): Nibble ramp (Device specific e.g. adrv9001)
 *  - 11 (0x0B): 16 bit ramp (Device specific e.g. adrv9001)
*/
#define TX_DAC_MODE 0

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

#if (TX_DAC_MODE == 2)
const uint32_t sine_lut_iq[1024] = {
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A,
	0x00002666, 0x01E2265A, 0x03C32636, 0x05A225FB, 0x077D25A9,
	0x0954253F, 0x0B2524BE, 0x0CEF2427, 0x0EB12379, 0x106A22B6,
	0x121921DD, 0x13BD20EF, 0x15551FED, 0x16DF1ED7, 0x185C1DAE,
	0x19C91C73, 0x1B261B26, 0x1C7319C9, 0x1DAE185C, 0x1ED716DF,
	0x1FED1555, 0x20EF13BD, 0x21DD1219, 0x22B6106A, 0x23790EB1,
	0x24270CEF, 0x24BE0B25, 0x253F0954, 0x25A9077D, 0x25FB05A2,
	0x263603C3, 0x265A01E2, 0x26660000, 0x265AFE1E, 0x2636FC3D,
	0x25FBFA5E, 0x25A9F883, 0x253FF6AC, 0x24BEF4DB, 0x2427F311,
	0x2379F14F, 0x22B6EF96, 0x21DDEDE7, 0x20EFEC43, 0x1FEDEAAB,
	0x1ED7E921, 0x1DAEE7A4, 0x1C73E637, 0x1B26E4DA, 0x19C9E38D,
	0x185CE252, 0x16DFE129, 0x1555E013, 0x13BDDF11, 0x1219DE23,
	0x106ADD4A, 0x0EB1DC87, 0x0CEFDBD9, 0x0B25DB42, 0x0954DAC1,
	0x077DDA57, 0x05A2DA05, 0x03C3D9CA, 0x01E2D9A6, 0x0000D99A,
	0xFE1ED9A6, 0xFC3DD9CA, 0xFA5EDA05, 0xF883DA57, 0xF6ACDAC1,
	0xF4DBDB42, 0xF311DBD9, 0xF14FDC87, 0xEF96DD4A, 0xEDE7DE23,
	0xEC43DF11, 0xEAABE013, 0xE921E129, 0xE7A4E252, 0xE637E38D,
	0xE4DAE4DA, 0xE38DE637, 0xE252E7A4, 0xE129E921, 0xE013EAAB,
	0xDF11EC43, 0xDE23EDE7, 0xDD4AEF96, 0xDC87F14F, 0xDBD9F311,
	0xDB42F4DB, 0xDAC1F6AC, 0xDA57F883, 0xDA05FA5E, 0xD9CAFC3D,
	0xD9A6FE1E, 0xD99A0000, 0xD9A601E2, 0xD9CA03C3, 0xDA0505A2,
	0xDA57077D, 0xDAC10954, 0xDB420B25, 0xDBD90CEF, 0xDC870EB1,
	0xDD4A106A, 0xDE231219, 0xDF1113BD, 0xE0131555, 0xE12916DF,
	0xE252185C, 0xE38D19C9, 0xE4DA1B26, 0xE6371C73, 0xE7A41DAE,
	0xE9211ED7, 0xEAAB1FED, 0xEC4320EF, 0xEDE721DD, 0xEF9622B6,
	0xF14F2379, 0xF3112427, 0xF4DB24BE, 0xF6AC253F, 0xF88325A9,
	0xFA5E25FB, 0xFC3D2636, 0xFE1E265A
};
#endif

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

	/* printout some useful info */
	ret = iio_channel_attr_read_longlong(chan, "rf_bandwidth", &val);
	if (ret)
		return ret;

	info("adrv9002 bandwidth: %lld\n", val);

	ret = iio_channel_attr_read_longlong(chan, "sampling_frequency", &val);
	if (ret)
		return ret;

	info("adrv9002 sampling_frequency: %lld\n", val);

	/* set the LO to 2.5GHz */
	val = GHZ(2.5);
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

/** Generate a single tone using the DDSs
 *  For complex data devices this will create a complex
 *  or single sided tone spectrally using two DDSs.
 *  parameters:
 *      frequency: type=long long
 *          Frequency in hertz of the generated tone. This must be
 *          less than 1/2 the sample rate.
 *      scale: type=double
 *          Scale of the generated tone in range [0,1]. At 1 the tone
 *          will be full-scale.
 *      channel: type=uint16_t
 *          Channel index to generate tone from. This is zero based
 *          and for complex devices this index relates to the pair
 *          of related converters. For non-complex devices this is
 *          the index of the individual converters.
 */
static int dds_single_tone(long long freq_val, double scale_val, uint16_t channel)
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

	}

	iio_channel_enable(chan_i);
	iio_channel_enable(chan_q);

	return 0;
}

/**
 *  Generate two tones simultaneously using the DDSs
 *  For complex data devices this will create two complex
 *  or single sided tones spectrally using four DDSs.
 *  parameters:
 *      freq_val1: type=long long
 *          Frequency of first tone in hertz of the generated tone.
 *          This must be less than 1/2 the sample rate.
 *      scale_val1: type=double
 *          Scale of the first tone generated tone in range [0,1].
 *          At 1 the tone will be full-scale.
 *      freq_val2: type=long
 *          Frequency of second tone in hertz of the generated tone.
 *          This must be less than 1/2 the sample rate.
 *      scale_val2: type=double
 *          Scale of the second tone generated tone in range [0,1].
 *          At 1 the tone will be full-scale.
 *      channel: type=uint16_t
 *          Channel index to generate tone from. This is zero based
 *          and for complex devices this index relates to the pair
 *          of related converters. For non-complex devices this is
 *          the index of the individual converters.
 */
static int dds_dual_tone(long long freq_val1, double scale_val1,
			long long freq_val2, double scale_val2, uint16_t channel)
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

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val1);
		if (ret)
		return ret;

		// Find TX1_Q_F1 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage2", true);
		if (!chan_q) {
			error("Could not find TX altvoltage2 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_longlong(chan_q, "phase", 90000);
		if (ret)
			return ret;


		// Find TX1_I_F2 channel attributes for given channel
		chan_i = iio_device_find_channel(tx, "altvoltage1", true);
		if (!chan_i) {
			error("Could not find TX altvoltage1 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val2);
		if (ret)
		return ret;

		// Find TX1_Q_F2 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage3", true);
		if (!chan_q) {
			error("Could not find TX altvoltage3 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_longlong(chan_q, "phase", 90000);
		if (ret)
			return ret;

	}
	else {
		// Find TX2_I_F1 channel attributes
		chan_i = iio_device_find_channel(tx, "altvoltage4", true);
		if (!chan_i) {
			error("Could not find TX altvoltage4 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val1);
		if (ret)
		return ret;

		// Find TX2_Q_F1 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage6", true);
		if (!chan_q) {
			error("Could not find TX altvoltage6 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val1);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_longlong(chan_q, "phase", 90000);
		if (ret)
			return ret;

		// Find TX2_I_F2 channel attributes
		chan_i = iio_device_find_channel(tx, "altvoltage5", true);
		if (!chan_i) {
			error("Could not find TX altvoltage5 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_i, "frequency", freq_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_i, "scale", scale_val2);
		if (ret)
		return ret;

		// Find TX2_Q_F2 channel attributes
		chan_q = iio_device_find_channel(tx, "altvoltage7", true);
		if (!chan_q) {
			error("Could not find TX altvoltage7 channel\n");
			return -ENODEV;
		}

		ret = iio_channel_attr_write_longlong(chan_q, "frequency", freq_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_double(chan_q, "scale", scale_val2);
		if (ret)
			return ret;

		ret = iio_channel_attr_write_longlong(chan_q, "phase", 90000);
		if (ret)
			return ret;
	}

	iio_channel_enable(chan_i);
	iio_channel_enable(chan_q);

	return 0;
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

static void stream(void)
{
	const struct iio_channel *rx_i_chan = rx_chan[I_CHAN];
#if (TX_DAC_MODE == 2)
	const struct iio_channel *tx_i_chan = tx_chan[I_CHAN];
	uint32_t i;
#endif

	info("* Starting IO streaming (press CTRL+C to cancel)\n");
	while (!stop) {
		ssize_t nbytes_rx;
		int16_t *p_dat, *p_end;
		ptrdiff_t p_inc;

#if (TX_DAC_MODE == 2)
		ssize_t nbytes_tx;
		nbytes_tx = iio_buffer_push(txbuf);
		if (nbytes_tx < 0) {
			error("Error pushing buf %zd\n", nbytes_tx);
			return;
		}
#endif
		info("Refilling RX buffer in 5 seconds. Press Ctrl+C to exit...\n");
		sleep(5);

		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) {
			error("Error refilling buf %zd\n", nbytes_rx);
			return;
		}

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

			printf("Voltage (Q) = %d		Voltage (I) = %d\n", p_dat[0], p_dat[1]);
		}

#if (TX_DAC_MODE == 2)
		/* WRITE: Get pointers to TX buf and write IQ to TX buf port 0 */
		p_inc = iio_buffer_step(txbuf);
		p_end = iio_buffer_end(txbuf);
		for (p_dat = iio_buffer_first(txbuf, tx_i_chan), i = 0; p_dat < p_end;
		     p_dat += p_inc / sizeof(*p_dat), i++) {
			if (i == 1024)
				i = 0;

			p_dat[0] = (uint16_t) (sine_lut_iq[i] >> 16); /* Real (I) */
			p_dat[1] = (uint16_t) (sine_lut_iq[i]); /* Imag (Q) */
		}
#endif
	}
}

/* simple configuration and streaming */
/* usage:
 * Default context, assuming local IIO devices, i.e., this script is run on-device for example
 $./a.out
 * URI context, find out the uri by typing `iio_info -s` at the command line of the host PC
 $./a.out usb:x.x.x
 */
int main(int argc, char **argv)
{
	struct iio_device *tx;
	struct iio_device *rx;
	struct iio_device *phy;
	uint32_t reg_val;
	int ret;

	if (register_signals() < 0)
		return EXIT_FAILURE;

	printf("* Acquiring IIO context\n");
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

	ret = configure_trx_lo();
	if (ret)
		goto clean;

	phy = iio_context_find_device(ctx, "adrv9002-phy");
	if (!phy) {
		ret = EXIT_FAILURE;
		goto clean;
	}

	/* Enable digital loopback */
	//iio_device_debug_attr_write(phy, "tx0_ssi_test_mode_loopback_en", "1");

	tx = iio_context_find_device(ctx, "axi-adrv9002-tx-lpc");
	if (!tx) {
		ret = EXIT_FAILURE;
		goto clean;
	}

	iio_device_reg_write(tx, DAC_MODE_REGISTER, TX_DAC_MODE);
	iio_device_reg_read(tx, DAC_MODE_REGISTER, &reg_val);
	printf("reg_val = 0x%x\n", reg_val);

#if (TX_DAC_MODE == 0)
	/* Generate a DDS single tone waveform */
	ret = dds_single_tone(5000, 0.4, 0);
	if (ret)
		goto clean;
#endif

	rx = iio_context_find_device(ctx, "axi-adrv9002-rx-lpc");
	if (!rx) {
		ret = EXIT_FAILURE;
		goto clean;
	}

	ret = stream_channels_get_enable(rx, rx_chan, false);
	if (ret)
		goto clean;

#if (TX_DAC_MODE == 2)
	ret = stream_channels_get_enable(tx, tx_chan, true);
	if (ret)
		goto clean;

	txbuf = iio_device_create_buffer(tx, 1024 * 1024, false);
	if (!txbuf) {
		error("Could not create TX buffer: %s\n", strerror(errno));
		ret = EXIT_FAILURE;
		goto clean;
	}
#endif
	info("* Creating non-cyclic IIO buffers with 1 MiS\n");
	rxbuf = iio_device_create_buffer(rx, 1024 * 1024, false);
	if (!rxbuf) {
		error("Could not create RX buffer: %s\n", strerror(errno));
		ret = EXIT_FAILURE;
		goto clean;
	}

	stream();

	/* Disable digital loopback if enabled */
	iio_device_debug_attr_write(phy, "tx0_ssi_test_mode_loopback_en", "0");

clean:
	cleanup();
	return ret;
}
