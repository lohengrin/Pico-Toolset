#include <stdlib.h>
#include <stdio.h>
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "dvi.h"
#include "dvi_timing.h"
#include "dvi_serialiser.h"
#include "tmds_encode.h"
#if DVI_ENABLE_AUDIO
#include "hardware/sync.h" // __dmb() -- see dvi_set_audio_freq()'s PATCH comment
#endif
#if DVI_ENABLE_IRQ_STATS
#include "hardware/timer.h" // time_us_32() -- see dvi_dma_irq_handler()'s PATCH comment
#endif

// Time-critical functions pulled into RAM but each in a unique section to
// allow garbage collection
#define __dvi_func(f) __not_in_flash_func(f)
#define __dvi_func_x(f) __scratch_x(__STRING(f)) f

// We require exclusive use of a DMA IRQ line. (you wouldn't want to share
// anyway). It's possible in theory to hook both IRQs and have two DVI outs.
static struct dvi_inst *dma_irq_privdata[2];
static void dvi_dma0_irq();
static void dvi_dma1_irq();

#if DVI_ENABLE_AUDIO
// PATCH (hdmi-audio): re-encodes inst->next_data_stream for the upcoming
// scanline -- called once per scanline from dvi_dma_irq_handler() once data
// islands are enabled. Ported verbatim from rh1tech/frank-hdmi-audio's
// frank_dvi.c (BSD-3-Clause): ask dvi_update_data_packet_() what this
// scanline's data island should carry (an InfoFrame, the ACR packet, an
// audio-sample packet, or nothing -- see that function's own doc comment in
// dvi.h), fall back to a null packet if it declines, then TERC4-encode it.
static inline void __dvi_func(dvi_update_data_packet)(struct dvi_inst *inst) {
	data_packet_t packet;
	if (!dvi_update_data_packet_(inst, &packet)) {
		data_packet_set_null(&packet);
	}
	bool vsync = inst->timing_state.v_state == DVI_STATE_SYNC;
	data_packet_encode(&inst->next_data_stream, &packet,
		inst->timing->v_sync_polarity == vsync, inst->timing->h_sync_polarity);
}
#endif

