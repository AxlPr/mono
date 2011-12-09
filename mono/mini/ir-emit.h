/*
 * ir-emit.h: IR Creation/Emission Macros
 *
 * Author:
 *   Zoltan Varga (vargaz@gmail.com)
 *
 * (C) 2002 Ximian, Inc.
 */

#ifndef __MONO_IR_EMIT_H__
#define __MONO_IR_EMIT_H__

#include "mini.h"

G_BEGIN_DECLS

static inline guint32
alloc_ireg (MonoCompile *cfg)
{
	return cfg->next_vreg ++;
}

static inline guint32
alloc_preg (MonoCompile *cfg)
{
	return alloc_ireg (cfg);
}

static inline guint32
alloc_lreg (MonoCompile *cfg)
{
#if SIZEOF_REGISTER == 8
	return cfg->next_vreg ++;
#else
	/* Use a pair of consecutive vregs */
	guint32 res = cfg->next_vreg;

	cfg->next_vreg += 3;

	return res;
#endif
}

static inline guint32
alloc_freg (MonoCompile *cfg)
{
#ifdef MONO_ARCH_SOFT_FLOAT
	/* Allocate an lvreg so float ops can be decomposed into long ops */
	return alloc_lreg (cfg);
#else
	/* Allocate these from the same pool as the int regs */
	return cfg->next_vreg ++;
#endif
}

static inline guint32
alloc_ireg_ref (MonoCompile *cfg)
{
	int vreg = alloc_ireg (cfg);

	if (cfg->compute_gc_maps)
		mono_mark_vreg_as_ref (cfg, vreg);

	return vreg;
}

static inline guint32
alloc_ireg_mp (MonoCompile *cfg)
{
	int vreg = alloc_ireg (cfg);

	if (cfg->compute_gc_maps)
		mono_mark_vreg_as_mp (cfg, vreg);

	return vreg;
}

static inline guint32
alloc_dreg (MonoCompile *cfg, MonoStackType stack_type)
{
	switch (stack_type) {
	case STACK_I4:
	case STACK_PTR:
		return alloc_ireg (cfg);
	case STACK_MP:
		return alloc_ireg_mp (cfg);
	case STACK_OBJ:
		return alloc_ireg_ref (cfg);
	case STACK_R8:
		return alloc_freg (cfg);
	case STACK_I8:
		return alloc_lreg (cfg);
	case STACK_VTYPE:
		return alloc_ireg (cfg);
	default:
		g_warning ("Unknown stack type %x\n", stack_type);
		g_assert_not_reached ();
		return -1;
	}
}

#undef MONO_INST_NEW
/* 
 * FIXME: zeroing out some fields is not needed with the new IR, but the old 
 * JIT code still uses the left and right fields, so it has to stay.
 */
#define MONO_INST_NEW(cfg,dest,op) do {	\
		(dest) = mono_mempool_alloc ((cfg)->mempool, sizeof (MonoInst));	\
		(dest)->inst_c0 = (dest)->inst_c1 = 0; \
		(dest)->next = (dest)->prev = NULL;    \
		(dest)->opcode = (op);	\
        (dest)->flags = 0; \
        (dest)->type = 0; \
        (dest)->dreg = -1;  \
	MONO_INST_NULLIFY_SREGS ((dest));		    \
        (dest)->cil_code = (cfg)->ip;  \
	} while (0)

/*
 * Variants which take a dest argument and don't do an emit
 */
#define NEW_ICONST(cfg,dest,val) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_ICONST); \
		(dest)->inst_c0 = (val);	\
		(dest)->type = STACK_I4;	\
		(dest)->dreg = alloc_dreg ((cfg), STACK_I4);	\
	} while (0)

/* 
 * Avoid using this with a non-NULL val if possible as it is not AOT 
 * compatible. Use one of the NEW_xxxCONST variants instead.
 */
#define NEW_PCONST(cfg,dest,val) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_PCONST); \
		(dest)->inst_p0 = (val);	\
		(dest)->type = STACK_PTR;	\
		(dest)->dreg = alloc_dreg ((cfg), STACK_PTR);	\
	} while (0)

#define NEW_I8CONST(cfg,dest,val) do {  \
        MONO_INST_NEW ((cfg), (dest), OP_I8CONST); \
        (dest)->dreg = alloc_lreg ((cfg)); \
        (dest)->type = STACK_I8; \
        (dest)->inst_l = (val); \
	} while (0)

#define NEW_STORE_MEMBASE(cfg,dest,op,base,offset,sr) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->sreg1 = sr; \
        (dest)->inst_destbasereg = base; \
        (dest)->inst_offset = offset; \
	} while (0)

#define NEW_LOAD_MEMBASE(cfg,dest,op,dr,base,offset) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->dreg = (dr); \
        (dest)->inst_basereg = (base); \
        (dest)->inst_offset = (offset); \
        (dest)->type = STACK_I4; \
	} while (0)

#define NEW_LOAD_MEM(cfg,dest,op,dr,mem) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->dreg = (dr); \
        (dest)->inst_p0 = (gpointer)(gssize)(mem); \
        (dest)->type = STACK_I4; \
	} while (0)

#define NEW_UNALU(cfg,dest,op,dr,sr1) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->dreg = dr; \
        (dest)->sreg1 = sr1; \
    } while (0)        

#define NEW_BIALU(cfg,dest,op,dr,sr1,sr2) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->dreg = (dr); \
        (dest)->sreg1 = (sr1); \
        (dest)->sreg2 = (sr2); \
	} while (0)

#define NEW_BIALU_IMM(cfg,dest,op,dr,sr,imm) do { \
        MONO_INST_NEW ((cfg), (dest), (op)); \
        (dest)->dreg = dr; \
        (dest)->sreg1 = sr; \
        (dest)->inst_imm = (imm); \
	} while (0)

