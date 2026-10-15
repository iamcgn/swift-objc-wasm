// Test-side wrappers over the SDK's <Block.h> (libobjc.a carries the Blocks runtime).
#include <Block.h>
static inline void *objc_test_block_copy(const void *block) { return _Block_copy(block); }
static inline void objc_test_block_release(const void *block) { _Block_release(block); }
