// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.


// Optimization ideas:
//
// It's common to see sequences of stores writing or reading to a contiguous set of
// addresses in function prologues/epilogues:
//  sw s5, 104(sp)
//  sw s4, 100(sp)
//  sw s3, 96(sp)
//  sw s2, 92(sp)
//  sw s1, 88(sp)
//  sw s0, 84(sp)
//  sw ra, 108(sp)
//  mov s4, a0
//  mov s3, a1
//  ...
// Such sequences could easily be detected and turned into nice contiguous
// sequences of ARM stores instead of the current 3 instructions per sw/lw.
//
// Also, if we kept track of the likely register content of a cached register,
// (pointer or data), we could avoid many BIC instructions.


#include "Core/MemMap.h"
#include "Core/Config.h"
#include "Core/MIPS/MIPS.h"
#include "Core/MIPS/MIPSAnalyst.h"
#include "Core/MIPS/MIPSCodeUtils.h"
#include "Core/MIPS/IR/IRFrontend.h"
#include "Core/MIPS/IR/IRRegCache.h"

#define _RS MIPS_GET_RS(op)
#define _RT MIPS_GET_RT(op)
#define _RD MIPS_GET_RD(op)
#define _FS MIPS_GET_FS(op)
#define _FT MIPS_GET_FT(op)
#define _FD MIPS_GET_FD(op)
#define _SA MIPS_GET_SA(op)
#define _POS  ((op>> 6) & 0x1F)
#define _SIZE ((op>>11) & 0x1F)
#define _IMM16 (signed short)(op & 0xFFFF)
#define _IMM26 (op & 0x03FFFFFF)

// All functions should have CONDITIONAL_DISABLE, so we can narrow things down to a file quickly.
// Currently known non working ones should have DISABLE.

// #define CONDITIONAL_DISABLE { Comp_Generic(op); return; }
#define CONDITIONAL_DISABLE ;
#define DISABLE { Comp_Generic(op); return; }

namespace MIPSComp {
	void IRFrontend::Comp_ITypeMemLR(MIPSOpcode op, bool load) {
		DISABLE;
	}

	void IRFrontend::Comp_ITypeMem(MIPSOpcode op) {
		CONDITIONAL_DISABLE;

		int offset = (signed short)(op & 0xFFFF);
		MIPSGPReg rt = _RT;
		MIPSGPReg rs = _RS;
		int o = op >> 26;
		if (((op >> 29) & 1) == 0 && rt == MIPS_REG_ZERO) {
			// Don't load anything into $zr
			return;
		}

		int addrReg = IRTEMP_0;
		switch (o) {
			// Load
		case 35:
			ir.Write(IROp::Load32, rt, rs, ir.AddConstant(offset));
			break;
		case 37:
			ir.Write(IROp::Load16, rt, rs, ir.AddConstant(offset));
			break;
		case 33:
			ir.Write(IROp::Load16Ext, rt, rs, ir.AddConstant(offset));
			break;
		case 36:
			ir.Write(IROp::Load8, rt, rs, ir.AddConstant(offset));
			break;
		case 32:
			ir.Write(IROp::Load8Ext, rt, rs, ir.AddConstant(offset));
			break;
			// Store
		case 43:
			ir.Write(IROp::Store32, rt, rs, ir.AddConstant(offset));
			break;
		case 41:
			ir.Write(IROp::Store16, rt, rs, ir.AddConstant(offset));
			break;
		case 40:
			ir.Write(IROp::Store8, rt, rs, ir.AddConstant(offset));
			break;

		case 34: //lwl
		case 38: //lwr
		case 42: //swl
		case 46: //swr
			DISABLE;
			break;
		default:
			Comp_Generic(op);
			return;
		}
	}

	void IRFrontend::Comp_Cache(MIPSOpcode op) {
//		int imm = (s16)(op & 0xFFFF);
//		int rs = _RS;
//		int addr = R(rs) + imm;
		int func = (op >> 16) & 0x1F;

		// It appears that a cache line is 0x40 (64) bytes, loops in games
		// issue the cache instruction at that interval.

		// These codes might be PSP-specific, they don't match regular MIPS cache codes very well
		switch (func) {
			// Icache
		case 8:
			// Invalidate the instruction cache at this address
			DISABLE;
			break;
			// Dcache
		case 24:
			// "Create Dirty Exclusive" - for avoiding a cacheline fill before writing to it.
			// Will cause garbage on the real machine so we just ignore it, the app will overwrite the cacheline.
			break;
		case 25:  // Hit Invalidate - zaps the line if present in cache. Should not writeback???? scary.
			// No need to do anything.
			break;
		case 27:  // D-cube. Hit Writeback Invalidate.  Tony Hawk Underground 2
			break;
		case 30:  // GTA LCS, a lot. Fill (prefetch).   Tony Hawk Underground 2
			break;

		default:
			DISABLE;
			break;
		}
	}
}
