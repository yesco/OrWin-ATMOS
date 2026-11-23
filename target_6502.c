#if defined(__CC65__)
    /* cc65 specific (e.g., #include <conio.h>, #ifdef __C64__) */
#elif defined(__OSCAR64__)
    /* Oscar64 specific (e.g., #include <c64/vic.h>) */
#elif defined(__mos__)
    /* llvm-mos specific */
#elif defined(__VBCC__)
    /* vbcc specific */
#elif defined(__SDCC)
    /* SDCC specific */
#elif defined(__linux__) || defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    #include <unistd.h>
    #ifdef _POSIX_VERSION
        /* Hosted POSIX / Linux / macOS code */
    #endif
#endif
#if defined(__CC65__) || defined(__OSCAR64__) || defined(__mos__) || \
    defined(__SDCC_mos6502) || defined(__SDCC_mos65c02) || defined(__KICKC__) || \
    defined(__M6502__)
  #define TARGET_6502 1
#endif
