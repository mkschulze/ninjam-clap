/*
    LICE stub - minimal declarations for vorbisencdec.h compilation
    We don't use LICE image loading for NINJAM audio codec
*/

#ifndef _LICE_H_
#define _LICE_H_

// Minimal LICE_IBitmap stub - never actually used in NINJAM codec path
class LICE_IBitmap {
public:
    virtual ~LICE_IBitmap() {}
    virtual int getWidth() { return 0; }
    virtual int getHeight() { return 0; }
};

#endif // _LICE_H_
