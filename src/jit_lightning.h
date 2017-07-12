#ifndef _JIT_LIGHTNING_H_
#define _JIT_LIGHTNING_H_

#include <lightning/lightning.h>

#include "bus.h"

typedef void (*rkjit_native_code)(void);

struct rkjit_tag {
	bus_t *b;
};

struct rkjit_block_tag {
	uint16_t offset;
	rkjit_native_code generated;
};

typedef struct rkjit_tag rkjit_t;
typedef struct rkjit_block_tag rkjit_block_t;

rkjit_t rkjit_init(bus_t *b);
void rkjit_finish(rkjit_t *rj);
rkjit_block_t rkjit_block_recompile(rkjit_t *rj, uint16_t offset);

#endif /* _JIT_LIGHTNING_H_ */

