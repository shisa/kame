/*-
 * Copyright (c) 2000 Taku YAMAMOTO <taku@cent.saitama-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: maestro.c,v 1.1 2000/11/21 08:39:16 kawa Exp $
 * $FreeBSD: src/sys/dev/sound/pci/maestro.c,v 1.2.2.1 2000/10/05 05:30:46 cg Exp $
 */

/*
 * Credits:
 *
 * Part of this code (especially in many magic numbers) was heavily inspired
 * by the Linux driver originally written by
 * Alan Cox <alan.cox@linux.org>, modified heavily by
 * Zach Brown <zab@zabbo.net>.
 *
 * busdma()-ize and buffer size reduction were suggested by
 * Cameron Grant <gandalf@vilnya.demon.co.uk>.
 * Also he showed me the way to use busdma() suite.
 *
 * Internal speaker problems on NEC VersaPro's and Dell Inspiron 7500
 * were looked at by
 * Munehiro Matsuda <haro@tk.kubota.co.jp>,
 * who brought patches based on the Linux driver with some simplification.
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/sound/pci/maestro_reg.h>

#define inline __inline

/*
 * PCI IDs of supported chips:
 *
 * MAESTRO-1	0x01001285
 * MAESTRO-2	0x1968125d
 * MAESTRO-2E	0x1978125d
 */

#define MAESTRO_1_PCI_ID	0x01001285
#define MAESTRO_2_PCI_ID	0x1968125d
#define MAESTRO_2E_PCI_ID	0x1978125d

#define NEC_SUBID1	0x80581033	/* Taken from Linux driver */
#define NEC_SUBID2	0x803c1033	/* NEC VersaProNX VA26D    */

#ifndef AGG_MAXPLAYCH
# define AGG_MAXPLAYCH	4
#endif

#define AGG_BUFSIZ	4096


/* -----------------------------
 * Data structures.
 */
struct agg_chinfo {
	struct agg_info		*parent;
	pcm_channel		*channel;
	snd_dbuf		*buffer;
	bus_addr_t		offset;
	u_int32_t		blocksize;
	int			dir;
	u_int			num;
	u_int16_t		aputype;
	u_int16_t		wcreg_tpl;
};

struct agg_info {
	device_t		dev;
	struct resource		*reg;
	int			regid;

	bus_space_tag_t		st;
	bus_space_handle_t	sh;
	bus_dma_tag_t		parent_dmat;

	struct resource		*irq;
	int			irqid;
	void			*ih;

	u_int8_t		*stat;
	bus_addr_t		baseaddr;

	struct ac97_info	*codec;

	u_int			playchns, active;
	struct agg_chinfo	pch[AGG_MAXPLAYCH];
	struct agg_chinfo	rch;
};


static u_int32_t	 agg_rdcodec(void *, int);
static void		 agg_wrcodec(void *, int, u_int32_t);

static inline void	 ringbus_setdest(struct agg_info*, int, int);

static inline u_int16_t	 wp_rdreg(struct agg_info*, u_int16_t);
static inline void	 wp_wrreg(struct agg_info*, u_int16_t, u_int16_t);
static inline u_int16_t	 wp_rdapu(struct agg_info*, int, u_int16_t);
static inline void	 wp_wrapu(struct agg_info*, int, u_int16_t, u_int16_t);
static inline void	 wp_settimer(struct agg_info*, u_int);
static inline void	 wp_starttimer(struct agg_info*);
static inline void	 wp_stoptimer(struct agg_info*);

static inline u_int16_t	 wc_rdreg(struct agg_info*, u_int16_t);
static inline void	 wc_wrreg(struct agg_info*, u_int16_t, u_int16_t);
static inline u_int16_t	 wc_rdchctl(struct agg_info*, int);
static inline void	 wc_wrchctl(struct agg_info*, int, u_int16_t);

static inline void	 agg_power(struct agg_info*, int);

static void		 agg_init(struct agg_info*);
static u_int32_t	 agg_ac97_init(void *);

static void		 aggch_start_dac(struct agg_chinfo*);
static void		 aggch_stop_dac(struct agg_chinfo*);

static inline void	 suppress_jitter(struct agg_chinfo*);

static inline u_int	 calc_timer_freq(struct agg_chinfo*);
static void		 set_timer(struct agg_info*);

static pcmchan_init_t		aggch_init;
static pcmchan_free_t		aggch_free;
static pcmchan_setformat_t	aggch_setplayformat;
static pcmchan_setspeed_t	aggch_setspeed;
static pcmchan_setblocksize_t	aggch_setblocksize;
static pcmchan_trigger_t	aggch_trigger;
static pcmchan_getptr_t		aggch_getplayptr;
static pcmchan_getcaps_t	aggch_getcaps;

