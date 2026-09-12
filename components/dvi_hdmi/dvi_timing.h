#ifndef _DVI_TIMING_H
#define _DVI_TIMING_H

#include "hardware/dma.h"
#include "pico/util/queue.h"

#include "dvi.h"

struct dvi_timing {
	bool h_sync_polarity;
	uint h_front_porch;
	uint h_sync_width;
	uint h_back_porch;
	uint h_active_pixels;

	bool v_sync_polarity;
	uint v_front_porch;
	uint v_sync_width;
	uint v_back_porch;
	uint v_active_lines;

	uint bit_clk_khz;
};

enum dvi_line_state {
	DVI_STATE_FRONT_PORCH = 0,
	DVI_STATE_SYNC,
	DVI_STATE_BACK_PORCH,
	DVI_STATE_ACTIVE,
	DVI_STATE_COUNT
};

// PATCH (hdmi-audio): per-lane chunk shapes once a scanline carries a data
// island -- see dvi_setup_scanline_for_*_with_audio()'s doc comment in
// dvi_timing.c for the exact pixel budget (the data island rides inside the
// generous h_sync_width pulse, not the much tighter h_back_porch; the video
// preamble+guardband immediately before active video come out of
// h_back_porch instead). Lane 0 (sync lane) carries the packet header
// symbols plus the vsync/hsync-encoding control symbols; lanes 1/2 carry
// the packet subpacket symbols plus the CTL/preamble/guardband framing.
// Each enum's last named member is the chunk immediately before ACTIVE
// itself, so _COUNT (one past it) is exactly the number of DMA descriptor
// slots dvi_scanline_dma_list needs per lane, ACTIVE included.
enum dvi_sync_lane_state {
	DVI_SYNC_LANE_STATE_FRONT_PORCH,
	DVI_SYNC_LANE_STATE_SYNC_DATA_ISLAND, // leading guardband, header, trailing guardband
	DVI_SYNC_LANE_STATE_SYNC,             // remainder of the sync pulse
	DVI_SYNC_LANE_STATE_BACK_PORCH,
	DVI_SYNC_LANE_STATE_VIDEO_GUARDBAND,
	DVI_SYNC_LANE_STATE_VIDEO,
	DVI_SYNC_LANE_STATE_COUNT,
};
enum dvi_nosync_lane_state {
	DVI_NOSYNC_LANE_STATE_CTL0,
	DVI_NOSYNC_LANE_STATE_PREAMBLE_TO_DATA,
	DVI_NOSYNC_LANE_STATE_DATA_ISLAND, // leading guardband, packet, trailing guardband
	DVI_NOSYNC_LANE_STATE_CTL1,
	DVI_NOSYNC_LANE_STATE_PREAMBLE_TO_VIDEO,
	DVI_NOSYNC_LANE_STATE_VIDEO_GUARDBAND,
	DVI_NOSYNC_LANE_STATE_VIDEO,
	DVI_NOSYNC_LANE_STATE_COUNT,
};

struct dvi_timing_state {
	uint v_ctr;
	enum dvi_line_state v_state;
};

// This should map directly to DMA register layout, but more convenient types
// (also this really shouldn't be here... we don't have a dma_cb in the SDK
// because there are many valid formats due to aliases)
typedef struct dma_cb {
	const void *read_addr;
	void *write_addr;
	uint32_t transfer_count;
	dma_channel_config c;
} dma_cb_t;

static_assert(sizeof(dma_cb_t) == 4 * sizeof(uint32_t), "bad dma layout");
static_assert(__builtin_offsetof(dma_cb_t, c.ctrl) == __builtin_offsetof(dma_channel_hw_t, ctrl_trig), "bad dma layout");

// Fixed, *unconditional* plain-shape chunk counts -- always 4/2 regardless
// of DVI_ENABLE_AUDIO. dvi_setup_scanline_for_active()'s one definition
// (dvi_timing.c) always builds the plain (non-audio) shape; with
// DVI_ENABLE_AUDIO=1, dvi_init() calls dvi_setup_scanline_for_active_with_audio()
// instead (see struct dvi_inst's own doc comment in dvi.h for why there is
// no runtime switch between the two), so this function's own "which index
// is the active-video chunk" arithmetic must use *these* fixed values, not
// the DVI_SYNC_LANE_CHUNKS/DVI_NOSYNC_LANE_CHUNKS macros below (which grow
// under DVI_ENABLE_AUDIO for the *struct's array-sizing* purpose only) --
// conflating the two was a real, if now-historical, bug: an earlier
// revision called this function unconditionally (audio build or not) and
// let it read the grown macros, planting the active chunk's real tmdsbuf
// pointer at the audio shape's index (5/6) while the per-scanline repatch
// (dvi_update_scanline_data_dma()) wrote to the plain shape's index (3/1)
// instead -- confirmed on real hardware as a total, immediate loss of
// picture, not a subtle glitch.
#define DVI_SYNC_LANE_CHUNKS_PLAIN DVI_STATE_COUNT
#define DVI_NOSYNC_LANE_CHUNKS_PLAIN 2

