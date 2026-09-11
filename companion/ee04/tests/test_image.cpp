#include "../firmware/EE04_Demo/ImageLogic.h"
#include <cassert>
#include <string>
#include <cstdio>
using namespace EE04Image;
int main() {
  assert(BYTES == 4736);
  Record image;
  assert(!valid(image));
  std::string hex(BYTES * 2, '0');
  assert(!decodeHex(hex.c_str(), hex.size()-1, image));
  assert(!decodeHex(hex.c_str(), hex.size()+1, image));
  hex[83]='g'; assert(!decodeHex(hex.c_str(), hex.size(), image));
  hex[83]='0'; hex.replace(0, 6, "80aaFF"); hex.replace(hex.size()-2,2,"01");
  assert(decodeHex(hex.c_str(),hex.size(),image));
  assert(image.pixels()[0]==0x80 && image.pixels()[1]==0xaa && image.pixels()[2]==0xff);
  assert(image.pixels()[BYTES-1]==1);
  assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926);
  seal(image,0x01020304); assert(valid(image)); assert(revision(image)==0x01020304);
  auto restored=image; assert(valid(restored));
  restored.pixels()[456]^=1; assert(!valid(restored));
  restored=image;restored.bytes[3]='2';assert(!valid(restored));
  seal(image,0); assert(valid(image) && revision(image)==1);
  puts("PASS image payload length/hex validation, packing, CRC32 reference, corruption/version rejection, persisted revision");
}