static void		 agg_intr(void *);
static int		 agg_probe(device_t);
static int		 agg_attach(device_t);
static int		 agg_detach(device_t);
static int		 agg_suspend(device_t);
static int		 agg_resume(device_t);
static int		 agg_shutdown(device_t);

static void	*dma_malloc(struct agg_info*, u_int32_t, bus_addr_t*);
static void	 dma_free(struct agg_info*, void *);

/* -----------------------------
 * Subsystems.
 */

/* Codec/Ringbus */

static u_int32_t
agg_rdcodec(void *sc, int regno)
{
	struct agg_info *ess = sc;
	unsigned t;

	/* We have to wait for a SAFE time to write addr/data */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) != CODEC_STAT_PROGLESS)
			break;
		DELAY(2);	/* 20.8us / 13 */
	}
	if (t == 20)
		device_printf(ess->dev, "agg_rdcodec() PROGLESS timed out.\n");

	bus_space_write_1(ess->st, ess->sh, PORT_CODEC_CMD,
	    CODEC_CMD_READ | regno);
	DELAY(21);	/* AC97 cycle = 20.8usec */

	/* Wait for data retrieve */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) == CODEC_STAT_RW_DONE)
			break;
		DELAY(2);	/* 20.8us / 13 */
	}
	if (t == 20)
		/* Timed out, but perform dummy read. */
		device_printf(ess->dev, "agg_rdcodec() RW_DONE timed out.\n");

	return bus_space_read_2(ess->st, ess->sh, PORT_CODEC_REG);
}

static void
agg_wrcodec(void *sc, int regno, u_int32_t data)
{
	unsigned t;
	struct agg_info *ess = sc;

	/* We have to wait for a SAFE time to write addr/data */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) != CODEC_STAT_PROGLESS)
			break;
		DELAY(2);	/* 20.8us / 13 */
	}
	if (t == 20) {
		/* Timed out. Abort writing. */
		device_printf(ess->dev, "agg_wrcodec() PROGLESS timed out.\n");
		return;
	}

	bus_space_write_2(ess->st, ess->sh, PORT_CODEC_REG, data);
	bus_space_write_1(ess->st, ess->sh, PORT_CODEC_CMD,
	    CODEC_CMD_WRITE | regno);
}

static inline void
ringbus_setdest(struct agg_info *ess, int src, int dest)
{
	u_int32_t	data;

	data = bus_space_read_4(ess->st, ess->sh, PORT_RINGBUS_CTRL);
	data &= ~(0xfU << src);
	data |= (0xfU & dest) << src;
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, data);
}

/* Wave Processor */

static inline u_int16_t
wp_rdreg(struct agg_info *ess, u_int16_t reg)
{
	bus_space_write_2(ess->st, ess->sh, PORT_DSP_INDEX, reg);
	return bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA);
}

static inline void
wp_wrreg(struct agg_info *ess, u_int16_t reg, u_int16_t data)
{
	bus_space_write_2(ess->st, ess->sh, PORT_DSP_INDEX, reg);
	bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, data);
}

static inline void
apu_setindex(struct agg_info *ess, u_int16_t reg)
{
	int t;

	wp_wrreg(ess, WPREG_CRAM_PTR, reg);
	/* Sometimes WP fails to set apu register index. */
	for (t = 0; t < 1000; t++) {
		if (bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA) == reg)
			break;
		bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, reg);
	}
	if (t == 1000)
		device_printf(ess->dev, "apu_setindex() timed out.\n");
}

static inline u_int16_t
wp_rdapu(struct agg_info *ess, int ch, u_int16_t reg)
{
	u_int16_t ret;

	apu_setindex(ess, ((unsigned)ch << 4) + reg);
	ret = wp_rdreg(ess, WPREG_DATA_PORT);
	return ret;
}

static inline void
wp_wrapu(struct agg_info *ess, int ch, u_int16_t reg, u_int16_t data)
{
	int t;

	apu_setindex(ess, ((unsigned)ch << 4) + reg);
	wp_wrreg(ess, WPREG_DATA_PORT, data);
	for (t = 0; t < 1000; t++) {
		if (bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA) == data)
			break;
		bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, data);
	}
	if (t == 1000)
		device_printf(ess->dev, "wp_wrapu() timed out.\n");
}

