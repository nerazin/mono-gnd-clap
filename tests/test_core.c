/* Regression test for mono_gnd.h that needs no firmware.
 *
 * The expected hashes come from Monomachine OS 1.32B itself: the tables were
 * read from the emulated DSP's memory, and the same stimulus was fed into the
 * firmware's GND SIN and GND NOIS machines on a synth track running in an
 * emulator (see the README), the AMP section starting idle and the track
 * filter's output replaced by the machine's output x 0.4455, as the plug-in
 * has it. The stimulus plays and releases notes over the whole range, moves
 * every control, including to the ends of its range and the portamento, the
 * pitch modulation (word 30) and the master tune (word 38), and changes the
 * tempo. Each block hashes the machine's left and right output and the AMP
 * section's left and right output.
 *
 * The main CPU's pitch words (master tune, pitch bend) are hashed against the
 * main OS's own tables: lab/pitch/maincpu_hash.py prints that hash from the
 * OS image. */
#include <stdio.h>
#include "../source/mono_gnd.h"

static uint32_t seed = 1;
static uint32_t rnd(uint32_t n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; }

/* The block words of one block: the SYN and AMP pages, the trig word (40),
 * the note word (41), the tick (42, 24 x BPM; word 35 is 0x800000 / tick),
 * the pitch modulation (30) and the master tune (38). Notes, the tempo and
 * those two change only every third block, where the Monomachine's CPU sends
 * them. */
typedef struct { int32_t syn[8], amp[8], trig, note, tick, pmod, mtune; } Words;
static void stimulus(int block, Words *w) {
   static const int amp_words[7] = {MG_ATK, MG_HOLD, MG_DEC, MG_REL, MG_VOL, MG_PAN, MG_PORT};
   if (block == 0) {
      memset(w, 0, sizeof *w);
      for (int k = 0; k < 8; ++k) w->syn[k] = 64 << 16;
      w->amp[MG_DEC] = w->amp[MG_REL] = w->amp[MG_VOL] = w->amp[MG_PAN] = 64 << 16;
      w->note = (60 << 11) / 12;
      w->tick = 2880;
      w->pmod = 64 << 16;
   }
   w->trig = block == 0 ? 0x80 : 0;   /* the machine's assignment */
   if (block % 23 == 0 && block) {                  /* knob-like moves, also between steps and to the ends */
      const int k = (int)rnd(15);
      const int32_t v = rnd(4) == 0 ? (rnd(2) ? 0x7fffff : 0) : (int32_t)rnd(0x7f0000);
      if (k < 8) w->syn[k] = v; else w->amp[amp_words[k - 8]] = k == 10 && v > 0x700000 ? 0x700000 : v;
   }
   if (block % 3 == 0 && block) {
      const uint32_t r = rnd(100);
      if (r < 6) { w->trig = 0x81; w->note = ((rnd(5) ? 24 + (int)rnd(85) : (int)rnd(128)) << 11) / 12; }
      else if (r < 10) w->trig = 0x02;
      else if (r < 11) w->tick = 720 + (int)rnd(6481);
      else if (r < 16) w->pmod = rnd(4) ? (64 << 16) + (int32_t)rnd(0x300000) - 0x180000 : (int32_t)rnd(0x7f0000);
      else if (r < 17) w->mtune = mg_master_tune(4000 + (int)rnd(1000));
   }
}

static uint64_t hash = 1469598103934665603ULL;
static void mix(const int32_t *v, int n) {
   for (int i = 0; i < n; ++i)
      for (int b = 0; b < 4; ++b) hash = (hash ^ (((uint32_t)v[i] >> (8 * b)) & 0xff)) * 1099511628211ULL;
}

#define TEST_BLOCKS 12000
#define FILTER_LEVEL 0x3905a5

#ifndef TEST_CORE_NO_MAIN
int main(void) {
   static const uint64_t want[MG_MACHINES] = {0x8b1600bd32688fb5ULL, 0xa0a9396ecbbca927ULL};
   const uint64_t want_tables = 0xbb31e5068d500935ULL, want_main = 0x3449da86dbec4126ULL;
   mg_tables();
   mix(mg_sine, 0x2000); mix(mg_exp, 0x800); mix(mg_attack, 128); mix(mg_decay, 129); mix(mg_port, 128); mix(mg_pink_c, 7);
   int fail = hash != want_tables;
   printf("tables %016llx\n", (unsigned long long)hash);
   hash = 1469598103934665603ULL;
   for (int t = 4000; t < 5000; ++t) { const int32_t v = mg_master_tune(t); mix(&v, 1); }
   for (int r = 0; r < 8; ++r) for (int v = 0; v < 256; ++v) { const int32_t b = mg_bend(v, r); mix(&b, 1); }
   fail |= hash != want_main;
   printf("main cpu %016llx\n", (unsigned long long)hash);
   for (int m = 0; m < MG_MACHINES; ++m) {
      MonoGnd s;
      MgAmp env = {3, 0, 0, {0, 0}};
      MgPitch glide;
      Words w;
      seed = 1 + (uint32_t)m;
      hash = 1469598103934665603ULL;
      mg_init(&s, m);
      for (int block = 0; block < TEST_BLOCKS; ++block) {
         stimulus(block, &w);
         if (block == 0) mg_pitch_reset(&glide, w.note);
         int32_t out[2][MG_FRAMES], g[2][MG_FRAMES], lr[2][MG_FRAMES];
         mg_block(&s, w.syn, mg_pitch(&glide, w.note, w.syn[7], w.amp[MG_PORT], w.tick, w.pmod, w.mtune), out);
         mg_amp(&env, w.trig & 0x7f, w.amp, 0x800000 / w.tick, g);
         for (int c = 0; c < 2; ++c)
            for (int i = 0; i < MG_FRAMES; ++i) lr[c][i] = mg_apply(g[c][i], mg_mpy(out[c][i], FILTER_LEVEL));
         mix(out[0], MG_FRAMES); mix(out[1], MG_FRAMES); mix(lr[0], MG_FRAMES); mix(lr[1], MG_FRAMES);
      }
      printf("machine %d audio %016llx\n", m, (unsigned long long)hash);
      fail |= hash != want[m];
   }
   if (fail) { puts("FAIL: output differs from the firmware"); return 1; }
   puts("PASS core tables and audio match the firmware");
   return 0;
}
#endif