#ifdef MONO_ARCH_NEED_GOT_VAR

#define NEW_PATCH_INFO(cfg,dest,el1,el2) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_PATCH_INFO); \
		(dest)->inst_left = (gpointer)(el1);	\
		(dest)->inst_right = (gpointer)(el2);	\
	} while (0)

/* FIXME: Add the PUSH_GOT_ENTRY optimizations */
#define NEW_AOTCONST(cfg,dest,patch_type,cons) do {			\
        MONO_INST_NEW ((cfg), (dest), cfg->compile_aot ? OP_GOT_ENTRY : OP_PCONST); \
		if (cfg->compile_aot) {					\
			MonoInst *group, *got_loc;		\
			got_loc = mono_get_got_var (cfg);		\
			NEW_PATCH_INFO ((cfg), group, cons, patch_type); \
			(dest)->inst_basereg = got_loc->dreg;			\
			(dest)->inst_p1 = group;			\
		} else {						\
			(dest)->inst_p0 = (cons);			\
			(dest)->inst_i1 = (gpointer)(patch_type);	\
		}							\
		(dest)->type = STACK_PTR;				\
		(dest)->dreg = alloc_dreg ((cfg), STACK_PTR);	\
	} while (0)

#define NEW_AOTCONST_TOKEN(cfg,dest,patch_type,image,token,generic_context,stack_type,stack_class) do { \
		MonoInst *group, *got_loc;			\
        MONO_INST_NEW ((cfg), (dest), OP_GOT_ENTRY); \
		got_loc = mono_get_got_var (cfg);			\
		NEW_PATCH_INFO ((cfg), group, NULL, patch_type);	\
		group->inst_p0 = mono_jump_info_token_new2 ((cfg)->mempool, (image), (token), (generic_context)); \
		(dest)->inst_basereg = got_loc->dreg;				\
		(dest)->inst_p1 = group;				\
		(dest)->type = (stack_type);				\
        (dest)->klass = (stack_class);          \
		(dest)->dreg = alloc_dreg ((cfg), (stack_type));	\
	} while (0)

#else

#define NEW_AOTCONST(cfg,dest,patch_type,cons) do {    \
        MONO_INST_NEW ((cfg), (dest), cfg->compile_aot ? OP_AOTCONST : OP_PCONST); \
		(dest)->inst_p0 = (cons);	\
		(dest)->inst_i1 = (gpointer)(patch_type); \
		(dest)->type = STACK_PTR;	\
		(dest)->dreg = alloc_dreg ((cfg), STACK_PTR);	\
    } while (0)

#define NEW_AOTCONST_TOKEN(cfg,dest,patch_type,image,token,generic_context,stack_type,stack_class) do { \
        MONO_INST_NEW ((cfg), (dest), OP_AOTCONST); \
		(dest)->inst_p0 = mono_jump_info_token_new2 ((cfg)->mempool, (image), (token), (generic_context)); \
		(dest)->inst_p1 = (gpointer)(patch_type); \
		(dest)->type = (stack_type);	\
        (dest)->klass = (stack_class);          \
		(dest)->dreg = alloc_dreg ((cfg), (stack_type));	\
    } while (0)

#endif

#define NEW_CLASSCONST(cfg,dest,val) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_CLASS, (val))

#define NEW_IMAGECONST(cfg,dest,val) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_IMAGE, (val))

#define NEW_FIELDCONST(cfg,dest,val) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_FIELD, (val))

#define NEW_METHODCONST(cfg,dest,val) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_METHODCONST, (val))

#define NEW_VTABLECONST(cfg,dest,vtable) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_VTABLE, cfg->compile_aot ? (gpointer)((vtable)->klass) : (vtable))

#define NEW_SFLDACONST(cfg,dest,val) NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_SFLDA, (val))

#define NEW_LDSTRCONST(cfg,dest,image,token) NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_LDSTR, (image), (token), NULL, STACK_OBJ, mono_defaults.string_class)

#define NEW_TYPE_FROM_HANDLE_CONST(cfg,dest,image,token,generic_context) NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_TYPE_FROM_HANDLE, (image), (token), (generic_context), STACK_OBJ, mono_defaults.monotype_class)

#define NEW_LDTOKENCONST(cfg,dest,image,token) NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_LDTOKEN, (image), (token), NULL, STACK_PTR, NULL)

#define NEW_DECLSECCONST(cfg,dest,image,entry) do { \
		if (cfg->compile_aot) { \
			NEW_AOTCONST_TOKEN (cfg, dest, MONO_PATCH_INFO_DECLSEC, image, (entry).index, NULL, STACK_OBJ, NULL); \
		} else { \
			NEW_PCONST (cfg, args [0], (entry).blob); \
		} \
	} while (0)

#define NEW_METHOD_RGCTX_CONST(cfg,dest,method) do { \
		if (cfg->compile_aot) {											\
			NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_METHOD_RGCTX, (method)); \
		} else {														\
			MonoMethodRuntimeGenericContext *mrgctx;					\
			mrgctx = mono_method_lookup_rgctx (mono_class_vtable ((cfg)->domain, (method)->klass), mini_method_get_context ((method))->method_inst); \
			NEW_PCONST ((cfg), (dest), (mrgctx));						\
		}																\
	} while (0)

#define NEW_DOMAINCONST(cfg,dest) do { \
		if ((cfg->opt & MONO_OPT_SHARED) || cfg->compile_aot) {				 \
			/* avoid depending on undefined C behavior in sequence points */ \
			MonoInst* __domain_var = mono_get_domainvar (cfg); \
			NEW_TEMPLOAD (cfg, dest, __domain_var->inst_c0); \
		} else { \
			NEW_PCONST (cfg, dest, (cfg)->domain); \
		} \
	} while (0)