void dvi_init(struct dvi_inst *inst, uint spinlock_tmds_queue, uint spinlock_colour_queue) {
	dvi_timing_state_init(&inst->timing_state);
	dvi_serialiser_init(&inst->ser_cfg);
	for (int i = 0; i < N_TMDS_LANES; ++i) {
		inst->dma_cfg[i].chan_ctrl = dma_claim_unused_channel(true);
		inst->dma_cfg[i].chan_data = dma_claim_unused_channel(true);
		inst->dma_cfg[i].tx_fifo = (void*)&inst->ser_cfg.pio->txf[inst->ser_cfg.sm_tmds[i]];
		inst->dma_cfg[i].dreq = pio_get_dreq(inst->ser_cfg.pio, inst->ser_cfg.sm_tmds[i], true);
	}
	inst->late_scanline_ctr = 0;
	inst->tmds_buf_release_next = NULL;
	inst->tmds_buf_release = NULL;
#if DVI_ENABLE_AUDIO
	// Frame counter for InfoFrame alternation -- see dvi_dma_irq_handler()'s
	// own PATCH comment.
	inst->dvi_frame_count = 0;
#endif
#if DVI_ENABLE_IRQ_STATS
	// PATCH (irq-stats): see struct dvi_inst's own doc comment in dvi.h.
	inst->irq_us_accum = 0;
#endif
	queue_init_with_spinlock(&inst->q_tmds_valid,   sizeof(void*),  8, spinlock_tmds_queue);
	queue_init_with_spinlock(&inst->q_tmds_free,    sizeof(void*),  8, spinlock_tmds_queue);
	queue_init_with_spinlock(&inst->q_colour_valid, sizeof(void*),  8, spinlock_colour_queue);
	queue_init_with_spinlock(&inst->q_colour_free,  sizeof(void*),  8, spinlock_colour_queue);

#if DVI_ENABLE_AUDIO
	// PATCH (hdmi-audio): every list is built in the data-island-carrying
	// shape right here, before this function returns -- i.e. before
	// dvi_start()/core1 are ever reached, so no list is ever mutated while
	// the DMA engine is walking it (see struct dvi_inst's own doc comment
	// in dvi.h for why there is no runtime shape switch). `black` is always
	// false, same blank-scanline behavior as before this patch.
	dvi_setup_scanline_for_vblank_with_audio(inst->timing, inst->dma_cfg, true, &inst->dma_list_vblank_sync);
	dvi_setup_scanline_for_vblank_with_audio(inst->timing, inst->dma_cfg, false, &inst->dma_list_vblank_nosync);
	dvi_setup_scanline_for_active_with_audio(inst->timing, inst->dma_cfg, (void*)SRAM_BASE, &inst->dma_list_active, false);
	dvi_setup_scanline_for_active_with_audio(inst->timing, inst->dma_cfg, NULL, &inst->dma_list_error, false);
	// dvi_audio_init() runs *after* the lists above exist -- it repoints
	// each one's data-island slot at next_data_stream (dvi.h's own doc
	// comment on dvi_audio_init() covers this).
	dvi_audio_init(inst);
	// AVI InfoFrame: static per-mode video-format metadata, set once here
	// (not per dvi_set_audio_freq() call) since it never changes -- values
	// match this project's fixed 640x480p60 RGB output. Ported from
	// shuichitakano's pico_lib DVI constructor (MIT License), which sets
	// this exact InfoFrame the same way.
	data_packet_set_avi_info_frame(&inst->avi_info_frame, SCAN_INFO_UNDERSCAN, PIXEL_FORMAT_RGB, COLORIMETRY_ITU601,
		PICTURE_ASPECT_RATIO_4_3, ACTIVE_FORMAT_ASPECT_RATIO_SAME_AS_PAR, RGB_QUANTIZATION_FULL, VIDEO_CODE_640x480P60);
#else
	dvi_setup_scanline_for_vblank(inst->timing, inst->dma_cfg, true, &inst->dma_list_vblank_sync);
	dvi_setup_scanline_for_vblank(inst->timing, inst->dma_cfg, false, &inst->dma_list_vblank_nosync);
	dvi_setup_scanline_for_active(inst->timing, inst->dma_cfg, (void*)SRAM_BASE, &inst->dma_list_active);
	dvi_setup_scanline_for_active(inst->timing, inst->dma_cfg, NULL, &inst->dma_list_error);
#endif

	for (int i = 0; i < DVI_N_TMDS_BUFFERS; ++i) {
		void *tmdsbuf;
#if DVI_MONOCHROME_TMDS
		tmdsbuf = malloc(inst->timing->h_active_pixels / DVI_SYMBOLS_PER_WORD * sizeof(uint32_t));
#else
		tmdsbuf = malloc(3 * inst->timing->h_active_pixels / DVI_SYMBOLS_PER_WORD * sizeof(uint32_t));
#endif
		if (!tmdsbuf)
			panic("TMDS buffer allocation failed");
		queue_add_blocking_u32(&inst->q_tmds_free, &tmdsbuf);
	}
}

