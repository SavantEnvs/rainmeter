// Leak detection is off for every target (ASan's memory-safety checks and UBSan stay active).
extern "C" int __lsan_is_turned_off() { return 1; }
