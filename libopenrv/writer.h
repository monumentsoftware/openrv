/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPENRV_WRITER_H
#define OPENRV_WRITER_H

#if !defined(_MSC_VER)
#include <arpa/inet.h>
#else // _MSC_VER
#include <WinSock2.h>
#endif // _MSC_VER
#include <string.h>

class Writer
{
public:
    static void writeUInt8(char* buffer, uint8_t v);
    static void writeUInt16(char* buffer, uint16_t v);
    static void writeUInt32(char* buffer, uint32_t v);
    static void writeInt32(char* buffer, int32_t v);
};

/**
 * @pre @p buffer is non-NULL and has space for at least 4 bytes
 **/
inline void Writer::writeUInt32(char* buffer, uint32_t v)
{
    uint32_t tmp = htonl(v);
    memcpy(buffer, &tmp, 4);
}

/**
 * @pre @p buffer is non-NULL and has space for at least 4 bytes
 **/
inline void Writer::writeInt32(char* buffer, int32_t v)
{
    int32_t tmp = (int32_t)htonl((uint32_t)v);
    memcpy(buffer, &tmp, 4);
}

/**
 * @pre @p buffer is non-NULL and has space for at least 2 bytes
 **/
inline void Writer::writeUInt16(char* buffer, uint16_t v)
{
    uint16_t tmp = htons(v);
    memcpy(buffer, &tmp, 2);
}

/**
 * @pre @p buffer is non-NULL
 **/
inline void Writer::writeUInt8(char* buffer, uint8_t v)
{
    buffer[0] = v;
}



#endif

