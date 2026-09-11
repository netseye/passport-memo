#include "../firmware/EE04_Demo/RefreshLogic.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <random>
using namespace EE04Refresh;
int main() {
  std::array<uint8_t,BYTES> old{},next{},canvas{};
  old.fill(255); next=old; canvas.fill(255);
  toPanel(canvas.data(),next.data());assert(old==next);
  // Logical top-left maps to native x=127,y=0; bottom-right to x=0,y=295.
  canvas[0]&=0x7f;canvas[BYTES-1]&=0xfe;
  toPanel(canvas.data(),next.data());assert(next[15]==254 && next[295*16]==127);
  assert(difference(old.data(),old.data()).w==0);
  auto r=difference(old.data(),next.data());assert(r.x==0&&r.y==0&&r.w==128&&r.h==296);
  next=old;next[50*16+3]^=8;
  r=difference(old.data(),next.data());assert(r.x==24&&r.y==50&&r.w==8&&r.h==1);
  // Replacing only the proposed aligned rectangle must reproduce the full new image.
  std::mt19937 rng(17);
  for(int trial=0;trial<200;++trial) {
    for(auto& b:old)b=uint8_t(rng());next=old;
    for(int n=0;n<20;++n)next[rng()%BYTES]^=uint8_t(rng());
    r=difference(old.data(),next.data());assert(r.x%8==0 && r.w%8==0);
    auto simulated=old;
    for(int y=r.y;y<r.y+r.h;++y)for(int b=r.x/8;b<(r.x+r.w)/8;++b)simulated[y*16+b]=next[y*16+b];
    assert(simulated==next);
  }
  Rect small{16,40,40,110},large{0,0,128,296};
  assert(choose(false,3,3,false,true,0,0,{})==Mode::Full);
  assert(choose(true,3,0,false,true,0,0,{})==Mode::Full);
  assert(choose(true,3,3,true,true,0,0,{})==Mode::Full);
  assert(choose(true,3,3,false,true,10,MAX_PARTIAL_AGE_MS,{})==Mode::Skip);
  for(int page=0;page<8;++page)assert(choose(true,page,page,false,true,0,0,small)==(page==0||page==3?Mode::Partial:Mode::Full));
  for(int n=0;n<10;++n)assert(choose(true,3,3,false,true,n,0,small)==Mode::Partial);
  assert(choose(true,3,3,false,true,10,0,small)==Mode::Full);
  assert(choose(true,3,3,false,true,0,MAX_PARTIAL_AGE_MS,small)==Mode::Full);
  assert(choose(true,3,3,false,true,0,0,large)==Mode::Full);
  assert(choose(true,3,3,false,false,0,0,small)==Mode::Full);
  assert(healthy(Mode::Partial,650,false));assert(!healthy(Mode::Partial,5000,false));
  assert(!healthy(Mode::Full,10000,false));assert(!healthy(Mode::Partial,650,true));
  assert(healthy(Mode::Full,4023,false));
  assert(!ready(1749,1000,Mode::Partial,false));assert(ready(1750,1000,Mode::Partial,false));
  assert(!ready(3999,1000,Mode::Full,false));assert(ready(4000,1000,Mode::Full,false));
  assert(ready(100,100,Mode::Full,true));assert(ready(100,100,Mode::Skip,false));
  assert(ready(2500,0xffffff00u,Mode::Partial,false));assert(!ready(2500,0xffffff00u,Mode::Full,false));
  puts("PASS refresh rotation/corners, aligned dirty rectangles, 200 framebuffer reconstructions, page policy, dedupe, maintenance, fault detection and wraparound scheduling");
}