static inline void
wp_settimer(struct agg_info *ess, u_int freq)
{
	u_int clock = 48000 << 2;
	u_int prescale = 0, divide = (freq != 0) ? (clock / freq) : ~0;

	RANGE(divide, 4, 32 << 8);

	for (; divide > 32 << 1; divide >>= 1)
		prescale++;
	divide = (divide + 1) >> 1;

	for (; prescale < 7 && divide > 2 && !(divide & 1); divide >>= 1)
		prescale++;

	wp_wrreg(ess, WPREG_TIMER_ENABLE, 0);
	wp_wrreg(ess, WPREG_TIMER_FREQ,
	    (prescale << WP_TIMER_FREQ_PRESCALE_SHIFT) | (divide - 1));
	wp_wrreg(ess, WPREG_TIMER_ENABLE, 1);
}

static inline void
wp_starttimer(struct agg_info *ess)
{
	wp_wrreg(ess, WPREG_TIMER_START, 1);
}

static inline void
wp_stoptimer(struct agg_info *ess)
{
	wp_wrreg(ess, WPREG_TIMER_START, 0);
	bus_space_write_2(ess->st, ess->sh, PORT_INT_STAT, 1);
}

/* WaveCache */

static inline u_int16_t
wc_rdreg(struct agg_info *ess, u_int16_t reg)
{
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_INDEX, reg);
	return bus_space_read_2(ess->st, ess->sh, PORT_WAVCACHE_DATA);
}

static inline void
wc_wrreg(struct agg_info *ess, u_int16_t reg, u_int16_t data)
{
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_INDEX, reg);
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_DATA, data);
}

static inline u_int16_t
wc_rdchctl(struct agg_info *ess, int ch)
{
	return wc_rdreg(ess, ch << 3);
}

static inline void
wc_wrchctl(struct agg_info *ess, int ch, u_int16_t data)
{
	wc_wrreg(ess, ch << 3, data);
}

/* Power management */

static inline void
agg_power(struct agg_info *ess, int status)
{
	u_int8_t data;

	data = pci_read_config(ess->dev, CONF_PM_PTR, 1);
	if (pci_read_config(ess->dev, data, 1) == PPMI_CID)
		pci_write_config(ess->dev, data + PM_CTRL, status, 1);
}


/* -----------------------------
 * Controller.
 */

static inline void
agg_initcodec(struct agg_info* ess)
{
	u_int16_t data;

	if (bus_space_read_4(ess->st, ess->sh, PORT_RINGBUS_CTRL)
	    & RINGBUS_CTRL_ACLINK_ENABLED) {
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
		DELAY(104);	/* 20.8us * (4 + 1) */
	}
	/* XXX - 2nd codec should be looked at. */
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_AC97_SWRESET);
	DELAY(2);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_ACLINK_ENABLED);
	DELAY(21);

	agg_rdcodec(ess, 0);
	if (bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
	    & CODEC_STAT_MASK) {
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
		DELAY(21);

		/* Try cold reset. */
		device_printf(ess->dev, "will perform cold reset.\n");
		data = bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DIR);
		if (pci_read_config(ess->dev, 0x58, 2) & 1)
			data |= 0x10;
		data |= 0x009 &
		    ~bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DATA);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_MASK, 0xff6);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR,
		    data | 0x009);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x000);
		DELAY(2);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x001);
		DELAY(1);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x009);
		DELAY(500000);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR, data);
		DELAY(84);	/* 20.8us * 4 */
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
		    RINGBUS_CTRL_ACLINK_ENABLED);
		DELAY(21);
	}
}