#define GET_VARINFO_INST(cfg,num) ((cfg)->varinfo [(num)]->inst)

#define NEW_VARLOAD(cfg,dest,var,vartype) do { \
        MONO_INST_NEW ((cfg), (dest), OP_MOVE); \
		(dest)->opcode = mono_type_to_regmove ((cfg), (vartype));  \
		type_to_eval_stack_type ((cfg), (vartype), (dest));	\
		(dest)->klass = var->klass;	\
		(dest)->sreg1 = var->dreg;   \
        (dest)->dreg = alloc_dreg ((cfg), (dest)->type); \
        if ((dest)->opcode == OP_VMOVE) (dest)->klass = mono_class_from_mono_type ((vartype)); \
	} while (0)

#ifdef MONO_ARCH_SOFT_FLOAT
#define DECOMPOSE_INTO_REGPAIR(stack_type) ((stack_type) == STACK_I8 || (stack_type) == STACK_R8)
#else
#define DECOMPOSE_INTO_REGPAIR(stack_type) ((stack_type) == STACK_I8)
#endif

#define NEW_VARLOADA(cfg,dest,var,vartype) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_LDADDR); \
		(dest)->inst_p0 = (var); \
		(var)->flags |= MONO_INST_INDIRECT;	\
		(dest)->type = STACK_MP;	\
		(dest)->klass = (var)->klass;	\
        (dest)->dreg = alloc_dreg ((cfg), STACK_MP); \
		if (SIZEOF_REGISTER == 4 && DECOMPOSE_INTO_REGPAIR ((var)->type)) { MonoInst *var1 = get_vreg_to_inst (cfg, (var)->dreg + 1); MonoInst *var2 = get_vreg_to_inst (cfg, (var)->dreg + 2); g_assert (var1); g_assert (var2); var1->flags |= MONO_INST_INDIRECT; var2->flags |= MONO_INST_INDIRECT; } \
	} while (0)

#define NEW_VARSTORE(cfg,dest,var,vartype,inst) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_MOVE); \
		(dest)->opcode = mono_type_to_regmove ((cfg), (vartype));    \
		(dest)->klass = (var)->klass;	\
        (dest)->sreg1 = (inst)->dreg; \
		(dest)->dreg = (var)->dreg;   \
        if ((dest)->opcode == OP_VMOVE) (dest)->klass = mono_class_from_mono_type ((vartype)); \
	} while (0)

#define NEW_TEMPLOAD(cfg,dest,num) NEW_VARLOAD ((cfg), (dest), (cfg)->varinfo [(num)], (cfg)->varinfo [(num)]->inst_vtype)

#define NEW_TEMPLOADA(cfg,dest,num) NEW_VARLOADA ((cfg), (dest), cfg->varinfo [(num)], cfg->varinfo [(num)]->inst_vtype)

#define NEW_TEMPSTORE(cfg,dest,num,inst) NEW_VARSTORE ((cfg), (dest), (cfg)->varinfo [(num)], (cfg)->varinfo [(num)]->inst_vtype, (inst))

#define NEW_ARGLOAD(cfg,dest,num) NEW_VARLOAD ((cfg), (dest), cfg->args [(num)], cfg->arg_types [(num)])

#define NEW_LOCLOAD(cfg,dest,num) NEW_VARLOAD ((cfg), (dest), cfg->locals [(num)], header->locals [(num)])

#define NEW_LOCSTORE(cfg,dest,num,inst) NEW_VARSTORE ((cfg), (dest), (cfg)->locals [(num)], (cfg)->locals [(num)]->inst_vtype, (inst))

#define NEW_ARGSTORE(cfg,dest,num,inst) NEW_VARSTORE ((cfg), (dest), cfg->args [(num)], cfg->arg_types [(num)], (inst))

#define NEW_LOCLOADA(cfg,dest,num) NEW_VARLOADA ((cfg), (dest), (cfg)->locals [(num)], (cfg)->locals [(num)]->inst_vtype)

#define NEW_RETLOADA(cfg,dest) do {	\
        MONO_INST_NEW ((cfg), (dest), OP_MOVE); \
        (dest)->type = STACK_MP; \
	    (dest)->klass = cfg->ret->klass;	\
	    (dest)->sreg1 = cfg->vret_addr->dreg;   \
        (dest)->dreg = alloc_dreg ((cfg), (dest)->type); \
	} while (0)

#define NEW_ARGLOADA(cfg,dest,num) NEW_VARLOADA ((cfg), (dest), arg_array [(num)], param_types [(num)])

/* Promote the vreg to a variable so its address can be taken */
#define NEW_VARLOADA_VREG(cfg,dest,vreg,ltype) do { \
        MonoInst *var = get_vreg_to_inst ((cfg), (vreg)); \
        if (!var) \
		    var = mono_compile_create_var_for_vreg ((cfg), (ltype), OP_LOCAL, (vreg)); \
        NEW_VARLOADA ((cfg), (dest), (var), (ltype)); \
    } while (0)

#define NEW_DUMMY_USE(cfg,dest,var) do { \
        MONO_INST_NEW ((cfg), (dest), OP_DUMMY_USE); \
		(dest)->sreg1 = var->dreg; \
    } while (0)

/* Variants which take a type argument and handle vtypes as well */
#define NEW_LOAD_MEMBASE_TYPE(cfg,dest,ltype,base,offset) do { \
	    NEW_LOAD_MEMBASE ((cfg), (dest), mono_type_to_load_membase ((cfg), (ltype)), 0, (base), (offset)); \
	    type_to_eval_stack_type ((cfg), (ltype), (dest)); \
	    (dest)->dreg = alloc_dreg ((cfg), (dest)->type); \
    } while (0)

