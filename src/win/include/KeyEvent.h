#pragma once

struct KeyInfo {
  UINT repeatCount : 16;
  UINT scanCode : 8;
  UINT isExtended : 1;
  UINT reserved : 4;
  UINT contextCode : 1;
  UINT prevKeyState : 1;
  UINT isKeyUp : 1;

  KeyInfo(LPARAM lparam) { *this = *reinterpret_cast<KeyInfo*>(&lparam); }

  operator UINT32() { return *reinterpret_cast<UINT32*>(this); }
};