static void
agg_init(struct agg_info* ess)
{
	u_int32_t data;

	/* Setup PCI config registers. */

	/* Disable all legacy emulations. */
	data = pci_read_config(ess->dev, CONF_LEGACY, 2);
	data |= LEGACY_DISABLED;
	pci_write_config(ess->dev, CONF_LEGACY, data, 2);

	/* Disconnect from CHI. (Makes Dell inspiron 7500 work?)
	 * Enable posted write.
	 * Prefer PCI timing rather than that of ISA.
	 * Don't swap L/R. */
	data = pci_read_config(ess->dev, CONF_MAESTRO, 4);
	data |= MAESTRO_CHIBUS | MAESTRO_POSTEDWRITE | MAESTRO_DMA_PCITIMING;
	data &= ~MAESTRO_SWAP_LR;
	pci_write_config(ess->dev, CONF_MAESTRO, data, 4);

	/* Reset direct sound. */
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL,
	    HOSTINT_CTRL_DSOUND_RESET);
	DELAY(10000);	/* XXX - too long? */
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL, 0);
	DELAY(10000);

	/* Enable direct sound interruption. */
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL,
	    HOSTINT_CTRL_DSOUND_INT_ENABLED);

	/* Setup Wave Processor. */

	/* Enable WaveCache, set DMA base address. */
	wp_wrreg(ess, WPREG_WAVE_ROMRAM,
	    WP_WAVE_VIRTUAL_ENABLED | WP_WAVE_DRAM_ENABLED);
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_CTRL,
	    WAVCACHE_ENABLED | WAVCACHE_WTSIZE_4MB);

	for (data = WAVCACHE_PCMBAR; data < WAVCACHE_PCMBAR + 4; data++)
		wc_wrreg(ess, data, ess->baseaddr >> WAVCACHE_BASEADDR_SHIFT);

	/* Setup Codec/Ringbus. */
	agg_initcodec(ess);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_RINGBUS_ENABLED | RINGBUS_CTRL_ACLINK_ENABLED);

	wp_wrreg(ess, WPREG_BASE, 0x8500);	/* Parallel I/O */
	ringbus_setdest(ess, RINGBUS_SRC_ADC,
	    RINGBUS_DEST_STEREO | RINGBUS_DEST_DSOUND_IN);
	ringbus_setdest(ess, RINGBUS_SRC_DSOUND,
	    RINGBUS_DEST_STEREO | RINGBUS_DEST_DAC);

	/* Setup ASSP. Needed for Dell Inspiron 7500? */
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_B, 0x00);
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_A, 0x03);
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_C, 0x00);

	/*
	 * Setup GPIO.
	 * There seems to be speciality with NEC systems.
	 */
	switch (pci_get_subvendor(ess->dev)
	    | (pci_get_subdevice(ess->dev) << 16)) {
	case NEC_SUBID1:
	case NEC_SUBID2:
		/* Matthew Braithwaite <matt@braithwaite.net> reported that
		 * NEC Versa LX doesn't need GPIO operation. */
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_MASK, 0x9ff);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR,
		    bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DIR) | 0x600);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x200);
		break;
	}
}

/* Channel controller. */

static void
aggch_start_dac(struct agg_chinfo *ch)
{
	u_int wpwa = APU_USE_SYSMEM | (ch->offset >> 9);
	u_int size = AGG_BUFSIZ >> 1;
	u_int speed = ch->channel->speed;
	u_int offset = ch->offset >> 1;
	u_int cp = ch->buffer->rp >> 1;
	u_int16_t apuch = ch->num << 1;
	u_int dv;
	int pan = 0;

	switch (ch->aputype) {
	case APUTYPE_16BITSTEREO:
		wpwa >>= 1;
		size >>= 1;
		offset >>= 1;
		cp >>= 1;
		/* FALLTHROUGH */
	case APUTYPE_8BITSTEREO:
		pan = 8;
		apuch++;
		break;
	case APUTYPE_8BITLINEAR:
		speed >>= 1;
		break;
	}

	dv = (((speed % 48000) << 16) + 24000) / 48000
	    + ((speed / 48000) << 16);

	do {
		wp_wrapu(ch->parent, apuch, APUREG_WAVESPACE, wpwa & 0xff00);
		wp_wrapu(ch->parent, apuch, APUREG_CURPTR, offset + cp);
		wp_wrapu(ch->parent, apuch, APUREG_ENDPTR, offset + size);
		wp_wrapu(ch->parent, apuch, APUREG_LOOPLEN, size);
		wp_wrapu(ch->parent, apuch, APUREG_AMPLITUDE, 0xe800);
		wp_wrapu(ch->parent, apuch, APUREG_POSITION, 0x8f00
		    | (RADIUS_CENTERCIRCLE << APU_RADIUS_SHIFT)
		    | ((PAN_FRONT + pan) << APU_PAN_SHIFT));
		wp_wrapu(ch->parent, apuch, APUREG_FREQ_LOBYTE, APU_plus6dB
		    | ((dv & 0xff) << APU_FREQ_LOBYTE_SHIFT));
		wp_wrapu(ch->parent, apuch, APUREG_FREQ_HIWORD, dv >> 8);

		if (ch->aputype == APUTYPE_16BITSTEREO)
			wpwa |= APU_STEREO >> 1;
		pan = -pan;
	} while (pan < 0 && apuch--);

	wc_wrchctl(ch->parent, apuch, ch->wcreg_tpl);
	wc_wrchctl(ch->parent, apuch + 1, ch->wcreg_tpl);

	wp_wrapu(ch->parent, apuch, APUREG_APUTYPE,
	    (ch->aputype << APU_APUTYPE_SHIFT) | APU_DMA_ENABLED | 0xf);
	if (ch->wcreg_tpl & WAVCACHE_CHCTL_STEREO)
		wp_wrapu(ch->parent, apuch + 1, APUREG_APUTYPE,
		    (ch->aputype << APU_APUTYPE_SHIFT) | APU_DMA_ENABLED | 0xf);
}