#define NEW_STORE_MEMBASE_TYPE(cfg,dest,ltype,base,offset,sr) do { \
        MONO_INST_NEW ((cfg), (dest), mono_type_to_store_membase ((cfg), (ltype))); \
        (dest)->sreg1 = sr; \
        (dest)->inst_destbasereg = base; \
        (dest)->inst_offset = offset; \
	    type_to_eval_stack_type ((cfg), (ltype), (dest)); \
        (dest)->klass = mono_class_from_mono_type (ltype); \
	} while (0)

#define NEW_SEQ_POINT(cfg,dest,il_offset,ss_loc) do {	 \
	MONO_INST_NEW ((cfg), (dest), OP_SEQ_POINT); \
	(dest)->inst_imm = (il_offset); \
	(dest)->flags = ss_loc ? MONO_INST_SINGLE_STEP_LOC : 0; \
	} while (0)

#define NEW_GC_PARAM_SLOT_LIVENESS_DEF(cfg,dest,offset,type) do { \
	MONO_INST_NEW ((cfg), (dest), OP_GC_PARAM_SLOT_LIVENESS_DEF); \
	(dest)->inst_offset = (offset); \
	(dest)->inst_vtype = (type); \
	} while (0)

/*
 * Variants which do an emit as well.
 */
#define EMIT_NEW_ICONST(cfg,dest,val) do { NEW_ICONST ((cfg), (dest), (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_PCONST(cfg,dest,val) do { NEW_PCONST ((cfg), (dest), (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define	EMIT_NEW_I8CONST(cfg,dest,val) do { NEW_I8CONST ((cfg), (dest), (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_AOTCONST(cfg,dest,patch_type,cons) do { NEW_AOTCONST ((cfg), (dest), (patch_type), (cons)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_AOTCONST_TOKEN(cfg,dest,patch_type,image,token,stack_type,stack_class) do { NEW_AOTCONST_TOKEN ((cfg), (dest), (patch_type), (image), (token), NULL, (stack_type), (stack_class)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_CLASSCONST(cfg,dest,val) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_CLASS, (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_IMAGECONST(cfg,dest,val) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_IMAGE, (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_FIELDCONST(cfg,dest,val) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_FIELD, (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_METHODCONST(cfg,dest,val) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_METHODCONST, (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_VTABLECONST(cfg,dest,vtable) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_VTABLE, cfg->compile_aot ? (gpointer)((vtable)->klass) : (vtable)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_SFLDACONST(cfg,dest,val) do { NEW_AOTCONST ((cfg), (dest), MONO_PATCH_INFO_SFLDA, (val)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LDSTRCONST(cfg,dest,image,token) do { NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_LDSTR, (image), (token), NULL, STACK_OBJ, mono_defaults.string_class); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_TYPE_FROM_HANDLE_CONST(cfg,dest,image,token,generic_context) do { NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_TYPE_FROM_HANDLE, (image), (token), (generic_context), STACK_OBJ, mono_defaults.monotype_class); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LDTOKENCONST(cfg,dest,image,token) do { NEW_AOTCONST_TOKEN ((cfg), (dest), MONO_PATCH_INFO_LDTOKEN, (image), (token), NULL, STACK_PTR, NULL); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_DOMAINCONST(cfg,dest) do { NEW_DOMAINCONST ((cfg), (dest)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_DECLSECCONST(cfg,dest,image,entry) do { NEW_DECLSECCONST ((cfg), (dest), (image), (entry)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_METHOD_RGCTX_CONST(cfg,dest,method) do { NEW_METHOD_RGCTX_CONST ((cfg), (dest), (method)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_VARLOAD(cfg,dest,var,vartype) do { NEW_VARLOAD ((cfg), (dest), (var), (vartype)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_VARSTORE(cfg,dest,var,vartype,inst) do { NEW_VARSTORE ((cfg), (dest), (var), (vartype), (inst)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_VARLOADA(cfg,dest,var,vartype) do { NEW_VARLOADA ((cfg), (dest), (var), (vartype)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)


#ifdef MONO_ARCH_SOFT_FLOAT

/*
 * Since the IL stack (and our vregs) contain double values, we have to do a conversion
 * when loading/storing args/locals of type R4.
 */

#define EMIT_NEW_VARLOAD_SFLOAT(cfg,dest,var,vartype) do { \
		if (COMPILE_SOFT_FLOAT ((cfg)) && !(vartype)->byref && (vartype)->type == MONO_TYPE_R4) { \
             MonoInst *iargs [1]; \
             EMIT_NEW_VARLOADA (cfg, iargs [0], (var), (vartype)); \
             (dest) = mono_emit_jit_icall (cfg, mono_fload_r4, iargs); \
        } else { \
             EMIT_NEW_VARLOAD ((cfg), (dest), (var), (vartype)); \
        } \
    } while (0)

#define EMIT_NEW_VARSTORE_SFLOAT(cfg,dest,var,vartype,inst) do {	\
		if (COMPILE_SOFT_FLOAT ((cfg)) && !(vartype)->byref && (vartype)->type == MONO_TYPE_R4) { \
             MonoInst *iargs [2]; \
             iargs [0] = (inst); \
             EMIT_NEW_VARLOADA (cfg, iargs [1], (var), (vartype)); \
             mono_emit_jit_icall (cfg, mono_fstore_r4, iargs); \
        } else { \
             EMIT_NEW_VARSTORE ((cfg), (dest), (var), (vartype), (inst)); \
        } \
    } while (0)

#define EMIT_NEW_ARGLOAD(cfg,dest,num) EMIT_NEW_VARLOAD_SFLOAT ((cfg), (dest), cfg->args [(num)], cfg->arg_types [(num)])

