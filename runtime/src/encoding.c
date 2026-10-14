#include "internal.h"
#include <ctype.h>

const char *enc_skip_qualifiers(const char *p) {
    while (*p && strchr("rnNoORVA", *p)) p++;
    return p;
}

static const char *skip_balanced(const char *p, char open, char close) {
    int depth = 0;
    do {
        if (*p == open) depth++;
        else if (*p == close) depth--;
        else if (*p == 0) return p;
        p++;
    } while (depth > 0);
    return p;
}

const char *enc_skip_type(const char *p) {
    p = enc_skip_qualifiers(p);
    switch (*p) {
    case 0: return p;
    case '{': return skip_balanced(p, '{', '}');
    case '(': return skip_balanced(p, '(', ')');
    case '[': return skip_balanced(p, '[', ']');
    case '^': return enc_skip_type(p + 1);
    case 'b': p++; while (isdigit((unsigned char)*p)) p++; return p;
    case '@':
        p++;
        if (*p == '?') return p + 1;
        if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; }
        return p;
    default: return p + 1;
    }
}

const char *enc_skip_offset(const char *p) {
    if (*p == '-' || *p == '+') p++;
    while (isdigit((unsigned char)*p)) p++;
    return p;
}

unsigned enc_arg_count(const char *types) {
    if (!types || !*types) return 0;
    const char *p = enc_skip_offset(enc_skip_type(types));
    unsigned n = 0;
    while (*p) { p = enc_skip_offset(enc_skip_type(p)); n++; }
    return n;
}

static bool enc_find_nth(const char *types, unsigned n, const char **begin, const char **end) {
    if (!types || !*types) return false;
    const char *p = types;
    for (unsigned i = 0; ; i++) {
        if (!*p) return false;
        const char *e = enc_skip_type(p);
        if (i == n) { *begin = p; *end = e; return true; }
        p = enc_skip_offset(e);
    }
}

char *enc_copy_nth(const char *types, unsigned n) {
    const char *b, *e;
    if (!enc_find_nth(types, n, &b, &e)) return NULL;
    size_t len = (size_t)(e - b);
    char *s = malloc(len + 1);
    memcpy(s, b, len); s[len] = 0;
    return s;
}

void enc_get_nth(const char *types, unsigned n, char *dst, size_t len) {
    if (!dst || !len) return;
    const char *b, *e;
    if (!enc_find_nth(types, n, &b, &e)) { dst[0] = 0; return; }
    size_t l = (size_t)(e - b);
    if (l >= len) l = len - 1;
    memcpy(dst, b, l); dst[l] = 0;
}
