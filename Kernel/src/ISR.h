#pragma once

#define ISR(name, vectno) extern "C" void name();
#define ISRE(name, vectno) ISR(name, vectno)
#include "ISR.inc"

#undef ISR
#undef ISRE

#define ISR(name, vectno) vectno_##name = vectno,
#define ISRE(name, vectno) ISR(name, vectno)

enum ISRVectorNumbers
{
    #include "ISR.inc"
};

#undef ISR
#undef ISRE