#define EMIT_NEW_LOCLOAD(cfg,dest,num) EMIT_NEW_VARLOAD_SFLOAT ((cfg), (dest), cfg->locals [(num)], header->locals [(num)])

#define EMIT_NEW_LOCSTORE(cfg,dest,num,inst) EMIT_NEW_VARSTORE_SFLOAT ((cfg), (dest), (cfg)->locals [(num)], (cfg)->locals [(num)]->inst_vtype, (inst))

#define EMIT_NEW_ARGSTORE(cfg,dest,num,inst) EMIT_NEW_VARSTORE_SFLOAT ((cfg), (dest), cfg->args [(num)], cfg->arg_types [(num)], (inst))

#else

#define EMIT_NEW_ARGLOAD(cfg,dest,num) do { NEW_ARGLOAD ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LOCLOAD(cfg,dest,num) do { NEW_LOCLOAD ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LOCSTORE(cfg,dest,num,inst) do { NEW_LOCSTORE ((cfg), (dest), (num), (inst)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_ARGSTORE(cfg,dest,num,inst) do { NEW_ARGSTORE ((cfg), (dest), (num), (inst)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#endif

#define EMIT_NEW_TEMPLOAD(cfg,dest,num) do { NEW_TEMPLOAD ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_TEMPLOADA(cfg,dest,num) do { NEW_TEMPLOADA ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LOCLOADA(cfg,dest,num) do { NEW_LOCLOADA ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_ARGLOADA(cfg,dest,num) do { NEW_ARGLOADA ((cfg), (dest), (num)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_RETLOADA(cfg,dest) do { NEW_RETLOADA ((cfg), (dest)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_TEMPSTORE(cfg,dest,num,inst) do { NEW_TEMPSTORE ((cfg), (dest), (num), (inst)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_VARLOADA_VREG(cfg,dest,vreg,ltype) do { NEW_VARLOADA_VREG ((cfg), (dest), (vreg), (ltype)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_DUMMY_USE(cfg,dest,var) do { NEW_DUMMY_USE ((cfg), (dest), (var)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_UNALU(cfg,dest,op,dr,sr1) do { NEW_UNALU ((cfg), (dest), (op), (dr), (sr1)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_BIALU(cfg,dest,op,dr,sr1,sr2) do { NEW_BIALU ((cfg), (dest), (op), (dr), (sr1), (sr2)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_BIALU_IMM(cfg,dest,op,dr,sr,imm) do { NEW_BIALU_IMM ((cfg), (dest), (op), (dr), (sr), (imm)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LOAD_MEMBASE(cfg,dest,op,dr,base,offset) do { NEW_LOAD_MEMBASE ((cfg), (dest), (op), (dr), (base), (offset)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_STORE_MEMBASE(cfg,dest,op,base,offset,sr) do { NEW_STORE_MEMBASE ((cfg), (dest), (op), (base), (offset), (sr)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_LOAD_MEMBASE_TYPE(cfg,dest,ltype,base,offset) do { NEW_LOAD_MEMBASE_TYPE ((cfg), (dest), (ltype), (base), (offset)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_STORE_MEMBASE_TYPE(cfg,dest,ltype,base,offset,sr) do { NEW_STORE_MEMBASE_TYPE ((cfg), (dest), (ltype), (base), (offset), (sr)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)

#define EMIT_NEW_GC_PARAM_SLOT_LIVENESS_DEF(cfg,dest,offset,type) do { NEW_GC_PARAM_SLOT_LIVENESS_DEF ((cfg), (dest), (offset), (type)); MONO_ADD_INS ((cfg)->cbb, (dest)); } while (0)
/*
 * Variants which do not take an dest argument, but take a dreg argument.
 */
#define	MONO_EMIT_NEW_ICONST(cfg,dr,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), OP_ICONST); \
        inst->dreg = dr; \
        inst->inst_c0 = imm; \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define MONO_EMIT_NEW_PCONST(cfg,dr,val) do {	\
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), OP_PCONST); \
        inst->dreg = dr; \
		(inst)->inst_p0 = (val);	\
		(inst)->type = STACK_PTR;	\
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define	MONO_EMIT_NEW_I8CONST(cfg,dr,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), OP_I8CONST); \
        inst->dreg = dr; \
        inst->inst_l = imm; \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#ifdef MONO_ARCH_NEED_GOT_VAR

#define MONO_EMIT_NEW_AOTCONST(cfg,dr,cons,patch_type) do { \
        MonoInst *inst; \
        NEW_AOTCONST ((cfg), (inst), (patch_type), (cons)); \
        inst->dreg = (dr); \
        MONO_ADD_INS ((cfg)->cbb, inst); \
    } while (0)

#else

#define	MONO_EMIT_NEW_AOTCONST(cfg,dr,imm,type) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), OP_AOTCONST); \
        inst->dreg = dr; \
        inst->inst_p0 = imm; \
        inst->inst_c1 = type; \
		MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#endif

#define	MONO_EMIT_NEW_CLASSCONST(cfg,dr,imm) MONO_EMIT_NEW_AOTCONST(cfg,dr,imm,MONO_PATCH_INFO_CLASS)
#define MONO_EMIT_NEW_VTABLECONST(cfg,dest,vtable) MONO_EMIT_NEW_AOTCONST ((cfg), (dest), (cfg)->compile_aot ? (gpointer)((vtable)->klass) : (vtable), MONO_PATCH_INFO_VTABLE)
#define MONO_EMIT_NEW_SIGNATURECONST(cfg,dr,sig) MONO_EMIT_NEW_AOTCONST ((cfg), (dr), (sig), MONO_PATCH_INFO_SIGNATURE)