// The IRQs will run on whichever core calls this function (this is why it's
// called separately from dvi_init)
void dvi_register_irqs_this_core(struct dvi_inst *inst, uint irq_num) {
	uint32_t mask_sync_channel = 1u << inst->dma_cfg[TMDS_SYNC_LANE].chan_data;
	uint32_t mask_all_channels = 0;
	for (int i = 0; i < N_TMDS_LANES; ++i)
		mask_all_channels |= 1u << inst->dma_cfg[i].chan_ctrl | 1u << inst->dma_cfg[i].chan_data;

	dma_hw->ints0 = mask_sync_channel;
	if (irq_num == DMA_IRQ_0) {
		hw_write_masked(&dma_hw->inte0, mask_sync_channel, mask_all_channels);
		dma_irq_privdata[0] = inst;
		irq_set_exclusive_handler(DMA_IRQ_0, dvi_dma0_irq);
	}
	else {
		hw_write_masked(&dma_hw->inte1, mask_sync_channel, mask_all_channels);
		dma_irq_privdata[1] = inst;
		irq_set_exclusive_handler(DMA_IRQ_1, dvi_dma1_irq);
	}
	irq_set_enabled(irq_num, true);
}

// Set up control channels to make transfers to data channels' control
// registers (but don't trigger the control channels -- this is done either by
// data channel CHAIN_TO or an initial write to MULTI_CHAN_TRIGGER)
static inline void __attribute__((always_inline)) _dvi_load_dma_op(const struct dvi_lane_dma_cfg dma_cfg[], struct dvi_scanline_dma_list *l) {
	for (int i = 0; i < N_TMDS_LANES; ++i) {
		dma_channel_config cfg = dma_channel_get_default_config(dma_cfg[i].chan_ctrl);
		channel_config_set_ring(&cfg, true, 4); // 16-byte write wrap
		channel_config_set_read_increment(&cfg, true);
		channel_config_set_write_increment(&cfg, true);
		dma_channel_configure(
			dma_cfg[i].chan_ctrl,
			&cfg,
			&dma_hw->ch[dma_cfg[i].chan_data],
			dvi_lane_from_list(l, i),
			4, // Configure all 4 registers then halt until next CHAIN_TO
			false
		);
	}
}

// Setup first set of control block lists, configure the control channels, and
// trigger them. Control channels will subsequently be triggered only by DMA
// CHAIN_TO on data channel completion. IRQ handler *must* be prepared before
// calling this. (Hooked to DMA IRQ0)
void dvi_start(struct dvi_inst *inst) {
	_dvi_load_dma_op(inst->dma_cfg, &inst->dma_list_vblank_nosync);
	dma_start_channel_mask(
		(1u << inst->dma_cfg[0].chan_ctrl) |
		(1u << inst->dma_cfg[1].chan_ctrl) |
		(1u << inst->dma_cfg[2].chan_ctrl));

	// We really don't want the FIFOs to bottom out, so wait for full before
	// starting the shift-out.
	for (int i = 0; i < N_TMDS_LANES; ++i)
		while (!pio_sm_is_tx_fifo_full(inst->ser_cfg.pio, inst->ser_cfg.sm_tmds[i]))
			tight_loop_contents();
	dvi_serialiser_enable(&inst->ser_cfg, true);
}

static inline void __dvi_func_x(_dvi_prepare_scanline_8bpp)(struct dvi_inst *inst, uint32_t *scanbuf) {
	uint32_t *tmdsbuf;
	queue_remove_blocking_u32(&inst->q_tmds_free, &tmdsbuf);
	uint pixwidth = inst->timing->h_active_pixels;
	uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
	// Scanline buffers are half-resolution; the functions take the number of *input* pixels as parameter.
	tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, DVI_8BPP_BLUE_MSB,  DVI_8BPP_BLUE_LSB );
	tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, DVI_8BPP_GREEN_MSB, DVI_8BPP_GREEN_LSB);
	tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, DVI_8BPP_RED_MSB,   DVI_8BPP_RED_LSB  );
	queue_add_blocking_u32(&inst->q_tmds_valid, &tmdsbuf);
}