static void
aggch_stop_dac(struct agg_chinfo *ch)
{
	wp_wrapu(ch->parent, (ch->num << 1), APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
	wp_wrapu(ch->parent, (ch->num << 1) + 1, APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
}

/*
 * Stereo jitter suppressor.
 * Sometimes playback pointers differ in stereo-paired channels.
 * Calling this routine within intr fixes the problem.
 */
static inline void
suppress_jitter(struct agg_chinfo *ch)
{
	if (ch->wcreg_tpl & WAVCACHE_CHCTL_STEREO) {
		int cp, diff, halfsize = AGG_BUFSIZ >> 2;

		if (ch->aputype == APUTYPE_16BITSTEREO)
			halfsize >>= 1;
		cp = wp_rdapu(ch->parent, (ch->num << 1), APUREG_CURPTR);
		diff = wp_rdapu(ch->parent, (ch->num << 1) + 1, APUREG_CURPTR);
		diff -= cp;
		if (diff >> 1 && diff > -halfsize && diff < halfsize)
			bus_space_write_2(ch->parent->st, ch->parent->sh,
			    PORT_DSP_DATA, cp);
	}
}

static inline u_int
calc_timer_freq(struct agg_chinfo *ch)
{
	u_int	ss = 2;

	if (ch->aputype == APUTYPE_16BITSTEREO)
		ss <<= 1;
	if (ch->aputype == APUTYPE_8BITLINEAR)
		ss >>= 1;

	return (ch->channel->speed * ss + ch->blocksize - 1) / ch->blocksize;
}

static void
set_timer(struct agg_info *ess)
{
	int i;
	u_int	freq = 0;

	for (i = 0; i < ess->playchns; i++)
		if ((ess->active & (1 << i)) &&
		    (freq < calc_timer_freq(ess->pch + i)))
			freq = calc_timer_freq(ess->pch + i);

	wp_settimer(ess, freq);
}


/* -----------------------------
 * Newpcm glue.
 */

static u_int32_t
agg_ac97_init(void *sc)
{
	struct agg_info *ess = sc;

	return (bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT) & CODEC_STAT_MASK)? 0 : 1;
}

static void *
aggch_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct agg_info *ess = devinfo;
	struct agg_chinfo *ch;
	bus_addr_t physaddr;

	ch = (dir == PCMDIR_PLAY)? ess->pch + ess->playchns : &ess->rch;

	ch->parent = ess;
	ch->channel = c;
	ch->buffer = b;
	ch->num = ess->playchns;
	ch->dir = dir;

	b->buf = dma_malloc(ess, AGG_BUFSIZ, &physaddr);
	if (b->buf == NULL)
		return NULL;

	ch->offset = physaddr - ess->baseaddr;
	if (physaddr < ess->baseaddr || ch->offset > WPWA_MAXADDR) {
		device_printf(ess->dev,
		    "offset %#x exceeds limit. ", ch->offset);
		dma_free(ess, b->buf);
		b->buf = NULL;
		return NULL;
	}

	b->bufsize = AGG_BUFSIZ;
	ch->wcreg_tpl = (physaddr - 16) & WAVCACHE_CHCTL_ADDRTAG_MASK;

	if (dir == PCMDIR_PLAY) {
		ess->playchns++;
		if (bootverbose)
			device_printf(ess->dev, "pch[%d].offset = %#x\n", ch->num, ch->offset);
	} else if (bootverbose)
		device_printf(ess->dev, "rch.offset = %#x\n", ch->offset);

	return ch;
}

static int
aggch_free(void *data)
{
	struct agg_chinfo *ch = data;
	struct agg_info *ess = ch->parent;

	/* free up buffer - called after channel stopped */
	dma_free(ess, ch->buffer->buf);

	/* return 0 if ok */
	return 0;
}

static int
aggch_setplayformat(void *data, u_int32_t format)
{
	struct agg_chinfo *ch = data;
	u_int16_t wcreg_tpl;
	u_int16_t aputype = APUTYPE_16BITLINEAR;

	wcreg_tpl = ch->wcreg_tpl & WAVCACHE_CHCTL_ADDRTAG_MASK;

	if (format & AFMT_STEREO) {
		wcreg_tpl |= WAVCACHE_CHCTL_STEREO;
		aputype += 1;
	}
	if (format & AFMT_U8 || format & AFMT_S8) {
		aputype += 2;
		if (format & AFMT_U8)
			wcreg_tpl |= WAVCACHE_CHCTL_U8;
	}
	if (format & AFMT_BIGENDIAN || format & AFMT_U16_LE) {
		format &= ~AFMT_BIGENDIAN & ~AFMT_U16_LE;
		format |= AFMT_S16_LE;
	}
	ch->wcreg_tpl = wcreg_tpl;
	ch->aputype = aputype;
	return format;
}

