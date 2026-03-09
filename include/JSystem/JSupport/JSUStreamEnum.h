#ifndef JSUSTREAMENUM_H
#define JSUSTREAMENUM_H

#ifdef TARGET_PC
/* On PC, SEEK_SET/SEEK_CUR/SEEK_END/EOF are macros from stdio.h.
   Undefine them so we can use them as enum values, then restore. */
#pragma push_macro("SEEK_SET")
#pragma push_macro("SEEK_CUR")
#pragma push_macro("SEEK_END")
#pragma push_macro("EOF")
#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#undef EOF
#endif

enum JSUStreamSeekFrom { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

enum EIoState { GOOD = 0, EOF = 1 };

#ifdef TARGET_PC
#pragma pop_macro("EOF")
#pragma pop_macro("SEEK_END")
#pragma pop_macro("SEEK_CUR")
#pragma pop_macro("SEEK_SET")
#endif

#endif