static inline void __dvi_func_x(_dvi_prepare_scanline_16bpp)(struct dvi_inst *inst, uint32_t *scanbuf) {
	uint32_t *tmdsbuf;
	queue_remove_blocking_u32(&inst->q_tmds_free, &tmdsbuf);
	uint pixwidth = inst->timing->h_active_pixels;
	uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
	tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, DVI_16BPP_BLUE_MSB,  DVI_16BPP_BLUE_LSB );
	tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, DVI_16BPP_GREEN_MSB, DVI_16BPP_GREEN_LSB);
	tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, DVI_16BPP_RED_MSB,   DVI_16BPP_RED_LSB  );
	queue_add_blocking_u32(&inst->q_tmds_valid, &tmdsbuf);
}

// "Worker threads" for TMDS encoding (core enters and never returns, but still handles IRQs)

// Version where each record in q_colour_valid is one scanline:
void __dvi_func(dvi_scanbuf_main_8bpp)(struct dvi_inst *inst) {
	uint y = 0;
	while (1) {
		uint32_t *scanbuf;
		queue_remove_blocking_u32(&inst->q_colour_valid, &scanbuf);
		_dvi_prepare_scanline_8bpp(inst, scanbuf);
		queue_add_blocking_u32(&inst->q_colour_free, &scanbuf);
		++y;
		if (y == inst->timing->v_active_lines) {
			y = 0;
		}
	}
	__builtin_unreachable();
}

// Ugh copy/paste but it lets us garbage collect the TMDS stuff that is not being used from .scratch_x
void __dvi_func(dvi_scanbuf_main_16bpp)(struct dvi_inst *inst) {
	uint y = 0;
	while (1) {
		uint32_t *scanbuf;
		queue_remove_blocking_u32(&inst->q_colour_valid, &scanbuf);
		_dvi_prepare_scanline_16bpp(inst, scanbuf);
		queue_add_blocking_u32(&inst->q_colour_free, &scanbuf);
		++y;
		if (y == inst->timing->v_active_lines) {
			y = 0;
		}
	}
	__builtin_unreachable();
}