static int
aggch_setspeed(void *data, u_int32_t speed)
{
	return speed;
}

static int
aggch_setblocksize(void *data, u_int32_t blocksize)
{
	return ((struct agg_chinfo*)data)->blocksize = blocksize;
}

static int
aggch_trigger(void *data, int go)
{
	struct agg_chinfo *ch = data;

	switch (go) {
	case PCMTRIG_EMLDMAWR:
		return 0;
	case PCMTRIG_START:
		ch->parent->active |= (1 << ch->num);
		if (ch->dir == PCMDIR_PLAY)
			aggch_start_dac(ch);
#if 0	/* XXX - RECORDING */
		else
			aggch_start_adc(ch);
#endif
		break;
	case PCMTRIG_ABORT:
	case PCMTRIG_STOP:
		ch->parent->active &= ~(1 << ch->num);
		if (ch->dir == PCMDIR_PLAY)
			aggch_stop_dac(ch);
#if 0	/* XXX - RECORDING */
		else
			aggch_stop_adc(ch);
#endif
		break;
	}

	if (ch->parent->active) {
		set_timer(ch->parent);
		wp_starttimer(ch->parent);
	} else
		wp_stoptimer(ch->parent);

	return 0;
}

static int
aggch_getplayptr(void *data)
{
	struct agg_chinfo *ch = data;
	u_int cp;

	cp = wp_rdapu(ch->parent, (ch->num << 1), APUREG_CURPTR);
	if (ch->aputype == APUTYPE_16BITSTEREO)
		cp = (0xffff << 2) & ((cp << 2) - ch->offset);
	else
		cp = (0xffff << 1) & ((cp << 1) - ch->offset);

	return cp;
}

static pcmchan_caps *
aggch_getcaps(void *data)
{
	static u_int32_t playfmt[] = {
		AFMT_U8,
		AFMT_STEREO | AFMT_U8,
		AFMT_S8,
		AFMT_STEREO | AFMT_S8,
		AFMT_S16_LE,
		AFMT_STEREO | AFMT_S16_LE,
		0
	};
	static pcmchan_caps playcaps = {2000, 96000, playfmt, 0};

	static u_int32_t recfmt[] = {
		AFMT_S8,
		AFMT_STEREO | AFMT_S8,
		AFMT_S16_LE,
		AFMT_STEREO | AFMT_S16_LE,
		0
	};
	static pcmchan_caps reccaps = {4000, 48000, recfmt, 0};

	return (((struct agg_chinfo*)data)->dir == PCMDIR_PLAY)?
	    &playcaps : &reccaps;
}


/* -----------------------------
 * Bus space.
 */

static void
agg_intr(void *sc)
{
	struct agg_info* ess = sc;
	u_int16_t status;
	int i;

	status = bus_space_read_1(ess->st, ess->sh, PORT_HOSTINT_STAT);
	if (!status)
		return;

	/* Acknowledge all. */
	bus_space_write_2(ess->st, ess->sh, PORT_INT_STAT, 1);
	bus_space_write_1(ess->st, ess->sh, PORT_HOSTINT_STAT, 0);
#if 0	/* XXX - HWVOL */
	if (status & HOSTINT_STAT_HWVOL) {
		u_int delta;
		delta = bus_space_read_1(ess->st, ess->sh, PORT_HWVOL_MASTER)
		    - 0x88;
		if (delta & 0x11)
			mixer_set(device_get_softc(ess->dev),
			    SOUND_MIXER_VOLUME, 0);
		else {
			mixer_set(device_get_softc(ess->dev),
			    SOUND_MIXER_VOLUME,
			    mixer_get(device_get_softc(ess->dev),
				SOUND_MIXER_VOLUME)
			    + ((delta >> 5) & 0x7) - 4
			    + ((delta << 7) & 0x700) - 0x400);
		}
		bus_space_write_1(ess->st, ess->sh, PORT_HWVOL_MASTER, 0x88);
	}
#endif	/* XXX - HWVOL */

	for (i = 0; i < ess->playchns; i++)
		if (ess->active & (1 << i)) {
			suppress_jitter(ess->pch + i);
			chn_intr(ess->pch[i].channel);
		}
#if 0	/* XXX - RECORDING */
	if (ess->active & (1 << i))
		chn_intr(ess->rch.channel);
#endif
}