#define MONO_EMIT_NEW_VZERO(cfg,dr,kl) do {	\
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), MONO_CLASS_IS_SIMD (cfg, kl) ? OP_XZERO : OP_VZERO); \
        inst->dreg = dr; \
		(inst)->type = STACK_VTYPE;	\
        (inst)->klass = (kl); \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define MONO_EMIT_NEW_UNALU(cfg,op,dr,sr1) do { \
        MonoInst *inst; \
        EMIT_NEW_UNALU ((cfg), (inst), (op), (dr), (sr1)); \
	} while (0)

#define MONO_EMIT_NEW_BIALU(cfg,op,dr,sr1,sr2) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (op)); \
        inst->dreg = dr; \
        inst->sreg1 = sr1; \
        inst->sreg2 = sr2; \
	    MONO_ADD_INS (cfg->cbb, inst); \
	} while (0)

#define MONO_EMIT_NEW_BIALU_IMM(cfg,op,dr,sr,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (op)); \
        inst->dreg = dr; \
        inst->sreg1 = sr; \
        inst->inst_imm = (mgreg_t)(imm);		\
	    MONO_ADD_INS (cfg->cbb, inst); \
	} while (0)

#define	MONO_EMIT_NEW_COMPARE_IMM(cfg,sr1,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (OP_COMPARE_IMM)); \
        inst->sreg1 = sr1; \
        inst->inst_imm = (imm);			 \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define	MONO_EMIT_NEW_ICOMPARE_IMM(cfg,sr1,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), sizeof (mgreg_t) == 8 ? OP_ICOMPARE_IMM : OP_COMPARE_IMM); \
        inst->sreg1 = sr1; \
        inst->inst_imm = (imm);			 \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

/* This is used on 32 bit machines too when running with LLVM */
#define	MONO_EMIT_NEW_LCOMPARE_IMM(cfg,sr1,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (OP_LCOMPARE_IMM)); \
        inst->sreg1 = sr1;									\
        if (SIZEOF_REGISTER == 4 && COMPILE_LLVM (cfg))  { 	\
			guint64 _l = (imm);								\
			inst->inst_imm = _l & 0xffffffff;				\
			inst->inst_offset = _l >> 32;						\
		} else { \
			inst->inst_imm = (imm);		 \
		}								 \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define MONO_EMIT_NEW_LOAD_MEMBASE_OP(cfg,op,dr,base,offset) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (op)); \
        inst->dreg = dr; \
        inst->inst_basereg = base; \
        inst->inst_offset = offset; \
	    MONO_ADD_INS (cfg->cbb, inst); \
    } while (0)

#define MONO_EMIT_NEW_LOAD_MEMBASE(cfg,dr,base,offset) MONO_EMIT_NEW_LOAD_MEMBASE_OP ((cfg), (OP_LOAD_MEMBASE), (dr), (base), (offset))

#define MONO_EMIT_NEW_STORE_MEMBASE(cfg,op,base,offset,sr) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (op)); \
        (inst)->sreg1 = sr; \
        (inst)->inst_destbasereg = base; \
        (inst)->inst_offset = offset; \
	    MONO_ADD_INS (cfg->cbb, inst); \
	} while (0)

#define MONO_EMIT_NEW_STORE_MEMBASE_IMM(cfg,op,base,offset,imm) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (op)); \
        inst->inst_destbasereg = base; \
        inst->inst_offset = offset; \
        inst->inst_imm = (mgreg_t)(imm); \
        MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

#define	MONO_EMIT_NEW_COND_EXC(cfg,cond,name) do { \
        MonoInst *inst; \
        MONO_INST_NEW ((cfg), (inst), (OP_COND_EXC_##cond)); \
        inst->inst_p1 = (char*)name; \
	    MONO_ADD_INS ((cfg)->cbb, inst); \
	} while (0)

/* Branch support */

/*
 * Basic blocks have two numeric identifiers:
 * dfn: Depth First Number
 * block_num: unique ID assigned at bblock creation
 */
#define NEW_BBLOCK(cfg,bblock) do { \
	(bblock) = mono_mempool_alloc0 ((cfg)->mempool, sizeof (MonoBasicBlock)); \
	(bblock)->block_num = cfg->num_bblocks++; \
    } while (0)

#define ADD_BBLOCK(cfg,b) do {	\
        if ((b)->cil_code)  {\
			cfg->cil_offset_to_bb [(b)->cil_code - cfg->cil_start] = (b);	\
        } \
		(b)->real_offset = cfg->real_offset;	\
	} while (0)

/* Emit a one-way conditional branch */
/* 
 * The inst_false_bb field of the cond branch will not be set, the JIT code should be
 * prepared to deal with this.
 */
#ifdef DEBUG_EXTENDED_BBLOCKS
static int ccount = 0;
#define MONO_EMIT_NEW_BRANCH_BLOCK(cfg,op,truebb) do { \
        MonoInst *ins; \
        MonoBasicBlock *falsebb; \
        MONO_INST_NEW ((cfg), (ins), (op)); \
        if ((op) == OP_BR) { \
	        NEW_BBLOCK ((cfg), falsebb); \
            ins->inst_target_bb = (truebb); \
            mono_link_bblock ((cfg), (cfg)->cbb, (truebb)); \
            MONO_ADD_INS ((cfg)->cbb, ins); \
            MONO_START_BB ((cfg), falsebb); \
        } else { \
            ccount ++; \
		    ins->inst_many_bb = mono_mempool_alloc (cfg->mempool, sizeof(gpointer)*2);	\
            ins->inst_true_bb = (truebb); \
            ins->inst_false_bb = NULL; \
            mono_link_bblock ((cfg), (cfg)->cbb, (truebb)); \
            MONO_ADD_INS ((cfg)->cbb, ins); \
            if (getenv ("COUNT2") && ccount == atoi (getenv ("COUNT2")) - 1) { printf ("HIT: %d\n", cfg->cbb->block_num); } \
            if (getenv ("COUNT2") && ccount < atoi (getenv ("COUNT2"))) { \
                 cfg->cbb->extended = TRUE; \
            } else { NEW_BBLOCK ((cfg), falsebb); ins->inst_false_bb = (falsebb); mono_link_bblock ((cfg), (cfg)->cbb, (falsebb)); MONO_START_BB ((cfg), falsebb); } \
        } \
	} while (0)