static void __dvi_func(dvi_dma_irq_handler)(struct dvi_inst *inst) {
#if DVI_ENABLE_IRQ_STATS
	// PATCH (irq-stats): see struct dvi_inst's own doc comment in dvi.h.
	// time_us_32() is a `static inline` single MMIO register read (no
	// cross-TU call, so no flash-residency risk -- see data_packet.h's "RAM
	// residency" note) chosen over get_absolute_time()/absolute_time_diff_us()
	// specifically to keep this handler's own overhead as close to zero as
	// the thing it's measuring, on a core with no scheduling slack at all.
	uint32_t irq_stats_start = time_us_32();
#endif
	// Every fourth interrupt marks the start of the horizontal active region. We
	// now have until the end of this region to generate DMA blocklist for next
	// scanline.
	dvi_timing_state_advance(inst->timing, &inst->timing_state);
	if (inst->tmds_buf_release && !queue_try_add_u32(&inst->q_tmds_free, &inst->tmds_buf_release))
		panic("TMDS free queue full in IRQ!");
	inst->tmds_buf_release = inst->tmds_buf_release_next;
	inst->tmds_buf_release_next = NULL;

	// Make sure all three channels have definitely loaded their last block
	// (should be within a few cycles of one another)
	for (int i = 0; i < N_TMDS_LANES; ++i) {
		while (dma_debug_hw->ch[inst->dma_cfg[i].chan_data].dbg_tcr != inst->timing->h_active_pixels / DVI_SYMBOLS_PER_WORD)
			tight_loop_contents();
	}

	uint32_t *tmdsbuf;
	while (inst->late_scanline_ctr > 0 && queue_try_remove_u32(&inst->q_tmds_valid, &tmdsbuf)) {
		// If we displayed this buffer then it would be in the wrong vertical
		// position on-screen. Just pass it back.
		queue_add_blocking_u32(&inst->q_tmds_free, &tmdsbuf);
		--inst->late_scanline_ctr;
	}

	if (inst->timing_state.v_state != DVI_STATE_ACTIVE) {
		// Don't care
		tmdsbuf = NULL;
	}
	else if (queue_try_peek_u32(&inst->q_tmds_valid, &tmdsbuf)) {
		if (inst->timing_state.v_ctr % DVI_VERTICAL_REPEAT == DVI_VERTICAL_REPEAT - 1) {
			queue_remove_blocking_u32(&inst->q_tmds_valid, &tmdsbuf);
			inst->tmds_buf_release_next = tmdsbuf;
		}
	}
	else {
		// No valid scanline was ready (generates solid red scanline)
		tmdsbuf = NULL;
		if (inst->timing_state.v_ctr % DVI_VERTICAL_REPEAT == DVI_VERTICAL_REPEAT - 1)
			++inst->late_scanline_ctr;
	}

	switch (inst->timing_state.v_state) {
		case DVI_STATE_ACTIVE:
			if (tmdsbuf) {
				dvi_update_scanline_data_dma(inst->timing, tmdsbuf, &inst->dma_list_active);
				_dvi_load_dma_op(inst->dma_cfg, &inst->dma_list_active);
			}
			else {
				_dvi_load_dma_op(inst->dma_cfg, &inst->dma_list_error);
			}
			if (inst->scanline_callback && inst->timing_state.v_ctr % DVI_VERTICAL_REPEAT == DVI_VERTICAL_REPEAT - 1) {
				inst->scanline_callback();
			}
			break;
		case DVI_STATE_SYNC:
			_dvi_load_dma_op(inst->dma_cfg, &inst->dma_list_vblank_sync);
#if DVI_ENABLE_AUDIO
			// PATCH (hdmi-audio): frame counter for InfoFrame alternation
			// (see dvi_update_data_packet_()'s use of it in dvi.c below).
			if (inst->timing_state.v_ctr == 0) {
				++inst->dvi_frame_count;
			}
#endif
			break;
		default:
			_dvi_load_dma_op(inst->dma_cfg, &inst->dma_list_vblank_nosync);
			break;
	}

#if DVI_ENABLE_AUDIO
	// PATCH (hdmi-audio): re-encode the data island for the scanline this
	// IRQ just loaded a DMA list for -- see dvi_update_data_packet()'s own
	// doc comment above. Unconditional: before dvi_set_audio_freq() has
	// ever been called, dvi_update_data_packet_() finds samples_per_frame
	// == 0 and this just re-encodes the same Null Packet every time.
	dvi_update_data_packet(inst);
#endif
#if DVI_ENABLE_IRQ_STATS
	// PATCH (irq-stats): unsigned subtraction stays correct even across a
	// time_us_32() wrap (~71 minutes) since both operands are the same
	// 32-bit width -- see struct dvi_inst's own doc comment in dvi.h.
	inst->irq_us_accum += time_us_32() - irq_stats_start;
#endif
}

static void __dvi_func(dvi_dma0_irq)() {
	struct dvi_inst *inst = dma_irq_privdata[0];
	dma_hw->ints0 = 1u << inst->dma_cfg[TMDS_SYNC_LANE].chan_data;
	dvi_dma_irq_handler(inst);
}

static void __dvi_func(dvi_dma1_irq)() {
	struct dvi_inst *inst = dma_irq_privdata[1];
	dma_hw->ints1 = 1u << inst->dma_cfg[TMDS_SYNC_LANE].chan_data;
	dvi_dma_irq_handler(inst);
}

#if DVI_ENABLE_AUDIO
// PATCH (hdmi-audio): HDMI data-island / audio API implementation, ported
// from rh1tech/frank-hdmi-audio's frank_dvi.c (BSD-3-Clause, same libdvi
// lineage as this file) -- see the matching declarations in dvi.h for what
// each function is for.

