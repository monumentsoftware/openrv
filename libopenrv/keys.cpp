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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "key_xkeygen.h"
#include <libopenrv/orv_logging.h>

const int HASH_BUCKET_COUNT = 1024;

struct CodeEntry
{
    int mKey;
    int mValue;
};

class CodeHashTable
{
public:
    ~CodeHashTable()
    {
        free(mEntries);
    }

    int mEntryCounts[HASH_BUCKET_COUNT] = {};
    CodeEntry* mEntries[HASH_BUCKET_COUNT] = {};
};

static CodeHashTable* UnicodeToXKeyTable = nullptr;
static CodeHashTable* XKeyToUnicodeTable = nullptr;

static CodeHashTable* createHashTable(const struct orv_context_t* ctx, bool xkeyAsKey)
{
    ORV_DEBUG(ctx, "createHashTable");
    CodeHashTable* table = new CodeHashTable();

    int totalEntryCount = sizeof(xkey_defs) / sizeof(*xkey_defs);
    ORV_DEBUG(ctx, "totalEntryCount=%d", totalEntryCount);
    for (int i = 0; i < totalEntryCount; i++)
    {
        int bucket = xkeyAsKey ? xkey_defs[i].mXCode:xkey_defs[i].mUCode;
        bucket %= HASH_BUCKET_COUNT;
        table->mEntryCounts[bucket]++;
    }
    ORV_DEBUG(ctx, "AAA");

    table->mEntries[0] = (CodeEntry*)malloc(sizeof(CodeEntry) * totalEntryCount);
    int c = table->mEntryCounts[0];
    for (int i = 1; i < HASH_BUCKET_COUNT; i++)
    {
        table->mEntries[i] = table->mEntries[0] + c;
        c += table->mEntryCounts[i];
    }
    ORV_DEBUG(ctx, "BBB");

    memset(table->mEntryCounts, 0, sizeof(int) * HASH_BUCKET_COUNT);
    for (int i = 0; i < totalEntryCount; i++)
    {
        int key, value;
        if (xkeyAsKey)
        {
            key = xkey_defs[i].mXCode;
            value = xkey_defs[i].mUCode;
        }
        else
        {
            key = xkey_defs[i].mUCode;
            value = xkey_defs[i].mXCode;
        }

        int bucket = key % HASH_BUCKET_COUNT;
        table->mEntries[bucket][table->mEntryCounts[bucket]].mKey = key;
        table->mEntries[bucket][table->mEntryCounts[bucket]].mValue = value;
        table->mEntryCounts[bucket]++;
    }
    return table;
}

static void initHashTables(const struct orv_context_t* ctx)
{
    if (XKeyToUnicodeTable)
    {
        return;
    }
    XKeyToUnicodeTable = createHashTable(ctx, true);
    UnicodeToXKeyTable = createHashTable(ctx, false);
}
static int lookupKey(const CodeHashTable& table, int key)
{
    int bucket = key % HASH_BUCKET_COUNT;
    int c = table.mEntryCounts[bucket];
    for (int i = 0; i < c; i++)
    {
        if (table.mEntries[bucket][i].mKey == key)
        {
            return table.mEntries[bucket][i].mValue;
        }
    }
    return -1;
}

int unicodeToXKeyCode(const struct orv_context_t* ctx, int unicode)
{
    initHashTables(ctx);
    return lookupKey(*UnicodeToXKeyTable, unicode);
}
static int xkeyCodeToUnicode(const struct orv_context_t* ctx, int xkeyCode)
{
    initHashTables(ctx);
    return lookupKey(*XKeyToUnicodeTable, xkeyCode);
}

static int readUtf8Char(const char* str, int& ofs)
{
    int c0 = str[ofs];
    if (!c0)
    {
        return -1;
    }
    if ((c0 & 0x80) == 0)
    {
        ofs += 1;
        return c0 & 0x7f;
    }
    int c1 = str[ofs + 1] & 0x3f;
    if ((c0 & 0xe0) == 0xc0)
    {
        ofs += 2;
        return ((c0 & 0x1f) << 6) + c1;
    }
    int c2 = str[ofs + 2] & 0x3f;
    if ((c0 & 0xf0) == 0xe0)
    {
        ofs += 3;
        return ((c0 & 0x0f) << 12)  + (c1 << 6) + c2;
    }
    int c3 = str[ofs + 3] & 0x3f;
    if ((c0 & 0xf8) == 0xf0)
    {
        ofs += 4;
        return ((c0 & 0x07) << 18) + (c1 << 12) + (c2 << 6) + c3;
    }

    return -1;
}

int utf8CharsToXKeys(const struct orv_context_t* ctx, const char* chars, int* xkeys, int bufSize)
{
    ORV_DEBUG(ctx, "utf8CharsToXKeys(%s)\n", chars);
    int ofs = 0;
    int xkeyCount = 0;
    while (1)
    {
        int c = readUtf8Char(chars, ofs);
        ORV_DEBUG(ctx, "utf8char %d\n", c);
        if (c == -1)
        {
            break;
        }
        int xkey = unicodeToXKeyCode(ctx ,c);
        if (xkey != -1)
        {
            if (xkeyCount >= bufSize)
            {
                return -1;
            }
            xkeys[xkeyCount] = xkey;
            xkeyCount++;
        }
        ORV_DEBUG(ctx, "xkeycode: %d\n", xkey);
    }
    return xkeyCount;
}