#else
#define MONO_EMIT_NEW_BRANCH_BLOCK(cfg,op,truebb) do { \
        MonoInst *ins; \
        MonoBasicBlock *falsebb; \
        MONO_INST_NEW ((cfg), (ins), (op)); \
        if ((op) == OP_BR) { \
	        NEW_BBLOCK ((cfg), falsebb); \
            ins->inst_target_bb = (truebb); \
            mono_link_bblock ((cfg), (cfg)->cbb, (truebb)); \
            MONO_ADD_INS ((cfg)->cbb, ins); \
            MONO_START_BB ((cfg), falsebb); \
        } else { \
		    ins->inst_many_bb = mono_mempool_alloc (cfg->mempool, sizeof(gpointer)*2);	\
            ins->inst_true_bb = (truebb); \
            ins->inst_false_bb = NULL; \
            mono_link_bblock ((cfg), (cfg)->cbb, (truebb)); \
            MONO_ADD_INS ((cfg)->cbb, ins); \
            if (!cfg->enable_extended_bblocks) { \
                NEW_BBLOCK ((cfg), falsebb); \
                ins->inst_false_bb = falsebb; \
                mono_link_bblock ((cfg), (cfg)->cbb, (falsebb)); \
                MONO_START_BB ((cfg), falsebb); \
            } else { \
				cfg->cbb->extended = TRUE; \
			} \
        } \
	} while (0)
#endif

/* Emit a two-way conditional branch */
#define	MONO_EMIT_NEW_BRANCH_BLOCK2(cfg,op,truebb,falsebb) do { \
        MonoInst *ins; \
        MONO_INST_NEW ((cfg), (ins), (op)); \
		ins->inst_many_bb = mono_mempool_alloc (cfg->mempool, sizeof(gpointer)*2);	\
        ins->inst_true_bb = (truebb); \
        ins->inst_false_bb = (falsebb); \
        mono_link_bblock ((cfg), (cfg)->cbb, (truebb)); \
        mono_link_bblock ((cfg), (cfg)->cbb, (falsebb)); \
        MONO_ADD_INS ((cfg)->cbb, ins); \
	} while (0)

#define MONO_START_BB(cfg, bblock) do { \
        ADD_BBLOCK ((cfg), (bblock)); \
        if (cfg->cbb->last_ins && MONO_IS_COND_BRANCH_OP (cfg->cbb->last_ins) && !cfg->cbb->last_ins->inst_false_bb) { \
            cfg->cbb->last_ins->inst_false_bb = (bblock); \
            mono_link_bblock ((cfg), (cfg)->cbb, (bblock)); \
        } else if (! (cfg->cbb->last_ins && ((cfg->cbb->last_ins->opcode == OP_BR) || (cfg->cbb->last_ins->opcode == OP_BR_REG) || MONO_IS_COND_BRANCH_OP (cfg->cbb->last_ins)))) \
            mono_link_bblock ((cfg), (cfg)->cbb, (bblock)); \
	    (cfg)->cbb->next_bb = (bblock); \
	    (cfg)->cbb = (bblock); \
    } while (0)

/* This marks a place in code where an implicit exception could be thrown */
#define MONO_EMIT_NEW_IMPLICIT_EXCEPTION(cfg) do { \
		if (COMPILE_LLVM ((cfg))) {									\
			MONO_EMIT_NEW_UNALU (cfg, OP_IMPLICIT_EXCEPTION, -1, -1);	\
		} \
	} while (0)

/* Loads/Stores which can fault are handled correctly by the LLVM mono branch */
#define MONO_EMIT_NEW_IMPLICIT_EXCEPTION_LOAD_STORE(cfg) do { \
	if (COMPILE_LLVM (cfg) && !IS_LLVM_MONO_BRANCH)			\
		MONO_EMIT_NEW_IMPLICIT_EXCEPTION ((cfg));			\
    } while (0)

/* Emit an explicit null check which doesn't depend on SIGSEGV signal handling */
#define MONO_EMIT_NULL_CHECK(cfg, reg) do { \
		if (cfg->explicit_null_checks) {							  \
			MONO_EMIT_NEW_BIALU_IMM (cfg, OP_COMPARE_IMM, -1, (reg), 0); \
			MONO_EMIT_NEW_COND_EXC (cfg, EQ, "NullReferenceException"); \
		} else {			\
			MONO_EMIT_NEW_IMPLICIT_EXCEPTION_LOAD_STORE (cfg);						\
		}																\
	} while (0)

#define MONO_EMIT_NEW_CHECK_THIS(cfg, sreg) do { \
		cfg->flags |= MONO_CFG_HAS_CHECK_THIS;	 \
		if (cfg->explicit_null_checks) {		 \
			MONO_EMIT_NULL_CHECK (cfg, sreg);				\
		} else {											\
			MONO_EMIT_NEW_UNALU (cfg, OP_CHECK_THIS, -1, sreg);			\
			MONO_EMIT_NEW_IMPLICIT_EXCEPTION_LOAD_STORE (cfg);			\
		}																\
		MONO_EMIT_NEW_UNALU (cfg, OP_NOT_NULL, -1, sreg);				\
	} while (0)

