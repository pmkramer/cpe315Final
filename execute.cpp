#include "thumbsim.hpp"
// These are just the register NUMBERS
#define PC_REG 15  
#define LR_REG 14
#define SP_REG 13

// These are the contents of those registers
#define PC rf[PC_REG]
#define LR rf[LR_REG]
#define SP rf[SP_REG]

Stats stats;
Caches caches(0);

// CPE 315: you'll need to implement a custom sign-extension function
// in addition to the ones given below, specifically for the unconditional
// branch instruction, which has an 11-bit immediate field

// Added by me
// DONE (double check)
unsigned int signExtend11to32ui(short i) {
  return static_cast<unsigned int>(static_cast<int>((i << 21) >> 21));
}
  
unsigned int signExtend16to32ui(short i) {
  return static_cast<unsigned int>(static_cast<int>(i));
}

unsigned int signExtend8to32ui(char i) {
  return static_cast<unsigned int>(static_cast<int>(i));
}


// This is the global object you'll use to store condition codes N,Z,V,C
// Set these bits appropriately in execute below.
ASPR flags;

// CPE 315: You need to implement a function to set the Negative and Zero
// flags for each instruction that does that. It only needs to take
// one parameter as input, the result of whatever operation is executing
// DONE
void setNegativeZero(int result)
{
   if (result < 0)
      flags.N = 1;
   else
      flags.N = 0;

   if (result == 0)
      flags.Z = 1;
   else
      flags.Z = 0;
}

// This function is complete, you should not have to modify it
void setCarryOverflow (int num1, int num2, OFType oftype) {
  switch (oftype) {
    case OF_ADD:
      if (((unsigned long long int)num1 + (unsigned long long int)num2) ==
          ((unsigned int)num1 + (unsigned int)num2)) {
        flags.C = 0;
      }
      else {
        flags.C = 1;
      }
      if (((long long int)num1 + (long long int)num2) ==
          ((int)num1 + (int)num2)) {
        flags.V = 0;
      }
      else {
        flags.V = 1;
      }
      break;
    case OF_SUB:
      if (num1 >= num2) {
        flags.C = 1;
      }
      else if (((unsigned long long int)num1 - (unsigned long long int)num2) ==
          ((unsigned int)num1 - (unsigned int)num2)) {
        flags.C = 0;
      }
      else {
        flags.C = 1;
      }
      if (((num1==0) && (num2==0)) ||
          (((long long int)num1 - (long long int)num2) ==
           ((int)num1 - (int)num2))) {
        flags.V = 0;
      }
      else {
        flags.V = 1;
      }
      break;
    case OF_SHIFT:
      // C flag unaffected for shifts by zero
      if (num2 != 0) {
        if (((unsigned long long int)num1 << (unsigned long long int)num2) ==
            ((unsigned int)num1 << (unsigned int)num2)) {
          flags.C = 0;
        }
        else {
          flags.C = 1;
        }
      }
      // Shift doesn't set overflow
      break;
    default:
      cerr << "Bad OverFlow Type encountered." << __LINE__ << __FILE__ << endl;
      exit(1);
  }
}

// CPE 315: You're given the code for evaluating BEQ, and you'll need to 
// complete the rest of these conditions. See Page 208 of the armv7 manual
// DONE
static int checkCondition(unsigned short cond) {
  switch(cond) {
    case EQ:
      if (flags.Z == 1) {
        return TRUE;
      }
      break;
    case NE:
      if (flags.Z == 0) {
         return TRUE;
      }
      break;
    case CS:
      if (flags.C == 1) {
         return TRUE;
      }
      break;
    case CC:
      if (flags.C == 0) {
         return TRUE;
      }
      break;
    case MI:
      if (flags.N == 1) {
         return TRUE;
      }
      break;
    case PL:
      if (flags.N == 0) {
         return TRUE;
      }
      break;
    case VS:
      if (flags.V == 1) {
         return TRUE;
      }
      break;
    case VC:
      if (flags.V == 0) {
         return TRUE;
      }
      break;
    case HI:
      if (flags.C == 1 and flags.Z == 0) {
         return TRUE;
      }
      break;
    case LS:
      if (flags.C == 0 or flags.Z == 1) {
         return TRUE;
      }
      break;
    case GE:
      if (flags.N == flags.V) {
         return TRUE;
      }
      break;
    case LT:
      if (flags.N != flags.V) {
         return TRUE;
      }
      break;
    case GT:
      if (flags.Z == 0 and flags.N == flags.V) {
         return TRUE;
      }
      break;
    case LE:
      if (flags.Z == 1 or flags.N != flags.V) {
         return TRUE;
      }
      break;
    case AL:
      return TRUE;
      break;
  }
  return FALSE;
}

