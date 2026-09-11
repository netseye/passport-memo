#include "../firmware/EE04_Demo/PassportProtocol.h"
#include <cassert>
#include <cstdio>
int main() {
  PassportChallenges challenges;
  const std::string a(32, 'a'), b(32, 'b');
  assert(challenges.match(a, 0) == -1);
  challenges.issue(a, 100);
  assert(challenges.match(a, 30099) >= 0);
  assert(challenges.match(a, 30100) == -1);
  assert(challenges.match(b, 100) == -1);
  challenges.consume(challenges.match(a, 100));
  assert(challenges.match(a, 101) == -1);
  challenges.issue(a, UINT32_MAX - 20);
  assert(challenges.match(a, 10) >= 0);
  assert(challenges.match(a, 30000) == -1);
  challenges.reset();
  assert(challenges.match(a, 10) == -1);
  for (char c = 'a'; c <= 'e'; ++c) challenges.issue(std::string(32,c), 100);
  assert(challenges.match(a, 100) == -1);
  assert(challenges.match(b, 100) >= 0);
  assert(passportCanonical(a, 42, true, "第一句\n第二句") ==
         "memo-v1\n" + a + "\n42\n1\n第一句\n第二句");
  puts("PASS Passport nonce expiry, replay rejection, rollover, eviction and signing bytes");
}
