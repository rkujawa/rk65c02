#include "jit_lightning.h"


rkjit_t
rkjit_init(bus_t *b) 
{
	rkjit_t rj;

	rj.b = b;

	init_jit(NULL);

	return rj;
}

void
rkjit_finish(rkjit_t *rj)
{
	finish_jit();
}

rkjit_block_t
rkjit_block_recompile(rkjit_t *rj, uint16_t offset)
{
	jit_state_t *_jit;
	rkjit_block_t jb;

	jb.offset = offset;

	_jit = jit_new_state();
	jit_prolog();
	jit_ret();
	jit_epilog();	

	jb.generated = jit_emit();

	jit_clear_state();
	jit_disassemble();

	jit_destroy_state();

	return jb;
}

/*bool
rkjit_nblock_analyze(rkjit_nblock *, uint16_t offset) 
{

}*/

