// Standalone checks for include/batteryAlert.hpp: clang++ -std=c++17 -Iinclude tests/batteryAlert_test.cpp
#include "batteryAlert.hpp"
#include <cstdio>
#include <cstdlib>
using B = BatteryAlert; using S = B::Signal;
static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
const uint8_t D = B::POWER_DISCHARGING, C = B::POWER_CHARGING, F = B::POWER_COMPLETE;
static void run(B& a, int64_t& t, int64_t until, uint8_t st, uint8_t pct, bool conn = true) { for (; t < until; t += 10) a.Update(conn, st, pct, t); }
int main() {
  { B a; int64_t t = 1000; run(a, t, 1100, D, 0); run(a, t, 9000, D, 80);
    CHECK(a.CurrentSignal() == S::None); CHECK(!a.IsBlinking(t)); CHECK(!a.IsRumbling(t)); }
  { B a; int64_t t = 0; run(a, t, 2900, D, 10); CHECK(a.CurrentSignal() == S::None); CHECK(!a.IsRumbling(t));
    run(a, t, 3010, D, 10); CHECK(a.CurrentSignal() == S::Low); CHECK(a.IsRumbling(t)); int64_t s = t;
    CHECK(a.IsBlinking(s + 50));
    run(a, t, s + 700, D, 10); CHECK(!a.IsRumbling(t));
    int dark = 0, lit = 0; for (int64_t x = 0; x < 2000; x += 10) { if (a.IsBlinking(s + x)) { a.IsDarkPhase(s + x) ? dark++ : lit++; } }
    CHECK(dark > 60 && dark < 110); CHECK(lit > 60 && lit < 110);
    CHECK(!a.IsBlinking(s + 2500)); CHECK(!a.IsBlinking(s + 299000)); CHECK(a.IsBlinking(s + 300100)); CHECK(a.IsBlinking(s + 600100));
    run(a, t, s + 5000, D, 20); CHECK(a.CurrentSignal() == S::Low); CHECK(!a.IsRumbling(t));
    run(a, t, s + 6000, D, 10, false); CHECK(a.CurrentSignal() == S::None);
    run(a, t, s + 10000, D, 10); CHECK(a.CurrentSignal() == S::Low); CHECK(!a.IsRumbling(t));
    run(a, t, s + 11000, C, 10); CHECK(a.CurrentSignal() == S::None); CHECK(!a.IsBlinking(t));
    run(a, t, s + 12000, D, 10); run(a, t, s + 15500, D, 10); CHECK(a.CurrentSignal() == S::Low);
  }
  { B a; int64_t t = 0; run(a, t, 3000, D, 10); bool prev = false; for (; t < 20000; t += 10) { a.Update(true, C, 20, t); }
    a = B(); t = 0; int rumbles = 0; for (; t < 8000; t += 10) { a.Update(true, D, 10, t); bool r = a.IsRumbling(t); if (r && !prev) rumbles++; prev = r; } CHECK(rumbles == 1); }
  { B a; int64_t t = 0; run(a, t, 4000, F, 100); CHECK(a.CurrentSignal() == S::Full); CHECK(!a.IsRumbling(t)); run(a, t, 6000, C, 90); CHECK(a.CurrentSignal() == S::Full);
    run(a, t, 6100, F, 0); CHECK(a.CurrentSignal() == S::Full); run(a, t, 6200, D, 100); CHECK(a.CurrentSignal() == S::None); }
  { B a; int64_t t = 0; run(a, t, 5000, 0x0A, 0); CHECK(a.CurrentSignal() == S::None); run(a, t, 9000, 0x0F, 0); CHECK(a.CurrentSignal() == S::None); }
  { B a; int64_t t = 0; run(a, t, 2000, D, 10); run(a, t, 2100, D, 50); run(a, t, 4000, D, 10); CHECK(a.CurrentSignal() == S::None); run(a, t, 7000, D, 10); CHECK(a.CurrentSignal() == S::Low); }
  // low threshold moved to 20% (battery on the player LEDs)
  { B a; a.SetLowPercent(20); int64_t t = 0; run(a, t, 3010, D, 20); CHECK(a.CurrentSignal() == S::Low); CHECK(a.IsRumbling(t)); }
  { B a; int64_t t = 0; run(a, t, 5000, D, 20); CHECK(a.CurrentSignal() == S::None); }
  printf("%d checks, %d failed\n", checks, fails); return fails != 0;
}
