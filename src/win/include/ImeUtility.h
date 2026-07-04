#pragma once
#include <string>

inline std::wstring get_ime_name() {
  LANGID langId = GetUserDefaultUILanguage();

  if (langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_HONGKONG) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_MACAU)) {
    return L"\u667A\u80FDABC";
  } else {
    return L"SmartABC";
  }
}