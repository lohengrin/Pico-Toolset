// Host tests: audio_synth sources, A2DP SBC quality arithmetic, HID report parsing.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "hid_report.h"
#include "pico_toolset/audio_synth.h"
#include "pico_toolset/sample_mixer.h"
#include "pico_toolset/bluetooth_a2dp.h"
#include "pico_toolset/bluetooth_hid.h"

using namespace pico_toolset;

static int g_failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

static void test_audio_synth() {
    ToneSource tone(Waveform::Sine, 440.0f, 0.5f);
    tone.prepare(44100, 2);
    int16_t pcm[2 * 441];
    tone.fill(pcm, 441); // 10 ms = 4.4 periods
    int peak = 0, crossings = 0;
    for (int i = 0; i < 441; ++i) {
        CHECK(pcm[2 * i] == pcm[2 * i + 1]); // identical channels
        peak = std::max(peak, std::abs(static_cast<int>(pcm[2 * i])));
        if (i && (pcm[2 * i - 2] < 0) != (pcm[2 * i] < 0)) ++crossings;
    }
    CHECK(peak > 15000 && peak <= 16400); // amplitude 0.5 of full scale
    CHECK(crossings >= 8 && crossings <= 10); // ~4.4 periods = ~8.8 zero crossings

    SilenceSource silence;
    silence.prepare(44100, 2);
    for (auto& v : pcm) v = 123;
    silence.fill(pcm, 441);
    for (auto v : pcm) CHECK(v == 0);

    const AudioNote notes[] = {{440, 20}, {0, 20}}; // a tone then a rest
    MelodySource melody(notes, 2);
    melody.prepare(44100, 1);
    int16_t mono[44100 / 25];
    melody.fill(mono, 44100 / 25); // 40 ms: both notes
    int loud_first = 0, loud_second = 0;
    for (int i = 0; i < 882; ++i) loud_first += std::abs(mono[i]) > 1000;
    for (int i = 882; i < 1764; ++i) loud_second += std::abs(mono[i]) > 1000;
    CHECK(loud_first > 300);
    CHECK(loud_second == 0);
}

static void test_a2dp_quality() {
    // The A2DP reference points: joint stereo 44.1 kHz, bitpool 53 = ~328 kbps, bitpool 25 = ~174 kbps.
    const uint32_t high = a2dp_sbc_bitrate_bps(44100, A2dpChannelMode::JointStereo, 53);
    CHECK(high > 325000 && high < 332000);
    const uint32_t low = a2dp_sbc_bitrate_bps(44100, A2dpChannelMode::JointStereo, 25);
    CHECK(low > 170000 && low < 177000); // measured on the hardware: 173 kbps
    const uint32_t mono = a2dp_sbc_bitrate_bps(44100, A2dpChannelMode::Mono, 16);
    CHECK(mono > 100000 && mono < 120000);
    CHECK(a2dp_sbc_bitrate_bps(48000, A2dpChannelMode::JointStereo, 53) > high);

    CHECK(a2dp_quality_config(A2dpQuality::High).bitpool == 53);
    CHECK(a2dp_quality_config(A2dpQuality::Low).bitpool == 25);
    CHECK(a2dp_quality_config(A2dpQuality::Mono).mode == A2dpChannelMode::Mono);
    CHECK(std::strcmp(a2dp_quality_name(A2dpQuality::Low), "Low") == 0);
    // Presets descend in bit rate.
    uint32_t prev = 0xFFFFFFFF;
    for (auto q : {A2dpQuality::High, A2dpQuality::Medium, A2dpQuality::Low, A2dpQuality::Mono, A2dpQuality::MonoLow}) {
        const auto c = a2dp_quality_config(q);
        const uint32_t r = a2dp_sbc_bitrate_bps(c.sample_rate, c.mode, c.bitpool);
        CHECK(r < prev);
        prev = r;
    }
}

// Standard boot-protocol descriptors.
static const uint8_t kKeyboard[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};
static const uint8_t kMouse[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00,
    0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30,
    0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0};

