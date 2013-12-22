#include "config.h"
#include "dill.h"
#include "dill_internal.h"
#include "virtual.h"
#include "assert.h"
#include <stdarg.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PREG(ec, reg) ((reg < 100) ? &ec->p[reg] : &ec->r[reg-100])

static void run_emulation(dill_exec_ctx ec);

#include "vm_include.c"
#ifdef BUILD_EMULATOR
#include <ffi.h>

static
void emu_func(ffi_cif *cif, void*vret, void* args[], void *client_data)
{
    int i;
    dill_stream c = client_data;
    int param_count = c->p->save_param_count;
    dill_exec_ctx ec;

    if ((param_count >= 1) && (c->p->c_param_args[0].type == DILL_EC)) {
	ec = *(dill_exec_ctx*)args[0];
    } else {
	/* create an execution context to use */
	ec = dill_get_exec_context(c);
    }	
    if (!ec->p) {
	ec->p = malloc(param_count * sizeof(struct reg_type));
    }
    for (i= 1; i < param_count; i++) {
	switch (c->p->c_param_args[i].type) {
	case DILL_C:
	    ec->p[i].u.c.c = *((char **)args)[i];
	    break;
	case DILL_UC:
	    ec->p[i].u.uc.uc = *((unsigned char **)args)[i];
	    break;
	case DILL_S:
	    ec->p[i].u.s.s = *((short **)args)[i];
	    break;
	case DILL_US:
	    ec->p[i].u.us.us = *((unsigned short **)args)[i];
	    break;
	case DILL_I:
	    ec->p[i].u.i.i = *((int **)args)[i];
	    break;
	case DILL_U:
	    ec->p[i].u.u.u = *((unsigned int **)args)[i];
	    break;
	case DILL_L:
	    ec->p[i].u.l.l = *((long **)args)[i];
	    break;
	case DILL_UL:
	    ec->p[i].u.ul.ul = *((unsigned long **)args)[i];
	    break;
	case DILL_F:
	    ec->p[i].u.f.f = *((float **)args)[i];
	    break;
	case DILL_D:
	    ec->p[i].u.d.d = *((double **)args)[i];
	    break;
	case DILL_P:
	case DILL_EC:
	    ec->p[i].u.p.p = *((void ***)args)[i];
	    break;
	}
    }
    /* execute the function */
    run_emulation(ec);
    /* do return */
    switch(c->p->ret_type) {
    case DILL_C:
	*((char *)vret) = (PREG(ec, ec->ret_reg)->u.c.c);
	break;
    case DILL_UC:
	*((unsigned char *)vret) = (PREG(ec, ec->ret_reg)->u.uc.uc);
	break;
    case DILL_S:
	*((short *)vret) = (PREG(ec, ec->ret_reg)->u.s.s);
	break;
    case DILL_US:
	*((unsigned short *)vret) = (PREG(ec, ec->ret_reg)->u.us.us);
	break;
    case DILL_I:
	*((int *)vret) = (PREG(ec, ec->ret_reg)->u.i.i);
	break;
    case DILL_U:
	*((unsigned int *)vret) = (PREG(ec, ec->ret_reg)->u.u.u);
	break;
    case DILL_L:
	*((long *)vret) = (PREG(ec, ec->ret_reg)->u.l.l);
	break;
    case DILL_UL:
	*((unsigned long *)vret) = (PREG(ec, ec->ret_reg)->u.ul.ul);
	break;
    case DILL_F:
	*((float *)vret) = (PREG(ec, ec->ret_reg)->u.f.f);
	break;
    case DILL_D:
	*((double *)vret) = (PREG(ec, ec->ret_reg)->u.d.d);
	break;
    case DILL_P:
	*((void**)vret) = (PREG(ec, ec->ret_reg)->u.p.p);
	break;
    }
}

extern void *
emulate_clone_code(c, new_base, available_size)
dill_stream c;
void *new_base;
int available_size;
{
    return c->p->fp;
}

     /* Acts like puts with the file given at time of enclosure. */
     void puts_binding(ffi_cif *cif, unsigned int *ret, void* args[],
                       FILE *stream)
     {
	 printf("We're in the puts binding, args %p, %d, %d\n", *((char***)args)[0], *((int**)args)[1], *((int**)args)[2]);
     }