// PATCH (hdmi-audio): with DVI_ENABLE_AUDIO off (the default), these are
// exactly the original values -- byte-for-byte the same dma_cb_t[] shapes
// as the unmodified vendored file. Only flipping the option grows them to
// fit a data island's CTL/preamble/guardband framing (see the
// dvi_sync_lane_state/dvi_nosync_lane_state enums above). Used for
// dvi_scanline_dma_list's own array sizes and by the *_with_audio()
// builders/dvi_update_scanline_data_dma()'s audio-path indices -- NOT by
// dvi_setup_scanline_for_active()'s own plain-shape arithmetic, which
// always uses the fixed _PLAIN constants above instead.
#if DVI_ENABLE_AUDIO
#define DVI_SYNC_LANE_CHUNKS DVI_SYNC_LANE_STATE_COUNT
#define DVI_NOSYNC_LANE_CHUNKS DVI_NOSYNC_LANE_STATE_COUNT
#else
#define DVI_SYNC_LANE_CHUNKS DVI_SYNC_LANE_CHUNKS_PLAIN
#define DVI_NOSYNC_LANE_CHUNKS DVI_NOSYNC_LANE_CHUNKS_PLAIN
#endif

struct dvi_scanline_dma_list {
	dma_cb_t l0[DVI_SYNC_LANE_CHUNKS];
	dma_cb_t l1[DVI_NOSYNC_LANE_CHUNKS];
	dma_cb_t l2[DVI_NOSYNC_LANE_CHUNKS];
};

static inline dma_cb_t* dvi_lane_from_list(struct dvi_scanline_dma_list *l, int i) {
	return i == 0 ? l->l0 : i == 1 ? l->l1 : l->l2;
}

// Each TMDS lane uses one DMA channel to transfer data to a PIO state
// machine, and another channel to load control blocks into this channel.
struct dvi_lane_dma_cfg {
	uint chan_ctrl;
	uint chan_data;
	void *tx_fifo;
	uint dreq;
};

// Note these are already converted to pseudo-differential representation
extern const uint32_t dvi_ctrl_syms[4];

extern const struct dvi_timing dvi_timing_640x480p_60hz;
extern const struct dvi_timing dvi_timing_720x480p_60hz;
extern const struct dvi_timing dvi_timing_800x480p_60hz;
extern const struct dvi_timing dvi_timing_800x600p_60hz;
extern const struct dvi_timing dvi_timing_960x540p_60hz;
extern const struct dvi_timing dvi_timing_1280x720p_30hz;

extern const struct dvi_timing dvi_timing_800x600p_reduced_60hz;
extern const struct dvi_timing dvi_timing_1280x720p_reduced_30hz;

void dvi_timing_state_init(struct dvi_timing_state *t);

void dvi_timing_state_advance(const struct dvi_timing *t, struct dvi_timing_state *s);

void dvi_scanline_dma_list_init(struct dvi_scanline_dma_list *dma_list);

void dvi_setup_scanline_for_vblank(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		bool vsync_asserted, struct dvi_scanline_dma_list *l);

// PATCH (hdmi-audio): `black` (false everywhere in this project -- see
// dvi.c's dvi_init(), which always passes false, same as upstream's own
// blank-scanline behavior) selects between the diagnostic red pattern and a
// plain black one when `tmdsbuf` is NULL. Only gated behind
// DVI_ENABLE_AUDIO so the off build keeps the exact original 4-argument
// signature -- see DVI_SYNC_LANE_CHUNKS's own doc comment for why that
// matters.
#if DVI_ENABLE_AUDIO
void dvi_setup_scanline_for_active(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l, bool black);
#else
void dvi_setup_scanline_for_active(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l);
#endif

// PATCH (hdmi-audio): with DVI_ENABLE_AUDIO on, dvi_init() builds every list
// in the *_with_audio() shape from the start (see struct dvi_inst's own doc
// comment in dvi.h for why -- no runtime shape switch exists any more), so
// this one definition (dvi_timing.c) always patches the right indices for
// whichever shape is actually compiled in -- no bool parameter needed.
void dvi_update_scanline_data_dma(const struct dvi_timing *t, const uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l);

#if DVI_ENABLE_AUDIO
// Audio-capable counterparts of dvi_setup_scanline_for_vblank()/
// dvi_setup_scanline_for_active() above -- same purpose, but build the
// larger DVI_*_LANE_CHUNKS shape with CTL/preamble/guardband framing around
// a data-island slot (see dvi_timing.c's doc comment on these for the exact
// pixel accounting). dvi_init() calls these directly (never the plain
// versions above) whenever DVI_ENABLE_AUDIO is on -- see struct dvi_inst's
// own doc comment in dvi.h for why there is no runtime switch between the
// two shapes.
void dvi_setup_scanline_for_vblank_with_audio(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		bool vsync_asserted, struct dvi_scanline_dma_list *l);

void dvi_setup_scanline_for_active_with_audio(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l, bool black);

// pixel_clock (Hz) and pixels-per-line/-frame, derived from `t` -- used by
// dvi_set_audio_freq() (dvi.h) to compute the CTS audio-clock-regeneration
// value and the samples-per-line accumulator step.
static inline uint32_t dvi_timing_get_pixel_clock(const struct dvi_timing *t) { return t->bit_clk_khz * 100; }
static inline uint32_t dvi_timing_get_pixels_per_line(const struct dvi_timing *t) {
	return t->h_front_porch + t->h_sync_width + t->h_back_porch + t->h_active_pixels;
}
static inline uint32_t dvi_timing_get_pixels_per_frame(const struct dvi_timing *t) {
	uint32_t h = t->v_front_porch + t->v_sync_width + t->v_back_porch + t->v_active_lines;
	return dvi_timing_get_pixels_per_line(t) * h;
}
#endif

#endif