static void test_hid_report() {
    using namespace hid_report;
    CHECK(scan_kinds(kKeyboard, sizeof(kKeyboard)) == bt_hid_kind::Keyboard);
    CHECK(scan_kinds(kMouse, sizeof(kMouse)) == bt_hid_kind::Mouse);
    const uint8_t kinds = scan_kinds(kRawPadDescriptor, kRawPadDescriptorSize);
    CHECK(kinds & bt_hid_kind::Gamepad); // the built-in pad descriptor declares a gamepad collection

    { // 'a' pressed with left shift
        const uint8_t rep[] = {0x02, 0x00, 0x04, 0, 0, 0, 0, 0};
        const auto r = parse_input_report(kKeyboard, sizeof(kKeyboard), rep, sizeof(rep), false);
        CHECK(r.has_keyboard);
        CHECK(r.modifiers == 0x02);
        CHECK(r.key_bits[0] == (1u << 4)); // usage 4
    }
    { // mouse: left button, dx +5, dy -3, wheel +1
        const uint8_t rep[] = {0x01, 0x05, 0xFD, 0x01};
        const auto r = parse_input_report(kMouse, sizeof(kMouse), rep, sizeof(rep), true);
        CHECK(r.raw_buttons == 1);
        CHECK(r.dx == 5 && r.dy == -3 && r.wheel == 1);
    }
    { // Logitech-style mouse: 5 buttons, 12-bit X/Y, 8-bit wheel (signed values must be sign-extended)
        static const uint8_t desc[] = {
            0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00,
            0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x03, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30,
            0x09, 0x31, 0x16, 0x01, 0xF8, 0x26, 0xFF, 0x07, 0x75, 0x0C, 0x95, 0x02, 0x81, 0x06, 0x09, 0x38, 0x15, 0x81,
            0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06, 0xC0, 0xC0};
        CHECK(scan_kinds(desc, sizeof(desc)) == bt_hid_kind::Mouse);
        // buttons: left + forward; x = -5 (0xFFB), y = +300 (0x12C), wheel = -1
        const uint8_t rep[] = {0x11, 0xFB, 0xCF, 0x12, 0xFF};
        const auto r = parse_input_report(desc, sizeof(desc), rep, sizeof(rep), true);
        CHECK(r.raw_buttons == 0x11);
        CHECK(r.dx == -5);
        CHECK(r.dy == 300);
        CHECK(r.wheel == -1);
    }
    { // the logged "SFR Gamepad" report 03 | buttons | hat 0x0F (centred) | 4 axes | 2 triggers
        const uint8_t rep[] = {0x03, 0x00, 0x00, 0x0F, 0x19, 0x7E, 0x7F, 0x7F, 0x00, 0x00, 0x00};
        const auto r = parse_input_report(kRawPadDescriptor, kRawPadDescriptorSize, rep, sizeof(rep), false);
        CHECK(!r.has_keyboard);
        CHECK(r.hat == -1);
        CHECK(r.have[0] && r.axis[0] == 0x19);
        CHECK(r.have[1] && r.axis[1] == 0x7E);
        CHECK(r.axis[2] == 0x7F && r.axis[3] == 0x7F);
        CHECK(r.raw_buttons == 0);
    }
    { // button 1 pressed, hat pointing up-right
        const uint8_t rep[] = {0x03, 0x01, 0x00, 0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00};
        const auto r = parse_input_report(kRawPadDescriptor, kRawPadDescriptorSize, rep, sizeof(rep), false);
        CHECK(r.raw_buttons == 1);
        CHECK(r.hat == 1);
        CHECK(map_buttons(r.raw_buttons) & kBtA);
        const uint16_t d = hat_to_dpad(r.hat);
        CHECK((d & kBtUp) && (d & kBtRight) && !(d & kBtDown) && !(d & kBtLeft));
    }
    CHECK(hat_to_dpad(-1) == 0);
    CHECK(hat_to_dpad(4) == kBtDown);
    CHECK(hat_to_dpad(6) == kBtLeft);
    // Axis scaling: 8-bit unsigned passes through, signed ranges are shifted up.
    CHECK(norm_axis(0x80, 8, 0) == 0x80);
    CHECK(norm_axis(-128, 8, -128) == 0);
    CHECK(norm_axis(127, 8, -128) == 255);
    CHECK(norm_axis(0x3FF, 10, 0) == 255); // 10-bit trigger
}

static void test_sample_mixer() {
    // A rising ramp 128..227 at the output rate, full volume: output follows (s-128)*127.
    uint8_t ramp[100];
    for (int i = 0; i < 100; ++i) ramp[i] = static_cast<uint8_t>(128 + i);
    SampleMixer<2> mixer;
    mixer.set_rate(8000);
    CHECK(!mixer.active());
    CHECK(!mixer.start(1, ramp, 1, 8000, 127)); // too short
    CHECK(mixer.start(1, ramp, 100, 8000, 127));
    CHECK(mixer.active() && mixer.playing(1) && !mixer.playing(2));
    int32_t acc[10] = {};
    mixer.mix(acc, 10);
    for (int i = 0; i < 10; ++i) CHECK(acc[i] == i * 127);

    // Double speed skips every other sample; half volume halves the level.
    mixer.stop_all();
    CHECK(!mixer.active());
    CHECK(mixer.start(2, ramp, 100, 8000, 127, 2.0f));
    mixer.set_volume(2, 64);
    int32_t acc2[5] = {};
    mixer.mix(acc2, 5);
    for (int i = 0; i < 5; ++i) CHECK(acc2[i] == 2 * i * 64);

    // Half rate source (4 kHz into 8 kHz) interpolates: ramp climbs 0.5 per output sample.
    mixer.stop_all();
    CHECK(mixer.start(3, ramp, 100, 4000, 127));
    int32_t acc3[4] = {};
    mixer.mix(acc3, 4);
    CHECK(acc3[0] == 0 && acc3[2] == 1 * 127 && std::abs(acc3[1] - 63) <= 1);

    // Two voices sum; a third start steals the lowest handle; a short sample frees itself.
    mixer.stop_all();
    CHECK(mixer.start(10, ramp, 100, 8000, 127));
    CHECK(mixer.start(11, ramp, 100, 8000, 127));
    CHECK(mixer.start(12, ramp, 100, 8000, 127));
    CHECK(!mixer.playing(10) && mixer.playing(11) && mixer.playing(12));
    mixer.stop(11);
    mixer.stop(12);
    uint8_t blip[4] = {200, 200, 200, 200};
    CHECK(mixer.start(20, blip, 4, 8000, 127));
    int32_t acc4[8] = {};
    mixer.mix(acc4, 8);
    CHECK(acc4[0] == 72 * 127 && !mixer.active());

    // Changing the rate stops everything.
    CHECK(mixer.start(30, ramp, 100, 8000, 127));
    mixer.set_rate(16000);
    CHECK(!mixer.active());
}

int main() {
    test_audio_synth();
    test_sample_mixer();
    test_a2dp_quality();
    test_hid_report();
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all host tests passed\n");
    return 0;
}