void
setup_VM_proc(dill_stream c)
{
     ffi_cif *cifp = malloc(sizeof(ffi_cif));
    ffi_type **args = NULL;
    ffi_closure *closure;
    void *func;
    ffi_type *ret_type;
    void *ret_addr = NULL;
    int i;

    c->p->fp = NULL;
    c->j->clone_code = emulate_clone_code;
    /* Allocate closure and func */
    closure = ffi_closure_alloc(sizeof(ffi_closure), &func);

    switch (c->p->ret_type) {
    case DILL_C:
	ret_type = &ffi_type_sint8;
	break;
    case DILL_UC:
	ret_type = &ffi_type_uint8;
	break;
    case DILL_S:
	ret_type = &ffi_type_sint16;
	break;
    case DILL_US:
	ret_type = &ffi_type_uint16;
	break;
    case DILL_I:
	ret_type = &ffi_type_sint32;
	break;
    case DILL_U:
	ret_type = &ffi_type_uint32;
	break;
    case DILL_L:
	ret_type = &ffi_type_sint64;
	break;
    case DILL_UL:
	ret_type = &ffi_type_uint64;
	break;
    case DILL_P:
	ret_type = &ffi_type_pointer;
	break;
    case DILL_F:
	ret_type = &ffi_type_float;
	break;
    case DILL_D:
	ret_type = &ffi_type_double;
	break;
    case DILL_V:
	ret_type = &ffi_type_void;
	break;
    }
    args = malloc(c->p->c_param_count * sizeof(args[0]));

    for (i=0; i < c->p->c_param_count; i++) {
	switch(c->p->c_param_args[i].type) {
	case DILL_C:
	    args[i] = &ffi_type_sint8;
	    break;
	case DILL_UC:
	    args[i] = &ffi_type_uint8;
	    break;
	case DILL_S:
	    args[i] = &ffi_type_sint16;
	    break;
	case DILL_US:
	    args[i] = &ffi_type_uint16;
	    break;
	case DILL_I:
	    args[i] = &ffi_type_sint32;
	    break;
	case DILL_U:
	    args[i] = &ffi_type_uint32;
	    break;
	case DILL_L:
	    args[i] = &ffi_type_sint64;
	    break;
	case DILL_UL:
	    args[i] = &ffi_type_uint64;
	    break;
	case DILL_EC:
	case DILL_P:
	    args[i] = &ffi_type_pointer;
	    break;
	case DILL_F:
	    args[i] = &ffi_type_float;
	    break;
	case DILL_D:
	    args[i] = &ffi_type_double;
	    break;
	case DILL_V:
	    args[i] = &ffi_type_void;
	}
    }
    if (!ffi_prep_cif(cifp, FFI_DEFAULT_ABI, c->p->c_param_count,
		      ret_type, args) == FFI_OK) {
	return;
    }
    if (!ffi_prep_closure_loc(closure, cifp, emu_func,
			      c, func) == FFI_OK) {

	return;
    }
    c->p->fp = func;
}