void dvi_audio_init(struct dvi_inst *inst) {
	inst->audio_freq = 0;
	inst->samples_per_frame = 0;
	inst->samples_per_line24 = 0;
	inst->audio_sample_pos = 0;
	inst->audio_frame_count = 0;

	// Point every list's data-island slot(s) at the one shared
	// next_data_stream buffer -- dvi_init() calls this only after the
	// lists themselves have already been built in the data-island shape,
	// and only before dvi_start()/core1 are reached, so there is no live
	// DMA chain to race against (see struct dvi_inst's own doc comment).
	dvi_update_data_island_ptr(&inst->dma_list_vblank_sync,   &inst->next_data_stream);
	dvi_update_data_island_ptr(&inst->dma_list_vblank_nosync, &inst->next_data_stream);
	dvi_update_data_island_ptr(&inst->dma_list_active,        &inst->next_data_stream);
	dvi_update_data_island_ptr(&inst->dma_list_error,         &inst->next_data_stream);

	// Pre-encode a Null Packet into next_data_stream right now, rather than
	// leaving it as raw zero words until the first dvi_dma_irq_handler()
	// invocation encodes something real: zero words aren't a defined TERC4
	// symbol at all, so the very first data-island slot any lane
	// transmits (which happens as soon as core1 starts, moments after this
	// returns) would otherwise carry invalid line content. A Null Packet
	// (CEA-861 header type 0x00) is the spec-defined "nothing to say yet"
	// packet -- exactly what data_packet_set_null() produces.
	{
		data_packet_t null_packet;
		data_packet_set_null(&null_packet);
		data_packet_encode(&inst->next_data_stream, &null_packet, false, false);
	}
}

void dvi_update_data_island_ptr(struct dvi_scanline_dma_list *dma_list, data_island_stream_t *stream) {
	for (int i = 0; i < N_TMDS_LANES; ++i) {
		dma_cb_t *cblist = dvi_lane_from_list(dma_list, i);
		uint32_t *src = stream->data[i];
		if (i == TMDS_SYNC_LANE) {
			cblist[DVI_SYNC_LANE_STATE_SYNC_DATA_ISLAND].read_addr = src;
		} else {
			cblist[DVI_NOSYNC_LANE_STATE_DATA_ISLAND].read_addr = src;
		}
	}
}

void dvi_audio_sample_buffer_set(struct dvi_inst *inst, audio_sample_t *buffer, int size) {
	audio_ring_set(&inst->audio_ring, buffer, size);
}

// video_freq: video sampling frequency
// audio_freq: audio sampling frequency
// CTS: Cycle Time Stamp
// N: HDMI Constant
// 128 * audio_freq = video_freq * N / CTS
void dvi_set_audio_freq(struct dvi_inst *inst, int audio_freq, int cts, int n) {
	inst->audio_freq = audio_freq;
	data_packet_set_audio_clock_regeneration(&inst->audio_clock_regeneration, cts, n);
	data_packet_set_audio_info_frame(&inst->audio_info_frame, audio_freq);

	uint32_t pixel_clock = dvi_timing_get_pixel_clock(inst->timing);
	uint32_t n_pix_per_frame = dvi_timing_get_pixels_per_frame(inst->timing);
	uint32_t n_pix_per_line = dvi_timing_get_pixels_per_line(inst->timing);

	uint64_t t = (uint64_t)audio_freq * n_pix_per_line * (uint64_t)0x1000000;
	inst->samples_per_line24 = (int)(t / pixel_clock);

	// PATCH (hdmi-audio): no DMA descriptor is touched by this function --
	// the data-island-carrying DMA shape was already built once in
	// dvi_init(), before dvi_start() (see struct dvi_inst's own doc
	// comment in dvi.h for why). This function only writes plain scalar
	// fields, but it still typically runs on a *different* core from
	// dvi_dma_irq_handler() (this project's core0 emulation loop calls it;
	// the IRQ runs wherever dvi_register_irqs_this_core() was called,
	// here core1) -- __dmb() orders every write above (ACR/InfoFrame
	// contents, samples_per_line24) before samples_per_frame below, the
	// field dvi_update_data_packet_() checks to decide whether real audio
	// is configured yet, becomes visible.
	__dmb();
	inst->samples_per_frame = (int)((uint64_t)audio_freq * n_pix_per_frame / pixel_clock);
}

