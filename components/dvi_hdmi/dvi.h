#ifndef _DVI_H
#define _DVI_H

#ifdef __cplusplus
extern "C" {
#endif

#define N_TMDS_LANES 3
#define TMDS_SYNC_LANE 0 // blue!

#include "pico/util/queue.h"

#include "dvi_config_defs.h"
#include "dvi_timing.h"
#include "dvi_serialiser.h"
#include "util_queue_u32_inline.h"
#if DVI_ENABLE_AUDIO
#include "data_packet.h"
#endif

typedef void (*dvi_callback_t)(void);

struct dvi_inst {
	// Config ---
	const struct dvi_timing *timing;
	struct dvi_lane_dma_cfg dma_cfg[N_TMDS_LANES];
	struct dvi_timing_state timing_state;
	struct dvi_serialiser_cfg ser_cfg;
	// Called in the DMA IRQ once per scanline -- careful with the run time!
	dvi_callback_t scanline_callback;

	// State ---
	struct dvi_scanline_dma_list dma_list_vblank_sync;
	struct dvi_scanline_dma_list dma_list_vblank_nosync;
	struct dvi_scanline_dma_list dma_list_active;
	struct dvi_scanline_dma_list dma_list_error;

	// After a TMDS buffer has been enqueue via a control block for the last
	// time, two IRQs must go by before freeing. The first indicates the control
	// block for this buf has been loaded, and the second occurs some time after
	// the actual data DMA transfer has completed.
	uint32_t *tmds_buf_release_next;
	uint32_t *tmds_buf_release;
	// Remember how far behind the source is on TMDS scanlines, so we can output
	// solid colour until they catch up (rather than dying spectacularly)
	uint late_scanline_ctr;

	// Encoded scanlines:
	queue_t q_tmds_valid;
	queue_t q_tmds_free;

	// Either scanline buffers or frame buffers:
	queue_t q_colour_valid;
	queue_t q_colour_free;

#if DVI_ENABLE_AUDIO
	// PATCH (hdmi-audio): HDMI data-island state -- see dvi_audio_init()/
	// dvi_set_audio_freq() below. Frame counter (incremented on every
	// SYNC-state entry in dvi_dma_irq_handler(), see its own PATCH comment)
	// alternates which InfoFrame goes out on alternate frames.
	//
	// Design note (why there is no runtime DMA-list rebuild anywhere in
	// this file): an earlier revision rebuilt dma_list_active/vblank_sync/
	// vblank_nosync/error from the plain shape to the data-island-carrying
	// shape *after* dvi_start() -- once as a direct call from
	// dvi_set_audio_freq() (a different core writing memory core1's live
	// DMA chain was reading), then deferred to dvi_dma_irq_handler()
	// itself. Every list is now instead built in the data-island-carrying
	// shape once, in dvi_init(), *before* dvi_start() or core1 are ever
	// reached, so no live structure is ever mutated. Only
	// dvi_set_audio_freq() still runs later (from the emulation core, once
	// a model is selected), and it only ever writes plain scalar fields
	// (frequency, ACR, InfoFrame contents below) -- a __dmb() before the
	// last one keeps that cross-core handoff correct without touching any
	// DMA descriptor.
	//
	// Honesty note on the above: neither runtime-rebuild variant was ever
	// shown to be the cause of a real failure. Both were written while
	// chasing a picture loss whose actual cause turned out to be unrelated
	// (flash-resident code on the core1 IRQ path -- see data_packet.h's
	// "RAM residency" note), and neither fixed it. Build-once-before-start
	// is kept because it is simply the better design -- mutating dma_cb_t
	// arrays that a DMA engine is autonomously walking needs a correctness
	// argument this driver does not otherwise need to make -- not because
	// the alternative was measured to be broken.
	uint dvi_frame_count;
	data_packet_t avi_info_frame;
	data_packet_t audio_clock_regeneration;
	data_packet_t audio_info_frame;
	int audio_freq;
	int samples_per_frame;
	// Fixed-point (24 fractional bits) samples-per-line accumulator step --
	// see dvi_set_audio_freq()'s doc comment for the derivation.
	int samples_per_line24;
	data_island_stream_t next_data_stream;
	audio_ring_t audio_ring;
	int audio_sample_pos;  // running accumulator, see dvi_update_data_packet_()
	int audio_frame_count; // 0-191, IEC 60958 channel-status block position
#endif

#if DVI_ENABLE_IRQ_STATS
	// PATCH (irq-stats): cumulative microseconds spent inside
	// dvi_dma_irq_handler()'s body, updated by core1 alone (dvi.c) every
	// time that IRQ fires. `volatile` and read cross-core with no locking
	// from dvi_irq_us_accum() below -- a plain aligned 32-bit load/store
	// never tears on Cortex-M33, and this is a display-only counter (same
	// tolerance PicoDviVideoOutput's own g_total_refresh_screen_us already
	// relies on). Wraps every ~71 minutes (2^32 us); callers must diff two
	// readings with unsigned subtraction, which stays correct across the
	// wrap.
	volatile uint32_t irq_us_accum;
#endif
};

// Set up data structures and hardware for DVI.
void dvi_init(struct dvi_inst *inst, uint spinlock_tmds_queue, uint spinlock_colour_queue);

// Call this after calling dvi_init(). DVI DMA interrupts will be routed to
// whichever core called this function. Registers an exclusive IRQ handler.
void dvi_register_irqs_this_core(struct dvi_inst *inst, uint irq_num);

// Start actually wiggling TMDS pairs. Call this once you have initialised the
// DVI, have registered the IRQs, and are producing rendered scanlines.
void dvi_start(struct dvi_inst *inst);

// TMDS encode worker function: core enters and doesn't leave, but still
// responds to IRQs. Repeatedly pop a scanline buffer from q_colour_valid,
// TMDS encode it, and pass it to the tmds valid queue.
void dvi_scanbuf_main_8bpp(struct dvi_inst *inst);
void dvi_scanbuf_main_16bpp(struct dvi_inst *inst);

// Same as above, but each q_colour_valid entry is a framebuffer
void dvi_framebuf_main_8bpp(struct dvi_inst *inst);
void dvi_framebuf_main_16bpp(struct dvi_inst *inst);

#if DVI_ENABLE_AUDIO
// PATCH (hdmi-audio): HDMI data-island / audio API. Every scanline DMA list
// is already built in the data-island-carrying shape by the time dvi_init()
// returns (see struct dvi_inst's own doc comment for why there is no
// runtime rebuild) -- so bring-up is just: dvi_audio_sample_buffer_set()
// (give it ring storage) then dvi_set_audio_freq() (declares the rate,
// writes the ACR/InfoFrame contents). Both are plain scalar/ring writes,
// safe to call from whichever core owns the emulation loop, at any time
// after dvi_init(). See PicoHdmiAudioOutput's own doc comment for this
// project's actual call site.

// Resets the audio sub-state to "no audio" and repoints every list's
// data-island DMA slot(s) at the shared next_data_stream buffer -- called
// once from dvi_init(), before dvi_start(); not normally called directly.
void dvi_audio_init(struct dvi_inst *inst);

// Repoints one scanline list's data-island DMA slot(s) at `stream` -- used
// once per list by dvi_audio_init(); not normally called directly.
void dvi_update_data_island_ptr(struct dvi_scanline_dma_list *dma_list, data_island_stream_t *stream);

// Hands the driver the storage for the audio sample ring (`buffer`, `size`
// frames, `size` a power of two). Call before dvi_set_audio_freq().
void dvi_audio_sample_buffer_set(struct dvi_inst *inst, audio_sample_t *buffer, int size);

// Declares the HDMI audio sample rate (Hz) and its CEA-861 clock-
// regeneration values (`cts`, `n`, satisfying `128 * audio_freq =
// pixel_clock * n / cts` -- pick `n` from the CEA-861 table for the chosen
// rate and derive `cts` from dvi_timing_get_pixel_clock(inst->timing)).
// Only ever writes scalar fields (audio_freq, samples_per_frame/line24, the
// ACR/InfoFrame data_packet_t contents) -- no DMA descriptor is touched, so
// this is safe to call from a different core than dvi_dma_irq_handler()'s
// own, at any time after dvi_init(). Before this is ever called,
// dvi_update_data_packet_() below finds samples_per_frame == 0 and every
// scanline's data island carries a spec-defined Null Packet instead.
void dvi_set_audio_freq(struct dvi_inst *inst, int audio_freq, int cts, int n);

// Fills `*packet` with whatever this scanline's data island should carry
// (an InfoFrame, the ACR packet, an audio-sample packet, or nothing) and
// returns whether `*packet` was actually written -- false means "send a
// null packet" (also what happens before dvi_set_audio_freq() has ever been
// called). Called from dvi_dma_irq_handler() every scanline; not normally
// called directly. Defined __dvi_func (RAM-resident) and everything it
// calls must be too -- see data_packet.h's "RAM residency" note.
bool dvi_update_data_packet_(struct dvi_inst *inst, data_packet_t *packet);
#endif

#if DVI_ENABLE_IRQ_STATS
// PATCH (irq-stats): reads inst->irq_us_accum (see struct dvi_inst's own doc
// comment) -- safe to call from either core, no locking, at any time.
uint32_t dvi_irq_us_accum(const struct dvi_inst *inst);
#endif

#ifdef __cplusplus
}
#endif

#endif