static void
setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *phys = arg;

	*phys = error? 0 : segs->ds_addr;

	if (bootverbose) {
		printf("setmap (%lx, %lx), nseg=%d, error=%d\n",
		    (unsigned long)segs->ds_addr, (unsigned long)segs->ds_len,
		    nseg, error);
	}
}

static void *
dma_malloc(struct agg_info *sc, u_int32_t sz, bus_addr_t *phys)
{
	void *buf;
	bus_dmamap_t map;

	if (bus_dmamem_alloc(sc->parent_dmat, &buf, BUS_DMA_NOWAIT, &map))
		return NULL;
	if (bus_dmamap_load(sc->parent_dmat, map, buf, sz, setmap, phys, 0)
	    || !*phys) {
		bus_dmamem_free(sc->parent_dmat, buf, map);
		return NULL;
	}
	return buf;
}

static void
dma_free(struct agg_info *sc, void *buf)
{
	bus_dmamem_free(sc->parent_dmat, buf, NULL);
}

static int
agg_probe(device_t dev)
{
	char *s = NULL;

	switch (pci_get_devid(dev)) {
	case MAESTRO_1_PCI_ID:
		s = "ESS Technology Maestro-1";
		break;

	case MAESTRO_2_PCI_ID:
		s = "ESS Technology Maestro-2";
		break;

	case MAESTRO_2E_PCI_ID:
		s = "ESS Technology Maestro-2E";
		break;
	}

	if (s != NULL && pci_get_class(dev) == PCIC_MULTIMEDIA) {
		device_set_desc(dev, s);
		return 0;
	}
	return ENXIO;
}