void execute() {
  Data16 instr = imem[PC];
  Data16 instr2;
  Data32 temp(0); // Use this for STRB instructions
  Thumb_Types itype;
  // the following counts as a read to PC
  // DONE
  unsigned int pctarget = PC + 2;
  stats.numRegReads += 1;
  unsigned int addr;
  int i, n, offset;
  unsigned int list, mask;
  int num1, num2, result, BitCount;
  unsigned int bit;

  /* Convert instruction to correct type */
  /* Types are described in Section A5 of the armv7 manual */
  BL_Type blupper(instr);
  ALU_Type alu(instr);
  SP_Type sp(instr);
  DP_Type dp(instr);
  LD_ST_Type ld_st(instr);
  MISC_Type misc(instr);
  COND_Type cond(instr);
  UNCOND_Type uncond(instr);
  LDM_Type ldm(instr);
  STM_Type stm(instr);
  LDRL_Type ldrl(instr);
  ADD_SP_Type addsp(instr);

  BL_Ops bl_ops;
  ALU_Ops add_ops;
  DP_Ops dp_ops;
  SP_Ops sp_ops;
  LD_ST_Ops ldst_ops;
  MISC_Ops misc_ops;

  // This counts as a write to the PC register
  // DONE
  rf.write(PC_REG, pctarget);
  stats.numRegWrites += 1;  

  itype = decode(ALL_Types(instr));

  // CPE 315: The bulk of your work is in the following switch statement
  // All instructions will need to have stats and cache access info added
  // as appropriate for that instruction.
  switch(itype) {
    case ALU:
      add_ops = decode(alu);
      switch(add_ops) {
        case ALU_LSLI:
          result = rf[alu.instr.lsli.rm] << alu.instr.lsli.imm;
          setNegativeZero(result);
          setCarryOverflow(rf[alu.instr.lsli.rm], alu.instr.lsli.imm, OF_SHIFT); 
          rf.write(alu.instr.lsli.rd, result);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_ADDR:
          // needs stats and flags
          // STATS DONE
          result = rf[alu.instr.addr.rn] + rf[alu.instr.addr.rm];
          setNegativeZero(result);
          setCarryOverflow(rf[alu.instr.addr.rn], rf[alu.instr.addr.rm], OF_ADD);
          rf.write(alu.instr.addr.rd, result);
          stats.numRegReads += 2;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_SUBR:
          // STATS DONE
          result = rf[alu.instr.subr.rn] - rf[alu.instr.subr.rm];
          //setNegativeZero(result);
          //setCarryOverflow(rf[alu.instr.subr.rn], rf[alu.instr.subr.rm], OF_SUB);
          rf.write(alu.instr.subr.rd, result);
          stats.numRegReads += 2;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_ADD3I:
          // needs stats and flags
          // STATS DONE
          result = rf[alu.instr.add3i.rn] + alu.instr.add3i.imm;
          setNegativeZero(result);
          setCarryOverflow(rf[alu.instr.add3i.rn], alu.instr.add3i.imm, OF_ADD);
          rf.write(alu.instr.add3i.rd, result);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_SUB3I:
          // STATS DONE
          result = rf[alu.instr.sub3i.rn] - alu.instr.sub3i.imm;
          //setNegativeZero(result);
          //setCarryOverflow(rf[alu.instr.sub3i.rn], alu.instr.sub3i.imm, OF_SUB);
          rf.write(alu.instr.sub3i.rd, result);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_MOV:
          // needs stats and flags
          // STATS DONE
          setNegativeZero(alu.instr.mov.imm);
          flags.C = 0;
          rf.write(alu.instr.mov.rdn, alu.instr.mov.imm);
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_CMP:
          // DONE
          setNegativeZero(rf[alu.instr.cmp.rdn] - alu.instr.cmp.imm);
          setCarryOverflow(rf[alu.instr.cmp.rdn], alu.instr.cmp.imm, OF_SUB);
          stats.numRegReads += 1;
          stats.instrs++;
          break;
        case ALU_ADD8I:
          // needs stats and flags
          // STATS DONE
          result = rf[alu.instr.add8i.rdn] + alu.instr.add8i.imm;
          setNegativeZero(result);
          setCarryOverflow(rf[alu.instr.add8i.rdn], alu.instr.add8i.imm, OF_ADD);
          rf.write(alu.instr.add8i.rdn, result);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case ALU_SUB8I:
          // STATS DONE
          result = rf[alu.instr.sub8i.rdn] - alu.instr.sub8i.imm;
          //setNegativeZero(result);
          //setCarryOverflow(rf[alu.instr.sub8i.rdn], alu.instr.sub8i.imm, OF_SUB);
          rf.write(alu.instr.sub8i.rdn, rf[alu.instr.sub8i.rdn] - alu.instr.sub8i.imm);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        default:
          cout << "instruction not implemented" << endl;
          exit(1);

      }
      break;
    case BL: 
      // This instruction is complete, nothing needed here
      bl_ops = decode(blupper);
      if (bl_ops == BL_UPPER) {
        // PC has already been incremented above
        instr2 = imem[PC];
        BL_Type bllower(instr2);
        if (blupper.instr.bl_upper.s) {
          addr = static_cast<unsigned int>(0xff<<24) | 
            ((~(bllower.instr.bl_lower.j1 ^ blupper.instr.bl_upper.s))<<23) |
            ((~(bllower.instr.bl_lower.j2 ^ blupper.instr.bl_upper.s))<<22) |
            ((blupper.instr.bl_upper.imm10)<<12) |
            ((bllower.instr.bl_lower.imm11)<<1);
        }
        else {
          addr = ((blupper.instr.bl_upper.imm10)<<12) |
            ((bllower.instr.bl_lower.imm11)<<1);
        }
        // return address is 4-bytes away from the start of the BL insn
        rf.write(LR_REG, PC + 2);
        // Target address is also computed from that point
        rf.write(PC_REG, PC + 2 + addr);

        stats.numRegReads += 1;
        stats.numRegWrites += 2; 
        stats.instrs++;
      }
      else {
        cerr << "Bad BL format." << endl;
        exit(1);
      }
      break;
    case DP:
      dp_ops = decode(dp);
      switch(dp_ops) {
        case DP_CMP:
          // need to implement
          setNegativeZero(rf[dp.instr.DP_Instr.rdn] - rf[dp.instr.DP_Instr.rm]);
          setCarryOverflow(rf[dp.instr.DP_Instr.rdn], rf[dp.instr.DP_Instr.rm], OF_SUB);
          stats.numRegReads += 2;
          stats.instrs++;
          break;
      }
      break;
    case SPECIAL:
      sp_ops = decode(sp);
      switch(sp_ops) {
        case SP_MOV:
          // needs stats and flags
          // STATS DONE
          rf.write((sp.instr.mov.d << 3 ) | sp.instr.mov.rd, rf[sp.instr.mov.rm]);
         stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case SP_ADD:
          // STATS DONE
          rf.write((sp.instr.add.d << 3) | sp.instr.add.rd, rf[(sp.instr.add.d << 3) | sp.instr.add.rd] + rf[sp.instr.add.rm]);
         // stats.numRegReads += 1;
          stats.numRegWrites += 1;
          //stats.instrs++;
        case SP_CMP:
          // need to implement these
          // DONE
          setNegativeZero(rf[(sp.instr.cmp.d << 3) | sp.instr.cmp.rd] - rf[sp.instr.cmp.rm]);
          setCarryOverflow(rf[(sp.instr.cmp.d << 3) | sp.instr.cmp.rd], rf[sp.instr.cmp.rm], OF_SUB);
          stats.numRegReads += 2;
          stats.instrs++;
          break;
      }
      break;
    case LD_ST:
      // You'll want to use these load and store models
      // to implement ldrb/strb, ldm/stm and push/pop
      ldst_ops = decode(ld_st);
      switch(ldst_ops) {
        case STRI:
          // functionally complete, needs stats
          // DONE
          addr = rf[ld_st.instr.ld_st_imm.rn] + ld_st.instr.ld_st_imm.imm * 4;
          caches.access(addr);
          dmem.write(addr, rf[ld_st.instr.ld_st_imm.rt]);
          stats.numRegReads += 2;
          stats.numMemWrites += 1;
          stats.instrs++;
          break;
        case LDRI:
          // functionally complete, needs stats
          // DONE
          addr = rf[ld_st.instr.ld_st_imm.rn] + ld_st.instr.ld_st_imm.imm * 4;
          caches.access(addr);
          rf.write(ld_st.instr.ld_st_imm.rt, dmem[addr]);
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.numMemReads += 1;
          stats.instrs++;
          break;
        case STRR:
          // need to implement
          addr = rf[ld_st.instr.ld_st_reg.rn] + (rf[ld_st.instr.ld_st_reg.rm]) ;
          caches.access(addr);
          dmem.write(addr, rf[ld_st.instr.ld_st_reg.rt]);
          stats.numRegReads += 3;
          stats.numMemWrites += 1;
          stats.instrs++;
          break;
        case LDRR:
          // need to implement
          addr = rf[ld_st.instr.ld_st_reg.rn] + (rf[ld_st.instr.ld_st_reg.rm]);
          caches.access(addr);
          rf.write(ld_st.instr.ld_st_reg.rt, dmem[addr]);
          stats.numRegReads += 2;
          stats.numRegWrites += 1;
          stats.numMemReads += 1;
          stats.instrs++;
          break;
        case STRBI:
          // need to implement
          addr = rf[ld_st.instr.ld_st_imm.rn] + ld_st.instr.ld_st_imm.imm;
          caches.access(addr);
          dmem.write(addr, (dmem[addr] & 0x00ffffff) | ((rf[ld_st.instr.ld_st_reg.rt] & 0x000000ff) << 24));
          stats.numRegReads += 2;
          stats.numMemWrites += 1;
          stats.instrs++;
          break;
        case LDRBI:
          // need to implement
          addr = rf[ld_st.instr.ld_st_reg.rn] + ld_st.instr.ld_st_imm.imm;
          caches.access(addr);
          rf.write(ld_st.instr.ld_st_imm.rt, (((dmem[addr] & 0xff000000) >> 24) & 0x000000ff));
          stats.numRegWrites += 1;
          stats.numRegReads += 1;
          stats.numMemReads += 1;
          stats.instrs++;
          break;
        case STRBR:
          // fix sign extend issue
          // need to implement
          addr = rf[ld_st.instr.ld_st_reg.rn] + rf[ld_st.instr.ld_st_reg.rm];
          caches.access(addr);
          dmem.write(addr, (dmem[addr] & 0x00ffffff) |((rf[ld_st.instr.ld_st_reg.rt] & 0x000000ff) << 24));
          stats.numRegReads += 3;
          stats.numMemWrites += 1;
          stats.instrs++;
          break;
        case LDRBR:
          // need to implement
          addr = rf[ld_st.instr.ld_st_reg.rn] + rf[ld_st.instr.ld_st_reg.rm];
          caches.access(addr);
          rf.write(ld_st.instr.ld_st_reg.rt, (((dmem[addr] & 0xff000000) >> 24) & 0x000000ff));
          stats.numRegReads += 2;
          stats.numRegWrites += 1;
          stats.numMemReads += 1;
          stats.instrs++;
          break;
      }
      break;
    case MISC:
      misc_ops = decode(misc);
      switch(misc_ops) {
        case MISC_PUSH:
            // need to implement
            // CHECK STATS
            addr = SP;
            if (misc.instr.push.m) {
                addr = addr - 4;
                caches.access(addr);
                dmem.write(addr, LR);

                stats.numRegReads += 1;
                stats.numMemWrites += 1;
            }
            for (i = 7; i >= 0; --i) {
                if (misc.instr.push.reg_list & (1 << i) ) {
                    addr = addr - 4;
                    caches.access(addr);
                    dmem.write(addr, rf[i]);

                    stats.numRegReads += 1;
                    stats.numMemWrites += 1;
                }
            }
            
            rf.write(SP_REG, addr);

            stats.numRegReads += 1;
            stats.numRegWrites += 1;
            stats.instrs++;
          break;
        case MISC_POP:
            // need to implement
            // CHECK STATS
            addr = SP;
            for (i = 0; i < 8; ++i) {
                if (misc.instr.pop.reg_list & (1 << i)) {
                    caches.access(addr);
                    rf.write(i, dmem[addr]);
                    addr = addr + 4;
                    stats.numRegWrites += 1;
                    stats.numMemReads += 1;
                }
            }
            if (misc.instr.pop.m) {
                caches.access(addr);
                rf.write(PC_REG, dmem[addr]);
                addr = addr + 4;
                stats.numMemReads += 1;
                stats.numRegWrites += 1;
            }
            rf.write(SP_REG, addr);
            stats.numRegWrites += 1;
            stats.numRegReads += 1;
            stats.instrs++;
          break;
        case MISC_SUB:
          // functionally complete, needs stats
          // STATS DONE (does SP count as a read?)
          rf.write(SP_REG, SP - (misc.instr.sub.imm*4));
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
        case MISC_ADD:
          // functionally complete, needs stats
          // STATS DONE (does SP count as a read?)
          rf.write(SP_REG, SP + (misc.instr.add.imm*4));
          stats.numRegReads += 1;
          stats.numRegWrites += 1;
          stats.instrs++;
          break;
      }
      break;
    case COND:
      decode(cond);
      // Once you've completed the checkCondition function,
      // this should work for all your conditional branches.
      // needs stats
      // STATS DONE (does PC count as a read?)
      if (PC < PC + 2 * signExtend8to32ui(cond.instr.b.imm)) {
        if (checkCondition(cond.instr.b.cond)) {
            stats.numForwardBranchesTaken += 1;
        } else {
            stats.numForwardBranchesNotTaken += 1;
        }
      } else {
        if (checkCondition(cond.instr.b.cond)) {
            stats.numBackwardBranchesTaken += 1;
        } else {
            stats.numBackwardBranchesNotTaken += 1;
        }
      }
      if (checkCondition(cond.instr.b.cond)){
        rf.write(PC_REG, PC + 2 * signExtend8to32ui(cond.instr.b.imm) + 2);
        stats.numRegWrites += 1;
        stats.numRegReads += 1;
      }
      stats.instrs++;
      break;
    case UNCOND:
      // Essentially the same as the conditional branches, but with no
      // condition check, and an 11-bit immediate field
      // STATS DONE 
      decode(uncond);
      rf.write(PC_REG, PC + 2 * signExtend11to32ui(uncond.instr.b.imm) + 2);
      stats.numRegReads += 1;
      stats.numRegWrites += 1;
      stats.instrs++;
      break;
    case LDM:
      decode(ldm);
      // DONE
      addr = rf[ldm.instr.ldm.rn];
      for (i = 0; i < 8; ++i) {
        if (ldm.instr.ldm.reg_list & (1 << i)) {
            caches.access(addr);
            rf.write(i, dmem[addr]);
            addr = addr + 4;
            stats.numRegWrites += 1;
            stats.numMemReads += 1;
        }
      }
      rf.write(ldm.instr.ldm.rn, addr);
      stats.numRegWrites += 1;
      stats.numRegReads += 1;
      stats.instrs++;
      break;
    case STM:
      decode(stm);
      // DONE
      addr = rf[stm.instr.stm.rn];
      for (i = 7; i >= 0; --i) {
        if (stm.instr.stm.reg_list & (1 << i)) {
            caches.access(addr);
            dmem.write(addr, rf[i]);
            addr = addr + 4;

            stats.numMemWrites += 1;
            stats.numRegReads += 1;
        }
      }
      rf.write(stm.instr.stm.rn, addr);
      stats.numRegWrites += 1;
      stats.numRegReads += 1;
      stats.instrs++;
      break;
    case LDRL:
      // This instruction is complete, nothing needed
      decode(ldrl);
      // Need to check for alignment by 4
      if (PC & 2) {
        addr = PC + 2 + (ldrl.instr.ldrl.imm)*4;
      }
      else {
        addr = PC + (ldrl.instr.ldrl.imm)*4;
      }
      // Requires two consecutive imem locations pieced together
      temp = imem[addr] | (imem[addr+2]<<16);  // temp is a Data32
      rf.write(ldrl.instr.ldrl.rt, temp);

      // One write for updated reg
      stats.numRegWrites++;
      // One read of the PC
      stats.numRegReads++;
      // One mem read, even though it's imem, and there's two of them
      stats.numMemReads++;
      stats.instrs++;
      break;
    case ADD_SP:
      // needs stats
      // STATS DONE (does SP count as a read?)
      decode(addsp);
      rf.write(addsp.instr.add.rd, SP + (addsp.instr.add.imm*4));
      stats.numRegReads += 1;
      stats.numRegWrites += 1;
      stats.instrs++;
      break;
    default:
      cout << "[ERROR] Unknown Instruction to be executed" << endl;
      exit(1);
      break;
  }
}