#define NEW_LOAD_MEMBASE_FLAGS(cfg,dest,op,dr,base,offset,ins_flags) do {	\
		int __ins_flags = ins_flags; \
		if (__ins_flags & MONO_INST_FAULT) {								\
			MONO_EMIT_NULL_CHECK ((cfg), (base));						\
			if (cfg->explicit_null_checks)								\
				__ins_flags &= ~MONO_INST_FAULT;							\
		}																\
		NEW_LOAD_MEMBASE ((cfg), (dest), (op), (dr), (base), (offset));	\
		(dest)->flags = (__ins_flags);									\
	} while (0)

#define MONO_EMIT_NEW_LOAD_MEMBASE_OP_FLAGS(cfg,op,dr,base,offset,ins_flags) do { \
        MonoInst *inst;													\
		int __ins_flags = ins_flags; \
	    if (__ins_flags & MONO_INST_FAULT) {									\
			MONO_EMIT_NULL_CHECK ((cfg), (base));						\
			if (cfg->explicit_null_checks)								\
				__ins_flags &= ~MONO_INST_FAULT;							\
		}																\
		NEW_LOAD_MEMBASE ((cfg), (inst), (op), (dr), (base), (offset)); \
		inst->flags = (__ins_flags); \
	    MONO_ADD_INS (cfg->cbb, inst); \
    } while (0)

#define MONO_EMIT_NEW_LOAD_MEMBASE_FLAGS(cfg,dr,base,offset,ins_flags) MONO_EMIT_NEW_LOAD_MEMBASE_OP_FLAGS ((cfg), (OP_LOAD_MEMBASE), (dr), (base), (offset),(ins_flags))

/* A load which can cause a nullref */
#define NEW_LOAD_MEMBASE_FAULT(cfg,dest,op,dr,base,offset) NEW_LOAD_MEMBASE_FLAGS ((cfg), (dest), (op), (dr), (base), (offset), MONO_INST_FAULT)

#define EMIT_NEW_LOAD_MEMBASE_FAULT(cfg,dest,op,dr,base,offset) do { \
		NEW_LOAD_MEMBASE_FAULT ((cfg), (dest), (op), (dr), (base), (offset)); \
		MONO_ADD_INS ((cfg)->cbb, (dest)); \
	} while (0)

#define MONO_EMIT_NEW_LOAD_MEMBASE_OP_FAULT(cfg,op,dr,base,offset) MONO_EMIT_NEW_LOAD_MEMBASE_OP_FLAGS ((cfg), (op), (dr), (base), (offset), MONO_INST_FAULT)

#define MONO_EMIT_NEW_LOAD_MEMBASE_FAULT(cfg,dr,base,offset) MONO_EMIT_NEW_LOAD_MEMBASE_OP_FAULT ((cfg), (OP_LOAD_MEMBASE), (dr), (base), (offset))

/*Object Model related macros*/

/* Default bounds check implementation for most architectures + llvm */
#define MONO_EMIT_DEFAULT_BOUNDS_CHECK(cfg, array_reg, offset, index_reg, fault) do { \
			int _length_reg = alloc_ireg (cfg); \
			if (fault) \
				MONO_EMIT_NEW_LOAD_MEMBASE_OP_FAULT (cfg, OP_LOADI4_MEMBASE, _length_reg, array_reg, offset); \
			else \
				MONO_EMIT_NEW_LOAD_MEMBASE_OP_FLAGS (cfg, OP_LOADI4_MEMBASE, _length_reg, array_reg, offset, MONO_INST_CONSTANT_LOAD); \
			MONO_EMIT_NEW_BIALU (cfg, OP_COMPARE, -1, _length_reg, index_reg); \
			MONO_EMIT_NEW_COND_EXC (cfg, LE_UN, "IndexOutOfRangeException"); \
	} while (0)

#ifndef MONO_ARCH_EMIT_BOUNDS_CHECK
#define MONO_ARCH_EMIT_BOUNDS_CHECK(cfg, array_reg, offset, index_reg) MONO_EMIT_DEFAULT_BOUNDS_CHECK ((cfg), (array_reg), (offset), (index_reg), TRUE)
#endif

/* cfg is the MonoCompile been used
 * array_reg is the vreg holding the array object
 * array_type is a struct (usually MonoArray or MonoString)
 * array_length_field is the field in the previous struct with the length
 * index_reg is the vreg holding the index
 */
#define MONO_EMIT_BOUNDS_CHECK(cfg, array_reg, array_type, array_length_field, index_reg) do { \
		if (!(cfg->opt & MONO_OPT_UNSAFE)) {							\
		if (!(cfg->opt & MONO_OPT_ABCREM)) {							\
			MONO_EMIT_NULL_CHECK (cfg, array_reg);						\
			if (COMPILE_LLVM (cfg)) \
				MONO_EMIT_DEFAULT_BOUNDS_CHECK ((cfg), (array_reg), G_STRUCT_OFFSET (array_type, array_length_field), (index_reg), TRUE); \
			else \
				MONO_ARCH_EMIT_BOUNDS_CHECK ((cfg), (array_reg), G_STRUCT_OFFSET (array_type, array_length_field), (index_reg)); \
		} else {														\
			MonoInst *ins;												\
			MONO_INST_NEW ((cfg), ins, OP_BOUNDS_CHECK);				\
			ins->sreg1 = array_reg;										\
			ins->sreg2 = index_reg;										\
			ins->inst_imm = G_STRUCT_OFFSET (array_type, array_length_field); \
			ins->flags |= MONO_INST_FAULT; \
			MONO_ADD_INS ((cfg)->cbb, ins);								\
			(cfg)->flags |= MONO_CFG_HAS_ARRAY_ACCESS;					\
			(cfg)->cbb->has_array_access = TRUE;						\
		}																\
		}																\
    } while (0)

G_END_DECLS

#endif
