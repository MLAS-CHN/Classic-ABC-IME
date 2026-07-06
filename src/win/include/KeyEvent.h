#pragma once
#include <cstring>

struct KeyInfo {
  UINT repeatCount : 16;
  UINT scanCode : 8;
  UINT isExtended : 1;
  UINT reserved : 4;
  UINT contextCode : 1;
  UINT prevKeyState : 1;
  UINT isKeyUp : 1;

  KeyInfo(LPARAM lparam) { memcpy(this, &lparam, sizeof(lparam)); }

  operator UINT32() { UINT32 v; memcpy(&v, this, sizeof(v)); return v; }
};