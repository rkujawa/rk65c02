#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdbool.h>

#define ROM_LOAD_ADDR 0xC000

bool rom_start(rk65c02emu_t *, const char *, const atf_tc_t *);
bool rom_start_with_jit(rk65c02emu_t *, const char *, const atf_tc_t *, bool);
const char * rom_path(const char *, const atf_tc_t *);

/* Require atf-c.h before utils.h when using this macro. */
#define ATF_TC_JIT_VARIANTS(tc_name, body_func) \
	ATF_TC_WITHOUT_HEAD(tc_name); \
	ATF_TC_BODY(tc_name, tc) { body_func(tc, false); } \
	ATF_TC_WITHOUT_HEAD(tc_name##_jit); \
	ATF_TC_BODY(tc_name##_jit, tc) { body_func(tc, true); }

#endif /* _UTILS_H_ */