// RAM-resident (__dvi_func), like every other function upstream libdvi puts
// on this path: this runs once per scanline inside dvi_dma_irq_handler() on
// core1 and must not fetch from flash -- see data_packet.h's "RAM residency"
// note for the real-hardware failure that leaving it in flash caused.
bool __dvi_func(dvi_update_data_packet_)(struct dvi_inst *inst, data_packet_t *packet) {
	if (inst->samples_per_frame == 0) {
		return false;
	}

	inst->audio_sample_pos += inst->samples_per_line24;
	// Bound the accumulator. It is normally drained every scanline by the
	// `-= n << 24` below, but a sustained producer underrun leaves n == 0
	// (see the clamp further down) and this would otherwise grow by
	// samples_per_line24 (~23.5M) at ~31.5k scanlines/sec -- signed
	// overflow, i.e. UB, within ~0.1s of silence. Capping instead of
	// accumulating is also the behavior we want: samples that were never
	// produced can't be played late, and letting a backlog build would only
	// discharge as a 4-frames-per-scanline burst once audio resumed.
	if (inst->audio_sample_pos > (4 << 24)) {
		inst->audio_sample_pos = 4 << 24;
	}

	if (inst->timing_state.v_state == DVI_STATE_FRONT_PORCH) {
		if (inst->timing_state.v_ctr == 0) {
			// Alternate the AVI and audio InfoFrames on successive frames --
			// each is only required "at least once per two video fields"
			// per CEA-861, and this halves how often either needs sending.
			if (inst->dvi_frame_count & 1) {
				*packet = inst->avi_info_frame;
			} else {
				*packet = inst->audio_info_frame;
			}
			return true;
		} else if (inst->timing_state.v_ctr == 1) {
			*packet = inst->audio_clock_regeneration;
			return true;
		}
	}

	int sample_pos = inst->audio_sample_pos >> 24;
	if (sample_pos < 0) {
		sample_pos = 0;
	}
	int n = sample_pos > 4 ? 4 : sample_pos;
	// PATCH (hdmi-audio): clamp to what the ring can actually supply.
	// data_packet_set_audio_sample() has no underrun check of its own --
	// calling it with n frames requested but fewer (or none) available
	// advances the ring's read pointer *past* its write pointer, which
	// corrupts audio_ring_get_write_size()'s arithmetic (it assumes read
	// never overtakes write) into returning a huge bogus "free space"
	// value. The next queue_audio_samples() call would then trust that
	// value and write past the end of the fixed-size static ring storage.
	// Underrun is entirely reachable here: the producer is core0's 50Hz
	// emulated-frame loop and the consumer is this ~31.5kHz scanline IRQ,
	// so any hitch on core0 empties the ring. If the producer hasn't kept
	// up, send a Null Packet this scanline (n stays 0) rather than
	// under-supplying.
	int available = (int)audio_ring_get_read_size(&inst->audio_ring);
	if (n > available) {
		n = available;
	}
	if (n > 0) {
		inst->audio_sample_pos -= n << 24;
		inst->audio_frame_count = data_packet_set_audio_sample(packet, &inst->audio_ring, n, inst->audio_frame_count);
		return true;
	}

	return false;
}
#endif

#if DVI_ENABLE_IRQ_STATS
// PATCH (irq-stats): plain read, no locking -- see struct dvi_inst's own doc
// comment in dvi.h for why that's safe. Not __dvi_func: called from core0
// (main_pico.cpp's stats line), not from the IRQ path itself.
uint32_t dvi_irq_us_accum(const struct dvi_inst *inst) {
	return inst->irq_us_accum;
}
#endif
