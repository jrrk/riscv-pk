#ifndef _RISCV_FP_EMULATION_H
#define _RISCV_FP_EMULATION_H

#include "emulation.h"

#define GET_PRECISION(insn) (((insn) >> 25) & 3)
#define GET_RM(insn) (((insn) >> 12) & 7)
#define PRECISION_S 0
#define PRECISION_D 1

# define GET_F64_REG(insn, pos, regs) (*(int64_t*)((void*)((regs) + 32) + (SHIFT_RIGHT(insn, (pos)-3) & 0xf8)))
# define SET_F64_REG(insn, pos, regs, val) (GET_F64_REG(insn, pos, regs) = (val))
# define GET_F32_REG(insn, pos, regs) (*(int32_t*)&GET_F64_REG(insn, pos, regs))
# define SET_F32_REG(insn, pos, regs, val) (GET_F32_REG(insn, pos, regs) = (val))
# define GET_FCSR() ({ register int tp asm("tp"); tp & 0xFF; })
# define SET_FCSR(value) ({ asm volatile("add tp, x0, %0" :: "rI"((value) & 0xFF)); SET_FS_DIRTY(); })
# define GET_FRM() (GET_FCSR() >> 5)
# define SET_FRM(value) SET_FCSR(GET_FFLAGS() | ((value) << 5))
# define GET_FFLAGS() (GET_FCSR() & 0x1F)
# define SET_FFLAGS(value) SET_FCSR((GET_FRM() << 5) | ((value) & 0x1F))

# define SETUP_STATIC_ROUNDING(insn) ({ \
  register int tp asm("tp"); tp &= 0xFF; \
  if (likely(((insn) & MASK_FUNCT3) == MASK_FUNCT3)) tp |= tp << 8; \
  else if (GET_RM(insn) > 4) return truly_illegal_insn(regs, mcause, mepc, mstatus, insn); \
  else tp |= GET_RM(insn) << 13; \
  asm volatile ("":"+r"(tp)); })
# define softfloat_raiseFlags(which) ({ asm volatile ("or tp, tp, %0" :: "rI"(which)); })
# define softfloat_roundingMode ({ register int tp asm("tp"); tp >> 13; })
# define SET_FS_DIRTY() set_csr(mstatus, MSTATUS_FS)

#define GET_F32_RS1(insn, regs) (GET_F32_REG(insn, 15, regs))
#define GET_F32_RS2(insn, regs) (GET_F32_REG(insn, 20, regs))
#define GET_F32_RS3(insn, regs) (GET_F32_REG(insn, 27, regs))
#define GET_F64_RS1(insn, regs) (GET_F64_REG(insn, 15, regs))
#define GET_F64_RS2(insn, regs) (GET_F64_REG(insn, 20, regs))
#define GET_F64_RS3(insn, regs) (GET_F64_REG(insn, 27, regs))
#define SET_F32_RD(insn, regs, val) (SET_F32_REG(insn, 7, regs, val), SET_FS_DIRTY())
#define SET_F64_RD(insn, regs, val) (SET_F64_REG(insn, 7, regs, val), SET_FS_DIRTY())

#define GET_F32_RS2C(insn, regs) (GET_F32_REG(insn, 2, regs))
#define GET_F32_RS2S(insn, regs) (GET_F32_REG(RVC_RS2S(insn), 0, regs))
#define GET_F64_RS2C(insn, regs) (GET_F64_REG(insn, 2, regs))
#define GET_F64_RS2S(insn, regs) (GET_F64_REG(RVC_RS2S(insn), 0, regs))

void emulate_fadd(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fsub(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fmul(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fdiv(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fcmp(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fcvt_fi(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fcvt_ff(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fmv_if(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fmv_fi(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void emulate_fsgnj(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
#endif