static void run_emulation(dill_exec_ctx ec)
{
    dill_stream c = ec->dc;
    void *insns = c->p->code_base;
    virtual_insn *ip = &((virtual_insn *)insns)[0];

    while (1) {
	struct reg_type *pused[3];
	struct reg_type *pdest;
	int loc;
	int insn_code;
	insn_code = ip->insn_code;
	loc = ((char*)ip - (char*)insns);
	if (c->dill_debug) {
	    printf("   v    loc(%d)  ", loc);
	    virtual_print_insn(c, NULL, ip);
	    printf("\n");
	}
	switch(ip->class_code) {
	case iclass_arith3: {
	    /* arith 3 operand integer insns */
	    int r0 = ip->opnds.a3.src1;
	    int r1 = ip->opnds.a3.src2;
	    int d = ip->opnds.a3.dest;
	    pused[0] = PREG(ec, r0);
	    pused[1] = PREG(ec, r1);
	    pdest = PREG(ec, d);
	    emulate_arith3(insn_code, pdest, pused[0], pused[1]);
	    break;
	}
	case iclass_arith2: {
	    /* arith 2 operand integer insns */
	    int r0 = ip->opnds.a2.src;
	    int d = ip->opnds.a2.dest;
	    pused[0] = PREG(ec, r0);
	    pdest = PREG(ec, d);
	    emulate_arith2(insn_code, pdest, pused[0]);
	    break;
	}
	case iclass_arith3i:{
	    /* arith 3 immediate operand integer insns */
	    int r0 = ip->opnds.a3i.src;
	    int d = ip->opnds.a3i.dest;
	    pused[0] = PREG(ec, r0);
	    pdest = PREG(ec, d);
	    emulate_arith3i(insn_code, pdest, pused[0], ip->opnds.a3i.u.imm);
	    break;
	}
	case iclass_ret:
	    ec->ret_reg = ip->opnds.a1.src;
	    return;
	    break;
	case iclass_convert: {
	    int r0 = ip->opnds.a2.src;
	    int d = ip->opnds.a2.dest;
	    pused[0] = PREG(ec, r0);
	    pdest = PREG(ec, d);
	    emulate_convert(ip->insn_code & 0xff, pdest, pused[0]);
	    break;
	}
	case iclass_loadstore: {
	    /* load store immediate operand integer insns */
	    pdest = PREG(ec, ip->opnds.a3.dest);
	    pused[0] = PREG(ec, ip->opnds.a3.src1);
	    pused[1] = PREG(ec, ip->opnds.a3.src2);
	    if ((ip->insn_code >> 4) == 0) {
		emulate_loadi(ip->insn_code & 0xf,
			      pdest, pused[0],
			      pused[1]->u.l.l);
	    } else {
		/* a store, dest is the source of the store */
		emulate_storei(ip->insn_code & 0xf,
			       pdest, pused[0],
			       pused[1]->u.l.l);
	    }
	    break;
	}
	case iclass_lea: {
	    int offset = ip->opnds.a3i.u.imm;
	    pused[0] = PREG(ec, ip->opnds.a3i.src);
	    pdest = PREG(ec, ip->opnds.a3i.dest);
	    pdest->u.p.p = ((char*)pused[0]->u.p.p) + offset;
	    break;
	}
	case iclass_loadstorei:
	    /* load store immediate operand integer insns */
	    pdest = PREG(ec, ip->opnds.a3i.dest);
	    pused[0] = PREG(ec, ip->opnds.a3i.src);
	    if ((ip->insn_code >> 4) == 0) {
		emulate_loadi(ip->insn_code & 0xf,
			       pdest, pused[0],
			       ip->opnds.a3i.u.imm);
	    } else {
		/* a store, dest is the source of the store */
		emulate_storei(ip->insn_code & 0xf,
			       pdest, pused[0],
			       ip->opnds.a3i.u.imm);
	    }
	    break;
	case iclass_set: {
	    pdest = PREG(ec, ip->opnds.a3i.dest);
	    pdest->u.l.l = ip->opnds.a3i.u.imm;
	    break;
	}
	case iclass_setf: {
	    pdest = PREG(ec, ip->opnds.sf.dest);
	    if ((ip->insn_code & 0xf) == DILL_F) {
		pdest->u.f.f = (float) ip->opnds.sf.imm;
	    } else {
		pdest->u.d.d = ip->opnds.sf.imm;
	    }
	    break;
	}
	case iclass_mov: {
	    pdest = PREG(ec, ip->opnds.a2.dest);
	    pused[0] = PREG(ec, ip->opnds.a2.src);

	    pdest->u = pused[0]->u;
	    break;
	}
	case iclass_reti: 
	    /* return immediate integer insns */
	    /* arbitrarily destroy reg 100 and return it */
	    ec->ret_reg = 100;
//	    switch(ip->insn_code & 0xf) {
	    PREG(ec, ec->ret_reg)->u.l.l = ip->opnds.a3i.u.imm;
	    return;
	case iclass_branch:
	{
	    /* branch */
	    int br_op = ip->insn_code;
	    int r0 = ip->opnds.br.src1;
	    int r1 = ip->opnds.br.src2;
	    pused[0] = PREG(ec, r0);
	    pused[1] = PREG(ec, r1);
	    if (emulate_branch(br_op, pused[0], pused[1])) {
		ip = (void*)(((char *)(&((virtual_insn *)insns)[-1])) + c->p->branch_table.label_locs[ip->opnds.br.label]);
	    }
	}
	break;
	case iclass_branchi:
	{
	    /* branch immediate */
	    int br_op = ip->insn_code;
	    int r0 = ip->opnds.bri.src;
	    pused[0] = PREG(ec, r0);
	    if (emulate_branchi(br_op, pused[0], ip->opnds.bri.imm_l)) {
		ip = (void*)(((char *)(&((virtual_insn *)insns)[-1])) + c->p->branch_table.label_locs[ip->opnds.bri.label]);
	    }
	}
	break;
	case iclass_jump_to_label:
	    ip = (void*)(((char *)(&((virtual_insn *)insns)[-1])) + c->p->branch_table.label_locs[ip->opnds.br.label]);
	    break;
	break;
	case iclass_jump_to_reg:
	    printf("Unimpl13\n");
	    /*	    dill_jp(c, pused[0]);*/
	    break;
	case iclass_jump_to_imm:
	    printf("Unimpl14\n");
	    dill_jpi(c, ip->opnds.bri.imm_a);
		break;
	case iclass_special:
	    printf("Unimpl15\n");
	    dill_special(c, ip->opnds.spec.type, ip->opnds.spec.param);
	    break;
	case iclass_call:
	{
	    int i;
	    void* imm = ip->opnds.bri.imm_a;
	    int ret_reg = ip->opnds.bri.src;
	    int reg = ip->insn_code & 0x10;
	    int typ = ip->insn_code & 0xf;
	    ffi_type **args;
	    void **values;
	    void *func;
	    ffi_type *ret_type;
	    void *ret_addr = NULL;
	    ffi_cif cif;
	    pused[0] = PREG(ec, ret_reg);
	    if (reg != 0) {
		func = PREG(ec, (long)imm)->u.p.p;
	    } else {
		func = (void*)imm;
	    }
	    switch(typ) {
	    case DILL_C:
		ret_type = &ffi_type_sint8;
		ret_addr = &pused[0]->u.c.c;
		break;
	    case DILL_UC:
		ret_type = &ffi_type_uint8;
		ret_addr = &pused[0]->u.uc.uc;
		break;
	    case DILL_S:
		ret_type = &ffi_type_sint16;
		ret_addr = &pused[0]->u.s.s;
		break;
	    case DILL_US:
		ret_type = &ffi_type_uint16;
		ret_addr = &pused[0]->u.us.us;
		break;
	    case DILL_I:
		ret_type = &ffi_type_sint32;
		ret_addr = &pused[0]->u.i.i;
		break;
	    case DILL_U:
		ret_type = &ffi_type_uint32;
		ret_addr = &pused[0]->u.u.u;
		break;
	    case DILL_L:
		ret_type = &ffi_type_sint64;
		ret_addr = &pused[0]->u.l.l;
		break;
	    case DILL_UL:
		ret_type = &ffi_type_uint64;
		ret_addr = &pused[0]->u.ul.ul;
		break;
	    case DILL_P:
		ret_type = &ffi_type_pointer;
		ret_addr = &pused[0]->u.p.p;
		break;
	    case DILL_F:
		ret_type = &ffi_type_float;
		ret_addr = &pused[0]->u.f.f;
		break;
	    case DILL_D:
		ret_type = &ffi_type_double;
		ret_addr = &pused[0]->u.d.d;
		break;
	    case DILL_V:
		ret_type = &ffi_type_void;
		ret_addr = NULL;
		break;
	    }
	    for (i=0; i < ec->out_param_count; i++) {
		switch(ec->out_params[i].typ) {
		case DILL_C:
		    args[i] = &ffi_type_sint8;
		    values[i] = &ec->out_params[i].val.u.c.c;
		    break;
		case DILL_UC:
		    args[i] = &ffi_type_uint8;
		    values[i] = &ec->out_params[i].val.u.uc.uc;
		    break;
		case DILL_S:
		    args[i] = &ffi_type_sint16;
		    values[i] = &ec->out_params[i].val.u.s.s;
		    break;
		case DILL_US:
		    args[i] = &ffi_type_uint16;
		    values[i] = &ec->out_params[i].val.u.us.us;
		    break;
		case DILL_I:
		    args[i] = &ffi_type_sint32;
		    values[i] = &ec->out_params[i].val.u.i.i;
		    break;
		case DILL_U:
		    args[i] = &ffi_type_uint32;
		    values[i] = &ec->out_params[i].val.u.u.u;
		    break;
		case DILL_L:
		    args[i] = &ffi_type_sint64;
		    values[i] = &ec->out_params[i].val.u.l.l;
		    break;
		case DILL_UL:
		    args[i] = &ffi_type_uint64;
		    values[i] = &ec->out_params[i].val.u.ul.ul;
		    break;
		case DILL_P:
		    args[i] = &ffi_type_pointer;
		    values[i] = &ec->out_params[i].val.u.p.p;
		    break;
		case DILL_F:
		    args[i] = &ffi_type_float;
		    values[i] = &ec->out_params[i].val.u.f.f;
		    break;
		case DILL_D:
		    args[i] = &ffi_type_double;
		    values[i] = &ec->out_params[i].val.u.d.d;
		    break;
		case DILL_V:
		    break;
		}
	    }
	    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
			     ret_type, args) == FFI_OK) {
		ffi_call(&cif, func, ret_addr, values);
	    }
	}
	    break;

	case iclass_push:
	{
	    int typ = ip->insn_code & 0xf;
	    int r0 = ip->opnds.a1.src;
	    if (ip->opnds.a1.src == 0xffff) {
		ec->out_param_count = 0;
		ec->out_params = malloc(sizeof(ec->out_params[0]));
	    } else {
		ec->out_params = realloc(ec->out_params, sizeof(ec->out_params[0]) * (ec->out_param_count + 1));
		ec->out_params[ec->out_param_count].typ = typ;
		ec->out_params[ec->out_param_count].val = *PREG(ec, r0);
		ec->out_param_count++;
	    }
	}
	break;
	case iclass_pushi:
	    ec->out_params = realloc(ec->out_params, sizeof(ec->out_params[0]) * (ec->out_param_count + 1));
	    ec->out_params[ec->out_param_count].typ = DILL_L;
	    ec->out_params[ec->out_param_count].val.u.l.l = ip->opnds.a3i.u.imm;
	    ec->out_param_count++;
	    break;
	case iclass_pushf:
	    ec->out_params = realloc(ec->out_params, sizeof(ec->out_params[0]) * (ec->out_param_count + 1));
	    ec->out_params[ec->out_param_count].typ = DILL_D;
	    ec->out_params[ec->out_param_count].val.u.l.l = ip->opnds.a3i.u.imm;
	    ec->out_param_count++;
	    break;
	case iclass_nop:
	    break;
	case iclass_compare:
	{
	    /* arith 3 operand integer insns */
	    int r0 = ip->opnds.a3.src1;
	    int r1 = ip->opnds.a3.src2;
	    int d = ip->opnds.a3.dest;
	    pused[0] = PREG(ec, r0);
	    pused[1] = PREG(ec, r1);
	    pdest = PREG(ec, d);
	    pdest->u.i.i = emulate_compare(insn_code, pused[0], pused[1]);
	    break;
	}
	case iclass_mark_label:
	    break;
	default:
	    printf("Unhandled insn in emulator, %d\n", ip->class_code);
	    break;
	}
	ip++;
    }
}
#endif
