// Standalone checks for include/batteryLeds.hpp: clang++ -std=c++17 -Iinclude tests/batteryLeds_test.cpp
#include "batteryLeds.hpp"
#include <cstdio>
#include <initializer_list>
using B = BatteryLeds;
static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static B ready(uint8_t st, uint8_t pct) { B b; b.Update(true, st, pct, 0); return b; }
int main() {
  const int full[11] = {0,0,1,1,2,2,3,3,4,4,5};
  for (int p = 0; p <= 100; p += 10) CHECK(B::FullSteps(p) == full[p / 10]);
  { B b; CHECK(b.Update(false, 0, 80, 0).level == -1); CHECK(b.Update(true, 0, 0, 1000).level == -1); CHECK(b.Update(true, 0, 80, 3999).level == -1);
    B::Show s = b.Update(true, 0, 80, 4000); CHECK(s.level == 4 && s.dim == 0); }
  // unplugged above 20%: steady and bright
  const int steady[11] = {-9,-9,-9,1,2,2,3,3,4,4,5};
  for (int p = 30; p <= 100; p += 10) { B b = ready(0, p); for (int64_t t = 3000; t < 9000; t += 37) { B::Show s = b.Update(true, 0, p, t); CHECK(s.level == steady[p / 10] && s.dim == 0); } }
  // 20% or less unplugged: the centre never goes off, it pulses through all three brightnesses
  for (int p : {0, 10, 20}) { B b = ready(0, p); int seen[3] = {}; for (int64_t t = 3000; t < 9000; t += 10) { B::Show s = b.Update(true, 0, p, t); CHECK(s.level == 1); seen[s.dim]++; } CHECK(seen[0] && seen[1] && seen[2]); }
  // charging: current and next take turns, and the change happens while dim
  const int a[6][2] = {{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}};
  for (int p = 0; p <= 90; p += 10) { int f = B::FullSteps(p); B b = ready(1, p); int prev = -2, prevDim = 0; bool sawA = false, sawB = false;
    for (int64_t t = 3000; t < 12000; t += 10) { B::Show s = b.Update(true, 1, p, t); CHECK(s.level == a[f][0] || s.level == a[f][1]);
      sawA |= s.level == a[f][0]; sawB |= s.level == a[f][1];
      if (prev != -2 && s.level != prev) { CHECK(prevDim == 2 && s.dim == 2); }
      prev = s.level; prevDim = s.dim; }
    CHECK(sawA && sawB); }
  // charged (complete, or charging at 100%): all five stay lit and pulse
  for (uint8_t st : {(uint8_t)2, (uint8_t)1}) { B b = ready(st, 100); int seen[3] = {}; for (int64_t t = 3000; t < 9000; t += 10) { B::Show s = b.Update(true, st, 100, t); CHECK(s.level == 5); seen[s.dim]++; } CHECK(seen[0] && seen[1] && seen[2]); }
  // pulse and picture shapes
  CHECK(B::PulseDim(0) == 0 && B::PulseDim(499) == 0 && B::PulseDim(500) == 1 && B::PulseDim(750) == 2 && B::PulseDim(1249) == 2 && B::PulseDim(1250) == 1 && B::PulseDim(1500) == 0);
  CHECK(B::PictureDim(0) == 2 && B::PictureDim(300) == 1 && B::PictureDim(550) == 0 && B::PictureDim(949) == 0 && B::PictureDim(950) == 1 && B::PictureDim(1200) == 2 && B::PictureDim(1499) == 2 && B::PictureDim(1500) == 2);
  // error state / disconnect
  { B b = ready(0, 80); CHECK(b.Update(true, 0x0F, 0, 5000).level == -1); CHECK(b.Update(true, 0, 80, 5100).level == -1); CHECK(b.Update(true, 0, 80, 8200).level == 4); }
  printf("%d checks, %d failed\n", checks, fails); return fails != 0;
}
