#include "internal.h"

/* Metadata sections are discovered through the __start_/__stop_ symbols wasm-ld
 * synthesizes for C-identifier section names. Each section is also declared here as
 * an empty retained section, so the symbols exist even when nothing was emitted. */
#define DECLARE_SECTION(name)                                                     \
    __asm__("\t.section " #name ",\"R\",@\n\t.p2align 2\n\t.text\n");             \
    extern void *__start_##name[] __attribute__((weak, visibility("hidden")));   \
    extern void *__stop_##name[] __attribute__((weak, visibility("hidden")));

DECLARE_SECTION(objc_selrefs)
DECLARE_SECTION(objc_classlist)
DECLARE_SECTION(objc_nlclslist)
DECLARE_SECTION(objc_catlist)
DECLARE_SECTION(objc_nlcatlist)
DECLARE_SECTION(objc_protolist)
DECLARE_SECTION(objc_protorefs)
DECLARE_SECTION(objc_classrefs)
DECLARE_SECTION(objc_superrefs)
DECLARE_SECTION(objc_stublist)
DECLARE_SECTION(objc_imageinfo)
DECLARE_SECTION(objc_clsrolist)

#define SECTION_BEGIN(name) ((void **)__start_##name)
#define SECTION_END(name)   ((void **)__stop_##name)

static bool images_loaded;
const int objc_runtime_init_anchor = 1;

void objc_runtime_init(void) {
    static bool done;
    if (done) return;
    done = true;
    selectors_init();
    class_table_init();
    protocol_table_init();
    hooks_init();
    refcount_init();
    weak_init();
    assoc_init();
    autorelease_init();
}

void objc_load_images(void) {
    if (images_loaded) return;
    images_loaded = true;

    for (void **p = SECTION_BEGIN(objc_selrefs); p < SECTION_END(objc_selrefs); p++)
        if (*p) *p = (void *)sel_intern((const char *)*p, false);

    for (void **p = SECTION_BEGIN(objc_classlist); p < SECTION_END(objc_classlist); p++) {
        Class cls = (Class)*p;
        if (!cls || cls_is_stub(cls)) continue;
        class_register_name(cls);
        class_track(cls);
    }

    for (void **p = SECTION_BEGIN(objc_protolist); p < SECTION_END(objc_protolist); p++)
        protocol_register((protocol_t *)*p);
    for (void **p = SECTION_BEGIN(objc_protorefs); p < SECTION_END(objc_protorefs); p++)
        if (*p) *p = protocol_canonical((protocol_t *)*p);

    for (void **p = SECTION_BEGIN(objc_catlist); p < SECTION_END(objc_catlist); p++)
        if (*p) class_load_category((category_t *)*p);

    for (void **p = SECTION_BEGIN(objc_nlclslist); p < SECTION_END(objc_nlclslist); p++)
        if (*p) class_realize((Class)*p);

    for (void **p = SECTION_BEGIN(objc_nlclslist); p < SECTION_END(objc_nlclslist); p++)
        if (*p) class_call_load((Class)*p);
    for (void **p = SECTION_BEGIN(objc_nlcatlist); p < SECTION_END(objc_nlcatlist); p++)
        if (*p) category_call_load((category_t *)*p);
}

__attribute__((constructor(101))) static void objc_init(void) {
    objc_runtime_init();
    objc_load_images();
}
