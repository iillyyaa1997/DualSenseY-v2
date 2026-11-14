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
  // unplugged steady levels: 30% -> 1, 40-50 -> 2, 60-70 -> 3, 80-90 -> 4, 100 -> 5
  const int steady[11] = {-9,-9,-9,1,2,2,3,3,4,4,5};
  for (int p = 30; p <= 100; p += 10) { B b = ready(0, p); for (int64_t t = 3000; t < 9000; t += 37) { B::Show s = b.Update(true, 0, p, t); CHECK(s.level == steady[p / 10] && s.dim == 0); } }
  // <=20% unplugged: centre blinks with fades
  for (int p : {0, 10, 20}) { B b = ready(0, p); int on = 0, off = 0, dimmed = 0; for (int64_t t = 3000; t < 13000; t += 10) { B::Show s = b.Update(true, 0, p, t); CHECK(s.level == 0 || s.level == 1); if (s.level == 1) { on++; if (s.dim) dimmed++; } else off++; } CHECK(on > 400 && off > 400 && dimmed > 100); }
  // charging: current <-> next
  const int a[6][2] = {{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}};
  for (int p = 0; p <= 90; p += 10) { int f = B::FullSteps(p); B b = ready(1, p); bool sawA = false, sawB = false; for (int64_t t = 3000; t < 9000; t += 10) { int l = b.Update(true, 1, p, t).level; CHECK(l == a[f][0] || l == a[f][1]); sawA |= l == a[f][0]; sawB |= l == a[f][1]; } CHECK(sawA && sawB); }
  // charged (complete, or charging at 100%): all five blink
  for (uint8_t st : {(uint8_t)2, (uint8_t)1}) { B b = ready(st, 100); bool five = false, none = false; for (int64_t t = 3000; t < 9000; t += 10) { int l = b.Update(true, st, 100, t).level; CHECK(l == 5 || l == 0); five |= l == 5; none |= l == 0; } CHECK(five && none); }
  // fade shape inside a half period
  CHECK(B::Dim(0) == 0 && B::Dim(899) == 0 && B::Dim(900) == 1 && B::Dim(1199) == 1 && B::Dim(1200) == 2 && B::Dim(1499) == 2 && B::Dim(1500) == 0);
  // error state / disconnect
  { B b = ready(0, 80); CHECK(b.Update(true, 0x0F, 0, 5000).level == -1); CHECK(b.Update(true, 0, 80, 5100).level == -1); CHECK(b.Update(true, 0, 80, 8200).level == 4); }
  printf("%d checks, %d failed\n", checks, fails); return fails != 0;
}
