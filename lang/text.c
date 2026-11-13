/* MiniSharp - text helpers (see ms.h): strings stay UTF-8 in memory, and a char is one code point, so
 * "żółw".Length is 4 like in C#. Indexing a string of only ASCII is direct (ulen == len); otherwise the byte
 * offset is found by walking from the last looked-up position (s[i] in a loop costs O(1) per step). */
#include "ms.h"

int ms_utf8_decode(const char* str, int len, int* adv) {
    const unsigned char* s = (const unsigned char*)str;
    int c = s[0];
    *adv = 1;
    if (c < 0x80 || len < 2) return c;
    int n = c >= 0xF0 && c < 0xF8 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (!n || len < n + 1) return c;
    int cp = c & (0x3F >> n);
    for (int i = 1; i <= n; i++) {
        if ((s[i] & 0xC0) != 0x80) return c;               /* not a continuation: the byte stands alone */
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *adv = n + 1;
    return cp;
}

int ms_utf8_encode(int cp, char* out) {
    unsigned char* o = (unsigned char*)out;
    if (cp < 0 || cp > 0x10FFFF) cp = '?';
    if (cp < 0x80) { o[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) { o[0] = (unsigned char)(0xC0 | cp >> 6); o[1] = (unsigned char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        o[0] = (unsigned char)(0xE0 | cp >> 12); o[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    o[0] = (unsigned char)(0xF0 | cp >> 18); o[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); o[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}

int ms_utf8_count(const char* s, int len) {
    int n = 0, adv;
    for (int i = 0; i < len; i += adv) { ms_utf8_decode(s + i, len - i, &adv); n++; }
    return n;
}

int ms_str_length(ObjString* s) {
    if (s->ulen < 0) s->ulen = ms_utf8_count(s->chars, s->len);
    return s->ulen;
}

int ms_str_offset(ObjString* s, int i) {
    int n = ms_str_length(s);
    if (n == s->len) return i;                             /* only ASCII */
    if (i <= 0) return 0;
    if (i >= n) return s->len;
    int ci = 0, b = 0, adv;
    if (s->cacheI > 0 && s->cacheI <= i) { ci = s->cacheI; b = s->cacheB; }
    while (ci < i) { ms_utf8_decode(s->chars + b, s->len - b, &adv); b += adv; ci++; }
    s->cacheI = ci; s->cacheB = b;
    return b;
}

int ms_str_char_at(ObjString* s, int i) {
    int b = ms_str_offset(s, i), adv;
    return ms_utf8_decode(s->chars + b, s->len - b, &adv);
}

/* ---------- letters ---------- */
int ms_char_upper(int c) {
    if (c < 0x80) return c >= 'a' && c <= 'z' ? c - 32 : c;
    if ((c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 0x20;
    if (c == 0xFF) return 0x178;
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x131) return 'I';
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c : c - 1;   /* odd = capital */
        if (c == 0x130 || c == 0x138 || c == 0x149 || c == 0x17F) return c;
        return (c & 1) ? c - 1 : c;                                                                  /* even = capital */
    }
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return c - 0x20;      /* Greek */
    if (c >= 0x430 && c <= 0x44F) return c - 0x20;                    /* Cyrillic */
    if (c >= 0x450 && c <= 0x45F) return c - 0x50;
    return c;
}

int ms_char_lower(int c) {
    if (c < 0x80) return c >= 'A' && c <= 'Z' ? c + 32 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
    if (c == 0x178) return 0xFF;
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x130) return 'i';
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c + 1 : c;
        if (c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F) return c;
        return (c & 1) ? c : c + 1;
    }
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 0x20;
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;
    if (c >= 0x400 && c <= 0x40F) return c + 0x50;
    return c;
}

int ms_char_is_letter(int c) {
    if (c < 0x80) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (c == 0xAA || c == 0xB5 || c == 0xBA) return 1;
    if (c >= 0xC0 && c <= 0x24F) return c != 0xD7 && c != 0xF7;      /* Latin-1, Latin Extended-A and -B */
    if (c >= 0x370 && c <= 0x3FF) return c != 0x37E && c != 0x387;   /* Greek */
    if (c >= 0x400 && c <= 0x52F) return 1;                          /* Cyrillic */
    return 0;
}

int ms_char_is_space(int c) {
    return c == ' ' || (c >= 9 && c <= 13) || c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}
