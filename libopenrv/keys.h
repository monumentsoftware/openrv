#ifndef OPENRV_KEYS_H
#define OPENRV_KEYS_H

int unicodeToXKeyCode(const struct orv_context_t* ctx, int unicode);
int utf8CharsToXKeys(const struct orv_context_t* ctx, const char* chars, int* xkeys, int bufSize);

#endif
