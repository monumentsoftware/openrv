#ifndef OPENRV_VNC_DES_H
#define OPENRV_VNC_DES_H

#include <stdint.h>
#include <stdlib.h>

class VncDES
{
public:
    static bool encrypt(uint8_t* response, const uint8_t* challenge, const char* password, size_t passwordLength);
};

#endif

