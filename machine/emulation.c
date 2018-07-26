#include "emulation.h"
#include "fp_emulation.h"
#include "config.h"
#include "unprivileged_memory.h"
#include "mtrap.h"
#include "atomic.h"
#include "hid.h"
#include <limits.h>

static void hex4(uint32_t h)
{
  if (h>>4) hex4(h>>4);
  hid_send("0123456789ABCDEF"[h&0xF]);
}

static void hex8(uint64_t h)
{
  if (h>>4) hex8(h>>4);
  hid_send("0123456789ABCDEF"[h&0xF]);
}

static void emuldebug4(uint32_t h)
{
  hex4(h);
  hid_send('\n');
}

static void emuldebug8(uint64_t h)
{
  hex8(h);
  hid_send('\n');
}

static void emuldebugm(const char *fmt, uint32_t insn)
{
  hid_send_string(fmt);
  hid_send_string(" DASM(0x");
  hex4(insn);
  hid_send(')');
  hid_send('\n');
}

#define do_amo(expr) \
    do { \
        set_csr(mstatus, MSTATUS_MPRV); \
        expr;                             \
        clear_csr(mstatus, MSTATUS_MPRV); \
    } while(0);

static inline uint32_t get32(volatile uint32_t *addr)
{
  uint32_t rslt = 0;
  do_amo(rslt = *addr);
  return rslt;
}

static inline void put32(volatile uint32_t *addr, uint32_t rslt)
{
  do_amo(*addr = rslt);
}

static inline uint64_t get64(volatile uint64_t *addr)
{
  uint64_t rslt = 0;
  do_amo(rslt = *addr);
  return rslt;
}

static inline void put64(volatile uint64_t *addr, uint64_t rslt)
{
  do_amo(*addr = rslt);
}

#define AMO_WORD_1(typ, siz, expr)               \
      { \
        typ rd = 0xDEADBEEF; \
        volatile typ* addr = (volatile typ*)*rs1; \
        typ old = rs2 != regs ? *(typ *)rs2 : 0;              \
        rd = get##siz(addr); \
        put##siz(addr, expr); \
        if (dest != regs) { \
            *(typ *)dest = (typ)rd; \
          } \
        write_csr(mepc, mepc + 4); \
      } \
  
#define AMO_WORD_2(typ, siz)                        \
      { \
        typ rd = 0xDEADBEEF; \
        volatile typ* addr = (volatile typ*)*rs1; \
        /*        typ old = rs2 != regs ? *(typ *)rs2 : 0;              */ \
        lr = *rs1; \
        rd = get##siz(addr); \
        if (dest != regs) { \
            *(typ *)dest = (typ)rd; \
          } \
        write_csr(mepc, mepc + 4); \
      } \
  
#define AMO_WORD_3(typ, siz)                        \
      { \
        typ rd = 0xDEADBEEF; \
        volatile typ* addr = (volatile typ*)*rs1; \
        typ old = rs2 != regs ? *(typ *)rs2 : 0;              \
        if (lr == *rs1) { \
          rd = 0; \
          put##siz(addr, old); \
        } else { \
          rd = 1; \
        } \
        if (dest != regs)  { \
          *dest = rd; \
        } \
        write_csr(mepc, mepc + 4); \
      } \