static int
agg_attach(device_t dev)
{
	struct agg_info	*ess = NULL;
	u_int32_t	data;
	int	mapped = 0;
	int	regid = PCIR_MAPS;
	struct resource	*reg = NULL;
	struct ac97_info	*codec = NULL;
	int	irqid = 0;
	struct resource	*irq = NULL;
	void	*ih = NULL;
	char	status[SND_STATUSLEN];
	static pcm_channel agg_pchtpl = {
	    	aggch_init,
		NULL, 			/* setdir */
	    	aggch_setplayformat,
	    	aggch_setspeed,
	    	aggch_setblocksize,
	    	aggch_trigger,
	    	aggch_getplayptr,
	    	aggch_getcaps,
		aggch_free, 		/* free */
		NULL, 			/* nop1 */
		NULL, 			/* nop2 */
		NULL, 			/* nop3 */
		NULL, 			/* nop4 */
		NULL, 			/* nop5 */
		NULL, 			/* nop6 */
		NULL, 			/* nop7 */
	};

	if ((ess = malloc(sizeof *ess, M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}
	bzero(ess, sizeof *ess);
	ess->dev = dev;

	if (bus_dma_tag_create(/*parent*/NULL,
	    /*alignment*/1 << WAVCACHE_BASEADDR_SHIFT,
	    /*boundary*/WPWA_MAXADDR + 1,
	    /*lowaddr*/MAESTRO_MAXADDR, /*highaddr*/BUS_SPACE_MAXADDR,
	    /*filter*/NULL, /*filterarg*/NULL,
	    /*maxsize*/AGG_BUFSIZ * 2, /*nsegments*/1, /*maxsegz*/0x3ffff,
	    /*flags*/0, &ess->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	ess->stat = dma_malloc(ess, AGG_BUFSIZ, &ess->baseaddr);
	if (ess->stat == NULL) {
		device_printf(dev, "cannot allocate status buffer\n");
		goto bad;
	}
	if (bootverbose)
		device_printf(dev, "Maestro DMA base: %#x\n", ess->baseaddr);

	agg_power(ess, PPMI_D0);
	DELAY(100000);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	if (data & PCIM_CMD_PORTEN) {
		reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &regid,
		    0, BUS_SPACE_UNRESTRICTED, 256, RF_ACTIVE);
		if (reg != NULL) {
			ess->reg = reg;
			ess->regid = regid;
			ess->st = rman_get_bustag(reg);
			ess->sh = rman_get_bushandle(reg);
			mapped++;
		}
	}
	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	agg_init(ess);
	if (agg_rdcodec(ess, 0) == 0x80) {
		device_printf(dev, "PT101 codec detected!\n");
		goto bad;
	}
	codec = ac97_create(dev, ess, agg_ac97_init, agg_rdcodec, agg_wrcodec);
	if (codec == NULL)
		goto bad;
	if (mixer_init(dev, &ac97_mixer, codec) == -1)
		goto bad;
	ess->codec = codec;

	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &irqid,
	    0, BUS_SPACE_UNRESTRICTED, 1, RF_ACTIVE | RF_SHAREABLE);
	if (irq == NULL
	    || bus_setup_intr(dev, irq, INTR_TYPE_TTY, agg_intr, ess, &ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}
	ess->irq = irq;
	ess->irqid = irqid;
	ess->ih = ih;

	snprintf(status, SND_STATUSLEN, "at I/O port 0x%lx irq %ld",
	    rman_get_start(reg), rman_get_start(irq));

	if (pcm_register(dev, ess, AGG_MAXPLAYCH, 1))
		goto bad;

	for (data = 0; data < AGG_MAXPLAYCH; data++)
		pcm_addchan(dev, PCMDIR_PLAY, &agg_pchtpl, ess);
#if 0	/* XXX - RECORDING */
	pcm_addchan(dev, PCMDIR_REC, &agg_rchtpl, ess);
#endif
	pcm_setstatus(dev, status);

	return 0;

 bad:
	if (codec != NULL)
		ac97_destroy(codec);
	if (ih != NULL)
		bus_teardown_intr(dev, irq, ih);
	if (irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, irqid, irq);
	if (reg != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, regid, reg);
	if (ess != NULL) {
		agg_power(ess, PPMI_D3);
		if (ess->stat != NULL)
			dma_free(ess, ess->stat);
		if (ess->parent_dmat != NULL)
			bus_dma_tag_destroy(ess->parent_dmat);
		free(ess, M_DEVBUF);
	}

	return ENXIO;
}

static int
agg_detach(device_t dev)
{
	struct agg_info	*ess = pcm_getdevinfo(dev);
	int r;

	r = pcm_unregister(dev);
	if (r)
		return r;

	ess = pcm_getdevinfo(dev);
	dma_free(ess, ess->stat);

	/* Power down everything except clock and vref. */
	agg_wrcodec(ess, AC97_REG_POWER, 0xd700);
	DELAY(20);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
	agg_power(ess, PPMI_D3);

	bus_teardown_intr(dev, ess->irq, ess->ih);
	bus_release_resource(dev, SYS_RES_IRQ, ess->irqid, ess->irq);
	bus_release_resource(dev, SYS_RES_IOPORT, ess->regid, ess->reg);
	bus_dma_tag_destroy(ess->parent_dmat);
	free(ess, M_DEVBUF);
	return 0;
}

static int
agg_suspend(device_t dev)
{
	struct agg_info *ess = pcm_getdevinfo(dev);
	int i, x;

	x = spltty();
	wp_stoptimer(ess);
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL, 0);

	for (i = 0; i < ess->playchns; i++)
		aggch_stop_dac(ess->pch + i);

#if 0	/* XXX - RECORDING */
	aggch_stop_adc(&ess->rch);
#endif
	splx(x);
	/* Power down everything except clock. */
	agg_wrcodec(ess, AC97_REG_POWER, 0xdf00);
	DELAY(20);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
	DELAY(1);
	agg_power(ess, PPMI_D3);

	return 0;
}

static int
agg_resume(device_t dev)
{
	int i, x;
	struct agg_info *ess = pcm_getdevinfo(dev);

	agg_power(ess, PPMI_D0);
	DELAY(100000);
	agg_init(ess);
	if (mixer_reinit(dev)) {
		device_printf(dev, "unable to reinitialize the mixer\n");
		return ENXIO;
	}

	x = spltty();
	for (i = 0; i < ess->playchns; i++)
		if (ess->active & (1 << i))
			aggch_start_dac(ess->pch + i);
#if 0	/* XXX - RECORDING */
	if (ess->active & (1 << i))
		aggch_start_adc(&ess->rch);
#endif
	if (ess->active) {
		set_timer(ess);
		wp_starttimer(ess);
	}
	splx(x);
	return 0;
}

static int
agg_shutdown(device_t dev)
{
	struct agg_info *ess = pcm_getdevinfo(dev);
	int i;

	wp_stoptimer(ess);
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL, 0);

	for (i = 0; i < ess->playchns; i++)
		aggch_stop_dac(ess->pch + i);

#if 0	/* XXX - RECORDING */
	aggch_stop_adc(&ess->rch);
#endif
	return 0;
}


static device_method_t agg_methods[] = {
    DEVMETHOD(device_probe,	agg_probe),
    DEVMETHOD(device_attach,	agg_attach),
    DEVMETHOD(device_detach,	agg_detach),
    DEVMETHOD(device_suspend,	agg_suspend),
    DEVMETHOD(device_resume,	agg_resume),
    DEVMETHOD(device_shutdown,	agg_shutdown),

    { 0, 0 }
};

static driver_t agg_driver = {
    "pcm",
    agg_methods,
    sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_maestro, pci, agg_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_maestro, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_maestro, 1);