DECLARE_EMULATION_FUNC(emulate_missing_insn)
{
    uintptr_t rd;
    uintptr_t status = clear_csr(mstatus, MSTATUS_MIE);
  enum {mask_amo_sc = 0xf800707f,
        mask_lr = 0xf9f0707f,
        mask_fadd = 0xfe00007f,
        mask_fld_fst = 0x707f,
        mask_fcmp_sgn = 0xfe00707f,
        mask_fcvt = 0xfff0007f,
        mask_fmv = 0xfff0707f,
        mask_cld_sd = 0xe003};
  static uintptr_t lr = 0;
  unsigned long *rs1, *rs2, *dest;
  rs1 = &regs[(insn >> SH_RS1) & 31];
  rs2 = &regs[(insn >> SH_RS2) & 31];
  dest = &regs[(insn >> SH_RD) & 31];
  switch(insn & mask_amo_sc)
      {
      case MATCH_AMOADD_W:
        AMO_WORD_1(uint32_t, 32, rd+old)
        break;
      case MATCH_AMOAND_W:
        AMO_WORD_1(uint32_t, 32, rd&old)
        break;
      case MATCH_AMOOR_W:
        AMO_WORD_1(uint32_t, 32, rd|old)
        break;
      case MATCH_AMOXOR_W:
        AMO_WORD_1(uint32_t, 32, rd^old)
        break;
      case MATCH_AMOMIN_W:
        AMO_WORD_1(uint32_t, 32, (int32_t)rd < (int32_t)old ? rd : old)
        break;
      case MATCH_AMOMAX_W:
        AMO_WORD_1(uint32_t, 32, (int32_t)rd > (int32_t)old ? rd : old)
        break;
      case MATCH_AMOMINU_W:
        AMO_WORD_1(uint32_t, 32, rd < old ? rd : old)
        break;
      case MATCH_AMOMAXU_W:
        AMO_WORD_1(uint32_t, 32, rd > old ? rd : old)
        break;
      case MATCH_AMOSWAP_W:
        AMO_WORD_1(uint32_t, 32, old)
        break;
      case MATCH_AMOADD_D:
        AMO_WORD_1(uint64_t, 64, rd+old)
        break;
      case MATCH_AMOAND_D:
        AMO_WORD_1(uint64_t, 64, rd&old)
        break;
      case MATCH_AMOOR_D:
        AMO_WORD_1(uint64_t, 64, rd|old)
        break;
      case MATCH_AMOXOR_D:
        AMO_WORD_1(uint64_t, 64, rd^old)
        break;
      case MATCH_AMOMIN_D:
        AMO_WORD_1(uint64_t, 64, (int64_t)rd < (int64_t)old ? rd : old)
        break;
      case MATCH_AMOMAX_D:
        AMO_WORD_1(uint64_t, 64, (int64_t)rd > (int64_t)old ? rd : old)
        break;
      case MATCH_AMOMINU_D:
        AMO_WORD_1(uint64_t, 64, rd < old ? rd : old)
        break;
      case MATCH_AMOMAXU_D:
        AMO_WORD_1(uint64_t, 64, rd > old ? rd : old)
        break;
      case MATCH_AMOSWAP_D:
        AMO_WORD_1(uint64_t, 64, old)
        break;
      case MATCH_SC_W:
        // SC returns either 0 or 1, so we need to write a full xlen
        // register here
        AMO_WORD_3(uint32_t, 32)                            
          break;
      case MATCH_SC_D:
        // SC returns either 0 or 1, so we need to write a full xlen
        // register here
        AMO_WORD_3(uint64_t, 64)                            
          break;
      default: switch(insn & mask_lr)
          {
          case MATCH_LR_W:
            AMO_WORD_2(uint32_t, 32)
              break;
          case MATCH_LR_D:
            AMO_WORD_2(uint64_t, 64)
              break;
          default: switch (insn & mask_fadd)
              {
              case MATCH_FADD_S:
              case MATCH_FADD_D:
              case MATCH_FADD_Q:
              case MATCH_FSUB_S:
              case MATCH_FSUB_D:
              case MATCH_FSUB_Q:
              case MATCH_FMUL_S:
              case MATCH_FMUL_D:
              case MATCH_FMUL_Q:
                emuldebugm("Skipped 4-byte floating point insn", insn);
                write_csr(mepc, mepc + 4);
                break;
              default: switch (insn & mask_fld_fst)
                  {
                  case MATCH_FSD:
                    {
                      uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
                      if (unlikely(addr % sizeof(uintptr_t)))
                        return misaligned_store_trap(regs, mcause, mepc);
                      store_uint64_t((void *)addr, GET_F64_RS2(insn, regs), mepc);
                      write_csr(mepc, mepc + 4);
                      break;
                    }
                  case MATCH_FSW:
                    {
                      uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
                      if (unlikely(addr % sizeof(uintptr_t)))
                        return misaligned_store_trap(regs, mcause, mepc);
                      store_uint32_t((void *)addr, GET_F32_RS2(insn, regs), mepc);
                      write_csr(mepc, mepc + 4);
                      break;
                    }
                  case MATCH_FLD:
                    {
                      uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
                      if (unlikely(addr % sizeof(uintptr_t)))
                        return misaligned_load_trap(regs, mcause, mepc);
                      SET_F64_RD(insn, regs, load_uint64_t((void *)addr, mepc));
                      write_csr(mepc, mepc + 4);
                      break;
                    }
                  case MATCH_FLW:
                    {
                      uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
                      if (unlikely(addr % sizeof(uintptr_t)))
                        return misaligned_load_trap(regs, mcause, mepc);
                      SET_F32_RD(insn, regs, load_uint32_t((void *)addr, mepc));
                      write_csr(mepc, mepc + 4);
                      break;
                    }
                  default: switch(insn & mask_fcmp_sgn)
                      {
                      case MATCH_FEQ_S:
                      case MATCH_FEQ_D:
                      case MATCH_FEQ_Q:
                      case MATCH_FLE_S:
                      case MATCH_FLE_D:
                      case MATCH_FLE_Q:
                      case MATCH_FLT_S:
                      case MATCH_FLT_D:
                      case MATCH_FLT_Q:
                      case  MATCH_FSGNJN_D:
                      case  MATCH_FSGNJN_Q:
                      case  MATCH_FSGNJN_S:
                      case  MATCH_FSGNJ_Q:
                      case  MATCH_FSGNJ_S:
                      case  MATCH_FSGNJX_D:
                      case  MATCH_FSGNJX_Q:
                      case  MATCH_FSGNJX_S:
                        emuldebugm("Skipped 4-byte floating point insn", insn);
                        write_csr(mepc, mepc + 4);
                        break;
                      default: switch(insn & mask_fmv)
                          {
                          case MATCH_FMV_X_W:
                          case MATCH_FMV_X_D:
                          case MATCH_FMV_X_Q:
                          case MATCH_FMV_W_X:
                          case MATCH_FMV_D_X:
                          case MATCH_FMV_Q_X:
                            emuldebugm("Skipped 4-byte floating point insn", insn);
                            write_csr(mepc, mepc + 4);
                            break;
                          default: switch(insn & mask_fcvt)
                              {
                              case  MATCH_FCVT_D_L:
                              case  MATCH_FCVT_D_LU:
                              case  MATCH_FCVT_D_Q:
                              case  MATCH_FCVT_D_S:
                              case  MATCH_FCVT_D_W:
                              case  MATCH_FCVT_D_WU:
                              case  MATCH_FCVT_L_D:
                              case  MATCH_FCVT_L_Q:
                              case  MATCH_FCVT_L_S:
                              case  MATCH_FCVT_LU_D:
                              case  MATCH_FCVT_LU_Q:
                              case  MATCH_FCVT_LU_S:
                              case  MATCH_FCVT_Q_D:
                              case  MATCH_FCVT_Q_L:
                              case  MATCH_FCVT_Q_LU:
                              case  MATCH_FCVT_Q_S:
                              case  MATCH_FCVT_Q_W:
                              case  MATCH_FCVT_Q_WU:
                              case  MATCH_FCVT_S_D:
                              case  MATCH_FCVT_S_L:
                              case  MATCH_FCVT_S_LU:
                              case  MATCH_FCVT_S_Q:
                              case  MATCH_FCVT_S_W:
                              case  MATCH_FCVT_S_WU:
                              case  MATCH_FCVT_W_D:
                              case  MATCH_FCVT_W_Q:
                              case  MATCH_FCVT_W_S:
                              case  MATCH_FCVT_WU_D:
                              case  MATCH_FCVT_WU_Q:
                              case  MATCH_FCVT_WU_S:
                                emuldebugm("Skipped 4-byte floating point insn", insn);
                                write_csr(mepc, mepc + 4);
                                break;
                              default: switch(insn & mask_cld_sd)
                                  {
                                  case MATCH_C_FLWSP:
                                    {
                                      uintptr_t addr = GET_SP(regs) + RVC_LWSP_IMM(insn);
                                      if (unlikely(addr % 4))
                                        return misaligned_load_trap(regs, mcause, mepc);
                                      SET_F32_RD(insn, regs, load_int32_t((void *)addr, mepc));
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_C_FSW:
                                    {
                                      uintptr_t addr = GET_RS1S(insn, regs) + RVC_LW_IMM(insn);
                                      if (unlikely(addr % 4))
                                        return misaligned_store_trap(regs, mcause, mepc);
                                      store_uint32_t((void *)addr, GET_F32_RS2(RVC_RS2S(insn) << SH_RS2, regs), mepc);
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_C_FSWSP:
                                    {
                                      uintptr_t addr = GET_SP(regs) + RVC_SWSP_IMM(insn);
                                      if (unlikely(addr % 4))
                                        return misaligned_store_trap(regs, mcause, mepc);
                                      store_uint32_t((void *)addr, GET_F32_RS2(RVC_RS2(insn) << SH_RS2, regs), mepc);
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_C_FLW:
                                    {
                                      uintptr_t addr = GET_RS1S(insn, regs) + RVC_LW_IMM(insn);
                                      if (unlikely(addr % 4))
                                        return misaligned_load_trap(regs, mcause, mepc);
                                      SET_F32_RD(RVC_RS2S(insn) << SH_RD, regs, load_int32_t((void *)addr, mepc));
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_C_FLD:
                                     {
                                       uintptr_t addr = GET_RS1S(insn, regs) + RVC_LD_IMM(insn);
                                       if (unlikely(addr % sizeof(uintptr_t)))
                                         return misaligned_load_trap(regs, mcause, mepc);
                                       SET_F64_RD(RVC_RS2S(insn) << SH_RD, regs, load_uint64_t((void *)addr, mepc));
                                       write_csr(mepc, mepc + 2);
                                       break;
                                     }
                                  case MATCH_C_FLDSP:
                                     {
                                       uintptr_t addr = GET_SP(regs) + RVC_LDSP_IMM(insn);
                                       if (unlikely(addr % sizeof(uintptr_t)))
                                         return misaligned_load_trap(regs, mcause, mepc);
                                       SET_F64_RD(insn, regs, load_uint64_t((void *)addr, mepc));
                                       write_csr(mepc, mepc + 2);
                                       break;
                                     }
                                  case MATCH_C_FSD:
                                    {
                                      uintptr_t addr = GET_RS1S(insn, regs) + RVC_LD_IMM(insn);
                                      if (unlikely(addr % sizeof(uintptr_t)))
                                        return misaligned_store_trap(regs, mcause, mepc);
                                      store_uint64_t((void *)addr, GET_F64_RS2(RVC_RS2S(insn) << SH_RS2, regs), mepc);
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_C_FSDSP:
                                    {
                                      uintptr_t addr = GET_SP(regs) + RVC_SDSP_IMM(insn);
                                      if (unlikely(addr % sizeof(uintptr_t)))
                                        return misaligned_store_trap(regs, mcause, mepc);
                                      store_uint64_t((void *)addr, GET_F64_RS2(RVC_RS2(insn) << SH_RS2, regs), mepc);
                                      write_csr(mepc, mepc + 2);
                                      break;
                                    }
                                  case MATCH_CSRRS:
                                  case MATCH_CSRRSI:
                                    emuldebugm("Skipped 2-byte floating point insn", insn);
                                    write_csr(mepc, mepc + 2);
                                    break;
                                  default:
				    {
				      return redirect_trap(mepc, mstatus, insn);
				    }
                                  }
                              }
                          }
                      }
                  }
              }
          }
      }
    write_csr(mstatus, status);
}

void illegal_insn_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  uintptr_t mstatus = read_csr(mstatus);
  insn_t insn = read_csr(mbadaddr);
#if 0
  if (!insn)
    insn = get_insn(mepc, &mstatus);
  if (!insn)
    redirect_trap(mepc, mstatus, insn);
#endif  
  emulate_missing_insn(regs, mcause, mepc, mstatus, insn);
}

static inline int emulate_read_csr(int num, uintptr_t mstatus, uintptr_t* result)
{
  uintptr_t counteren = -1;
  if (EXTRACT_FIELD(mstatus, MSTATUS_MPP) == PRV_U)
    counteren = read_csr(scounteren);

  switch (num)
  {
#if !defined(__riscv_flen) && defined(PK_ENABLE_FP_EMULATION)
    case CSR_FRM:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FRM();
      return 0;
    case CSR_FFLAGS:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FFLAGS();
      return 0;
    case CSR_FCSR:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FCSR();
      return 0;
#endif
  }
  return -1;
}

static inline int emulate_write_csr(int num, uintptr_t value, uintptr_t mstatus)
{
  switch (num)
  {
#if !defined(__riscv_flen) && defined(PK_ENABLE_FP_EMULATION)
    case CSR_FRM: SET_FRM(value); return 0;
    case CSR_FFLAGS: SET_FFLAGS(value); return 0;
    case CSR_FCSR: SET_FCSR(value); return 0;
#endif
  }
  return -1;
}

__attribute__((noinline))
DECLARE_EMULATION_FUNC(truly_illegal_insn)
{
  return redirect_trap(mepc, mstatus, insn);
}

DECLARE_EMULATION_FUNC(emulate_system_opcode)
{
  int rs1_num = (insn >> 15) & 0x1f;
  uintptr_t rs1_val = GET_RS1(insn, regs);
  int csr_num = (uint32_t)insn >> 20;
  uintptr_t csr_val, new_csr_val;

  if (emulate_read_csr(csr_num, mstatus, &csr_val))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int do_write = rs1_num;
  switch (GET_RM(insn))
  {
    case 0: return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    case 1: new_csr_val = rs1_val; do_write = 1; break;
    case 2: new_csr_val = csr_val | rs1_val; break;
    case 3: new_csr_val = csr_val & ~rs1_val; break;
    case 4: return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    case 5: new_csr_val = rs1_num; do_write = 1; break;
    case 6: new_csr_val = csr_val | rs1_num; break;
    case 7: new_csr_val = csr_val & ~rs1_num; break;
  }

  if (do_write && emulate_write_csr(csr_num, new_csr_val, mstatus))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  SET_RD(insn, regs, csr_val);
}
