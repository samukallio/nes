#define _CRT_SECURE_NO_WARNINGS
#include <stdarg.h>
#include <stdio.h>

#include "nes.h"

#define USING_CPU_REGISTERS \
	u8   &A  = CPU->A ; \
	u8   &X  = CPU->X ; \
	u8   &Y  = CPU->Y ; \
	u8   &M  = CPU->M ; \
	u8   &SP = CPU->SP; \
	u16  &R1 = CPU->R1; \
	u16  &R2 = CPU->R2; \
	u16  &R3 = CPU->R3; \
	u16  &PC = CPU->PC; \
	bool &CF = CPU->CF; \
	bool &ZF = CPU->ZF; \
	bool &IF = CPU->IF; \
	bool &DF = CPU->DF; \
	bool &BF = CPU->BF; \
	bool &VF = CPU->VF; \
	bool &NF = CPU->NF; \

enum cpu_state
{
	RESET                       = 0x00 << 3,
	FETCH                       = 0x01 << 3,
	INTERRUPT_JUMP              = 0x02 << 3,
	INTERRUPT_RETURN            = 0x03 << 3,
	SUBROUTINE_JUMP             = 0x04 << 3,
	SUBROUTINE_RETURN           = 0x05 << 3,
	STACK_PUSH                  = 0x06 << 3,
	STACK_PULL                  = 0x07 << 3,
	IMPLIED                     = 0x08 << 3,
	ACCUMULATOR                 = 0x09 << 3,
	IMMEDIATE                   = 0x0A << 3,
	RELATIVE_BRANCH             = 0x0B << 3,
	ZERO_PAGE_READ              = 0x0C << 3,
	ZERO_PAGE_MODIFY            = 0x0D << 3,
	ZERO_PAGE_WRITE             = 0x0E << 3,
	ZERO_PAGE_INDEXED_READ      = 0x0F << 3,
	ZERO_PAGE_INDEXED_MODIFY    = 0x10 << 3,
	ZERO_PAGE_INDEXED_WRITE     = 0x11 << 3,
	ABSOLUTE_JUMP               = 0x12 << 3,
	ABSOLUTE_READ               = 0x13 << 3,
	ABSOLUTE_MODIFY             = 0x14 << 3,
	ABSOLUTE_WRITE              = 0x15 << 3,
	ABSOLUTE_INDEXED_READ       = 0x16 << 3,
	ABSOLUTE_INDEXED_MODIFY     = 0x17 << 3,
	ABSOLUTE_INDEXED_WRITE      = 0x18 << 3,
	INDIRECT_JUMP               = 0x19 << 3,
	INDEXED_INDIRECT_READ       = 0x1A << 3,
	INDEXED_INDIRECT_MODIFY     = 0x1B << 3,
	INDEXED_INDIRECT_WRITE      = 0x1C << 3,
	INDIRECT_INDEXED_READ       = 0x1D << 3,
	INDIRECT_INDEXED_MODIFY     = 0x1E << 3,
	INDIRECT_INDEXED_WRITE      = 0x1F << 3,
};

enum cpu_operation : u8
{
	ADC, AHX, ALR, ANC, AND, ARR, ASL, AXS,
	BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK,
	BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX,
	CPY, DCP, DEC, DEX, DEY, EOR, INC, INX,
	INY, ISC, JMP, JSR, KIL, LAS, LAX, LDA,
	LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA,
	PLP, RLA, ROL, ROR, RRA, RTI, RTS, SAX,
	SBC, SEC, SED, SEI, SHX, SHY, SLO, SRE,
	STA, STX, STY, TAS, TAX, TAY, TSX, TXA,
	TXS, TYA, XAA,
};

const char* OperationNameTable[] =
{
	"ADC", "AHX", "ALR", "ANC", "AND", "ARR", "ASL", "AXS",
	"BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK",
	"BVC", "BVS", "CLC", "CLD", "CLI", "CLV", "CMP", "CPX",
	"CPY", "DCP", "DEC", "DEX", "DEY", "EOR", "INC", "INX",
	"INY", "ISC", "JMP", "JSR", "KIL", "LAS", "LAX", "LDA",
	"LDX", "LDY", "LSR", "NOP", "ORA", "PHA", "PHP", "PLA",
	"PLP", "RLA", "ROL", "ROR", "RRA", "RTI", "RTS", "SAX",
	"SBC", "SEC", "SED", "SEI", "SHX", "SHY", "SLO", "SRE",
	"STA", "STX", "STY", "TAS", "TAX", "TAY", "TSX", "TXA",
	"TXS", "TYA", "XAA",
};

constexpr bool INDEX_X = false;
constexpr bool INDEX_Y = true;

const cpu_instruction InstructionTable[256] =
{
	{ 0x00, BRK, INTERRUPT_JUMP                    },
	{ 0x01, ORA, INDEXED_INDIRECT_READ             },
	{ 0x02, KIL, IMPLIED                           },
	{ 0x03, SLO, INDEXED_INDIRECT_MODIFY           },
	{ 0x04, NOP, ZERO_PAGE_READ                    },
	{ 0x05, ORA, ZERO_PAGE_READ                    },
	{ 0x06, ASL, ZERO_PAGE_MODIFY                  },
	{ 0x07, SLO, ZERO_PAGE_MODIFY                  },
	{ 0x08, PHP, STACK_PUSH                        },
	{ 0x09, ORA, IMMEDIATE                         },
	{ 0x0A, ASL, ACCUMULATOR                       },
	{ 0x0B, ANC, IMMEDIATE                         },
	{ 0x0C, NOP, ABSOLUTE_READ                     },
	{ 0x0D, ORA, ABSOLUTE_READ                     },
	{ 0x0E, ASL, ABSOLUTE_MODIFY                   },
	{ 0x0F, SLO, ABSOLUTE_MODIFY                   },
	{ 0x10, BPL, RELATIVE_BRANCH                   },
	{ 0x11, ORA, INDIRECT_INDEXED_READ             },
	{ 0x12, KIL, IMPLIED                           },
	{ 0x13, SLO, INDIRECT_INDEXED_MODIFY           },
	{ 0x14, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x15, ORA, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x16, ASL, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x17, SLO, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x18, CLC, IMPLIED                           },
	{ 0x19, ORA, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0x1A, NOP, IMPLIED                           },
	{ 0x1B, SLO, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0x1C, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x1D, ORA, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x1E, ASL, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x1F, SLO, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x20, JSR, SUBROUTINE_JUMP                   },
	{ 0x21, AND, INDEXED_INDIRECT_READ             },
	{ 0x22, KIL, IMPLIED                           },
	{ 0x23, RLA, INDEXED_INDIRECT_MODIFY           },
	{ 0x24, BIT, ZERO_PAGE_READ                    },
	{ 0x25, AND, ZERO_PAGE_READ                    },
	{ 0x26, ROL, ZERO_PAGE_MODIFY                  },
	{ 0x27, RLA, ZERO_PAGE_MODIFY                  },
	{ 0x28, PLP, STACK_PULL                        },
	{ 0x29, AND, IMMEDIATE                         },
	{ 0x2A, ROL, ACCUMULATOR                       },
	{ 0x2B, ANC, IMMEDIATE                         },
	{ 0x2C, BIT, ABSOLUTE_READ                     },
	{ 0x2D, AND, ABSOLUTE_READ                     },
	{ 0x2E, ROL, ABSOLUTE_MODIFY                   },
	{ 0x2F, RLA, ABSOLUTE_MODIFY                   },
	{ 0x30, BMI, RELATIVE_BRANCH                   },
	{ 0x31, AND, INDIRECT_INDEXED_READ             },
	{ 0x32, KIL, IMPLIED                           },
	{ 0x33, RLA, INDIRECT_INDEXED_MODIFY           },
	{ 0x34, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x35, AND, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x36, ROL, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x37, RLA, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x38, SEC, IMPLIED                           },
	{ 0x39, AND, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0x3A, NOP, IMPLIED                           },
	{ 0x3B, RLA, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0x3C, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x3D, AND, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x3E, ROL, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x3F, RLA, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x40, RTI, INTERRUPT_RETURN                  },
	{ 0x41, EOR, INDEXED_INDIRECT_READ             },
	{ 0x42, KIL, IMPLIED                           },
	{ 0x43, SRE, INDEXED_INDIRECT_MODIFY           },
	{ 0x44, NOP, ZERO_PAGE_READ                    },
	{ 0x45, EOR, ZERO_PAGE_READ                    },
	{ 0x46, LSR, ZERO_PAGE_MODIFY                  },
	{ 0x47, SRE, ZERO_PAGE_MODIFY                  },
	{ 0x48, PHA, STACK_PUSH                        },
	{ 0x49, EOR, IMMEDIATE                         },
	{ 0x4A, LSR, ACCUMULATOR                       },
	{ 0x4B, ALR, IMMEDIATE                         },
	{ 0x4C, JMP, ABSOLUTE_JUMP                     },
	{ 0x4D, EOR, ABSOLUTE_READ                     },
	{ 0x4E, LSR, ABSOLUTE_MODIFY                   },
	{ 0x4F, SRE, ABSOLUTE_MODIFY                   },
	{ 0x50, BVC, RELATIVE_BRANCH                   },
	{ 0x51, EOR, INDIRECT_INDEXED_READ             },
	{ 0x52, KIL, IMPLIED                           },
	{ 0x53, SRE, INDIRECT_INDEXED_MODIFY           },
	{ 0x54, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x55, EOR, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x56, LSR, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x57, SRE, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x58, CLI, IMPLIED                           },
	{ 0x59, EOR, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0x5A, NOP, IMPLIED                           },
	{ 0x5B, SRE, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0x5C, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x5D, EOR, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x5E, LSR, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x5F, SRE, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x60, RTS, SUBROUTINE_RETURN                 },
	{ 0x61, ADC, INDEXED_INDIRECT_READ             },
	{ 0x62, KIL, IMPLIED                           },
	{ 0x63, RRA, INDEXED_INDIRECT_MODIFY           },
	{ 0x64, NOP, ZERO_PAGE_READ                    },
	{ 0x65, ADC, ZERO_PAGE_READ                    },
	{ 0x66, ROR, ZERO_PAGE_MODIFY                  },
	{ 0x67, RRA, ZERO_PAGE_MODIFY                  },
	{ 0x68, PLA, STACK_PULL                        },
	{ 0x69, ADC, IMMEDIATE                         },
	{ 0x6A, ROR, ACCUMULATOR                       },
	{ 0x6B, ARR, IMMEDIATE                         },
	{ 0x6C, JMP, INDIRECT_JUMP                     },
	{ 0x6D, ADC, ABSOLUTE_READ                     },
	{ 0x6E, ROR, ABSOLUTE_MODIFY                   },
	{ 0x6F, RRA, ABSOLUTE_MODIFY                   },
	{ 0x70, BVS, RELATIVE_BRANCH                   },
	{ 0x71, ADC, INDIRECT_INDEXED_READ             },
	{ 0x72, KIL, IMPLIED                           },
	{ 0x73, RRA, INDIRECT_INDEXED_MODIFY           },
	{ 0x74, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x75, ADC, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0x76, ROR, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x77, RRA, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0x78, SEI, IMPLIED                           },
	{ 0x79, ADC, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0x7A, NOP, IMPLIED                           },
	{ 0x7B, RRA, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0x7C, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x7D, ADC, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0x7E, ROR, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x7F, RRA, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0x80, NOP, IMMEDIATE                         },
	{ 0x81, STA, INDEXED_INDIRECT_WRITE            },
	{ 0x82, NOP, IMMEDIATE                         },
	{ 0x83, SAX, INDEXED_INDIRECT_WRITE            },
	{ 0x84, STY, ZERO_PAGE_WRITE                   },
	{ 0x85, STA, ZERO_PAGE_WRITE                   },
	{ 0x86, STX, ZERO_PAGE_WRITE                   },
	{ 0x87, SAX, ZERO_PAGE_WRITE                   },
	{ 0x88, DEY, IMPLIED                           },
	{ 0x89, NOP, IMMEDIATE                         },
	{ 0x8A, TXA, IMPLIED                           },
	{ 0x8B, XAA, IMMEDIATE                         },
	{ 0x8C, STY, ABSOLUTE_WRITE                    },
	{ 0x8D, STA, ABSOLUTE_WRITE                    },
	{ 0x8E, STX, ABSOLUTE_WRITE                    },
	{ 0x8F, SAX, ABSOLUTE_WRITE                    },
	{ 0x90, BCC, RELATIVE_BRANCH                   },
	{ 0x91, STA, INDIRECT_INDEXED_WRITE            },
	{ 0x92, KIL, IMPLIED                           },
	{ 0x93, AHX, INDIRECT_INDEXED_READ             },
	{ 0x94, STY, ZERO_PAGE_INDEXED_WRITE , INDEX_X },
	{ 0x95, STA, ZERO_PAGE_INDEXED_WRITE , INDEX_X },
	{ 0x96, STX, ZERO_PAGE_INDEXED_WRITE , INDEX_Y },
	{ 0x97, SAX, ZERO_PAGE_INDEXED_WRITE , INDEX_Y },
	{ 0x98, TYA, IMPLIED                           },
	{ 0x99, STA, ABSOLUTE_INDEXED_WRITE  , INDEX_Y },
	{ 0x9A, TXS, IMPLIED                           },
	{ 0x9B, TAS, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0x9C, SHY, ABSOLUTE_INDEXED_WRITE  , INDEX_X },
	{ 0x9D, STA, ABSOLUTE_INDEXED_WRITE  , INDEX_X },
	{ 0x9E, SHX, ABSOLUTE_INDEXED_WRITE  , INDEX_Y },
	{ 0x9F, AHX, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xA0, LDY, IMMEDIATE                         },
	{ 0xA1, LDA, INDEXED_INDIRECT_READ             },
	{ 0xA2, LDX, IMMEDIATE                         },
	{ 0xA3, LAX, INDEXED_INDIRECT_READ             },
	{ 0xA4, LDY, ZERO_PAGE_READ                    },
	{ 0xA5, LDA, ZERO_PAGE_READ                    },
	{ 0xA6, LDX, ZERO_PAGE_READ                    },
	{ 0xA7, LAX, ZERO_PAGE_READ                    },
	{ 0xA8, TAY, IMPLIED                           },
	{ 0xA9, LDA, IMMEDIATE                         },
	{ 0xAA, TAX, IMPLIED                           },
	{ 0xAB, LAX, IMMEDIATE                         },
	{ 0xAC, LDY, ABSOLUTE_READ                     },
	{ 0xAD, LDA, ABSOLUTE_READ                     },
	{ 0xAE, LDX, ABSOLUTE_READ                     },
	{ 0xAF, LAX, ABSOLUTE_READ                     },
	{ 0xB0, BCS, RELATIVE_BRANCH                   },
	{ 0xB1, LDA, INDIRECT_INDEXED_READ             },
	{ 0xB2, KIL, IMPLIED                           },
	{ 0xB3, LAX, INDIRECT_INDEXED_READ             },
	{ 0xB4, LDY, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xB5, LDA, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xB6, LDX, ZERO_PAGE_INDEXED_READ  , INDEX_Y },
	{ 0xB7, LAX, ZERO_PAGE_INDEXED_READ  , INDEX_Y },
	{ 0xB8, CLV, IMPLIED                           },
	{ 0xB9, LDA, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xBA, TSX, IMPLIED                           },
	{ 0xBB, LAS, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xBC, LDY, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xBD, LDA, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xBE, LDX, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xBF, LAX, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xC0, CPY, IMMEDIATE                         },
	{ 0xC1, CMP, INDEXED_INDIRECT_READ             },
	{ 0xC2, NOP, IMMEDIATE                         },
	{ 0xC3, DCP, INDEXED_INDIRECT_MODIFY           },
	{ 0xC4, CPY, ZERO_PAGE_READ                    },
	{ 0xC5, CMP, ZERO_PAGE_READ                    },
	{ 0xC6, DEC, ZERO_PAGE_MODIFY                  },
	{ 0xC7, DCP, ZERO_PAGE_MODIFY                  },
	{ 0xC8, INY, IMPLIED                           },
	{ 0xC9, CMP, IMMEDIATE                         },
	{ 0xCA, DEX, IMPLIED                           },
	{ 0xCB, AXS, IMMEDIATE                         },
	{ 0xCC, CPY, ABSOLUTE_READ                     },
	{ 0xCD, CMP, ABSOLUTE_READ                     },
	{ 0xCE, DEC, ABSOLUTE_MODIFY                   },
	{ 0xCF, DCP, ABSOLUTE_MODIFY                   },
	{ 0xD0, BNE, RELATIVE_BRANCH                   },
	{ 0xD1, CMP, INDIRECT_INDEXED_READ             },
	{ 0xD2, KIL, IMPLIED                           },
	{ 0xD3, DCP, INDIRECT_INDEXED_MODIFY           },
	{ 0xD4, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xD5, CMP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xD6, DEC, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0xD7, DCP, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0xD8, CLD, IMPLIED                           },
	{ 0xD9, CMP, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xDA, NOP, IMPLIED                           },
	{ 0xDB, DCP, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0xDC, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xDD, CMP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xDE, DEC, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0xDF, DCP, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0xE0, CPX, IMMEDIATE                         },
	{ 0xE1, SBC, INDEXED_INDIRECT_READ             },
	{ 0xE2, NOP, IMMEDIATE                         },
	{ 0xE3, ISC, INDEXED_INDIRECT_MODIFY           },
	{ 0xE4, CPX, ZERO_PAGE_READ                    },
	{ 0xE5, SBC, ZERO_PAGE_READ                    },
	{ 0xE6, INC, ZERO_PAGE_MODIFY                  },
	{ 0xE7, ISC, ZERO_PAGE_MODIFY                  },
	{ 0xE8, INX, IMPLIED                           },
	{ 0xE9, SBC, IMMEDIATE                         },
	{ 0xEA, NOP, IMPLIED                           },
	{ 0xEB, SBC, IMMEDIATE                         },
	{ 0xEC, CPX, ABSOLUTE_READ                     },
	{ 0xED, SBC, ABSOLUTE_READ                     },
	{ 0xEE, INC, ABSOLUTE_MODIFY                   },
	{ 0xEF, ISC, ABSOLUTE_MODIFY                   },
	{ 0xF0, BEQ, RELATIVE_BRANCH                   },
	{ 0xF1, SBC, INDIRECT_INDEXED_READ             },
	{ 0xF2, KIL, IMPLIED                           },
	{ 0xF3, ISC, INDIRECT_INDEXED_MODIFY           },
	{ 0xF4, NOP, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xF5, SBC, ZERO_PAGE_INDEXED_READ  , INDEX_X },
	{ 0xF6, INC, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0xF7, ISC, ZERO_PAGE_INDEXED_MODIFY, INDEX_X },
	{ 0xF8, SED, IMPLIED                           },
	{ 0xF9, SBC, ABSOLUTE_INDEXED_READ   , INDEX_Y },
	{ 0xFA, NOP, IMPLIED                           },
	{ 0xFB, ISC, ABSOLUTE_INDEXED_MODIFY , INDEX_Y },
	{ 0xFC, NOP, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xFD, SBC, ABSOLUTE_INDEXED_READ   , INDEX_X },
	{ 0xFE, INC, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
	{ 0xFF, ISC, ABSOLUTE_INDEXED_MODIFY , INDEX_X },
};

static void Operate(machine* Machine, u8 Operation)
{
	cpu* CPU = &Machine->CPU;
	USING_CPU_REGISTERS

	// Temporary registers.
	u32 R;

	switch (Operation) {
		case ISC: // unofficial
			M++;
			[[fallthrough]];
		case SBC:
		case ADC:
			if (Operation == ADC) {
				R = A + M + CF;
				VF = ~(A ^ M) & (A ^ R) & 0x80;
			}
			else {
				// SBC and ISC.
				R = A + (M ^ 0xFF) + CF;
				VF = ~(A ^ (M ^ 0xFF)) & (A ^ R) & 0x80;
			}
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = R > 0xFF;
			A = R;
			break;

		case AND:
			A &= M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case SLO: // unofficial
			R = M << 1;
			M = R;
			A |= M;
			ZF = A == 0;
			NF = A & 0x80;
			CF = R > 0xFF;
			break;

		case ASL:
			R = M << 1;
			M = R;
			ZF = M == 0;
			NF = M & 0x80;
			CF = R > 0xFF;
			break;

		case BIT:
			NF = M & 0x80;
			VF = M & 0x40;
			ZF = (A & M) == 0;
			break;

		case CLC:
			CF = false;
			break;

		case CLD:
			DF = false;
			break;

		case CLI:
			IF = false;
			break;

		case CLV:
			VF = false;
			break;

		case CMP:
			R = A - M;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = A >= M;
			break;

		case CPX:
			R = X - M;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = X >= M;
			break;

		case CPY:
			R = Y - M;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = Y >= M;
			break;

		case DCP: // unofficial
			M--;
			R = A - M;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = A >= M;
			break;

		case DEC:
			M--;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case DEX:
			X--;
			ZF = X == 0;
			NF = X & 0x80;
			break;

		case DEY:
			Y--;
			ZF = Y == 0;
			NF = Y & 0x80;
			break;

		case EOR:
			A ^= M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case INC:
			M++;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case INX:
			X++;
			ZF = X == 0;
			NF = X & 0x80;
			break;

		case INY:
			Y++;
			ZF = Y == 0;
			NF = Y & 0x80;
			break;

		case LAX: // unofficial
		case LDA:
			A = M;
			if (Operation == LAX) X = A;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case LDX:
			X = M;
			ZF = X == 0;
			NF = X & 0x80;
			break;

		case LDY:
			Y = M;
			ZF = Y == 0;
			NF = Y & 0x80;
			break;

		case SRE: // unofficial
		case LSR:
			CF = M & 0x01;
			M >>= 1;
			if (Operation == SRE) A ^= M;
			ZF = M == 0;
			NF = false;
			break;

		case NOP:
			break;

		case ORA:
			A |= M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case PHA:
			M = A;
			break;

		case PHP:
			M = 0x20;
			M |= u8(NF) << 7;
			M |= u8(VF) << 6;
			M |= u8(BF) << 4;
			M |= u8(DF) << 3;
			M |= u8(IF) << 2;
			M |= u8(ZF) << 1;
			M |= u8(CF) << 0;
			break;

		case PLA:
			A = M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case PLP:
			NF = M & 0x80;
			VF = M & 0x40;
			BF = true;
			DF = M & 0x08;
			IF = M & 0x04;
			ZF = M & 0x02;
			CF = M & 0x01;
			break;

		case RLA: // unofficial
		case ROL:
			R = (M << 1) | u8(CF);
			M = R;
			if (Operation == RLA) A &= M;
			CF = R > 0xFF;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case RRA: // unofficial
		case ROR:
			R = (M >> 1) | CF << 7;
			CF = M & 1;
			M = R;
			if (Operation == RRA) A = (A + M + CF) & 0xFF;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case SEC:
			CF = true;
			break;

		case SED:
			DF = true;
			break;

		case SEI:
			IF = true;
			break;

		case SAX: // unofficial
			M = A & X;
			break;

		case STA:
			M = A;
			break;

		case STX:
			M = X;
			break;

		case STY:
			M = Y;
			break;

		case TAX:
			X = A;
			ZF = X == 0;
			NF = X & 0x80;
			break;

		case TAY:
			Y = A;
			ZF = Y == 0;
			NF = Y & 0x80;
			break;

		case TXA:
			A = X;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case TYA:
			A = Y;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case TXS:
			SP = X;
			break;

		case TSX:
			X = SP;
			ZF = X == 0;
			NF = X & 0x80;
			break;
	}
}

static inline void Trace(machine* Machine)
{
	FILE* F = Machine->TraceFile;
	if (!F) return;

	cpu* CPU = &Machine->CPU;

	u8 Opcode = CPU->Instruction.Opcode;
	char const* OperationName = OperationNameTable[CPU->Instruction.Operation];
	char const* IndexName = CPU->Instruction.XY ? "Y" : "X";

	char C1[16] = {0};
	char C2[16] = {0};

	switch (CPU->State & ~7) {
		case RESET:
			sprintf(C2, "--- RESET ---");
			break;
		case STACK_PUSH:
		case STACK_PULL:
		case IMPLIED:
		case ACCUMULATOR:
		case SUBROUTINE_RETURN:
		case INTERRUPT_RETURN:
			sprintf(C1, "%02X", Opcode);
			sprintf(C2, OperationName);
			break;
		case INTERRUPT_JUMP:
			if (CPU->Interrupt == NO_INTERRUPT) {
				sprintf(C1, "%02X", Opcode);
				sprintf(C2, OperationName);
			}
			else {
				sprintf(C1, "--");
				sprintf(C2, "*** %s ***", (CPU->Interrupt == NMI ? "NMI" : "IRQ"));
			}
			break;
		case RELATIVE_BRANCH:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s $%02X", OperationName, CPU->R2);
			break;
		case IMMEDIATE:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s #$%02X", OperationName, CPU->R1);
			break;
		case ZERO_PAGE_READ:
		case ZERO_PAGE_MODIFY:
		case ZERO_PAGE_WRITE:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s $%02X", OperationName, CPU->R1);
			break;
		case ZERO_PAGE_INDEXED_READ:
		case ZERO_PAGE_INDEXED_MODIFY:
		case ZERO_PAGE_INDEXED_WRITE:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s $%02X,%s", OperationName, CPU->R1, IndexName);
			break;
		case SUBROUTINE_JUMP:
		case ABSOLUTE_JUMP:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->R1 & 0xFF, CPU->R1 >> 8);
			sprintf(C2, "%s $%04X", OperationName, CPU->R1);
			break;
		case ABSOLUTE_READ:
		case ABSOLUTE_MODIFY:
		case ABSOLUTE_WRITE:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->R1 & 0xFF, CPU->R1 >> 8);
			sprintf(C2, "%s $%04X", OperationName, CPU->R1);
			break;
		case ABSOLUTE_INDEXED_READ:
		case ABSOLUTE_INDEXED_MODIFY:
		case ABSOLUTE_INDEXED_WRITE:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->R1 & 0xFF, CPU->R1 >> 8);
			sprintf(C2, "%s $%04X,%s", OperationName, CPU->R1, IndexName);
			break;
		case INDIRECT_JUMP:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->R1 & 0xFF, CPU->R1 >> 8);
			sprintf(C2, "%s ($%04X)", OperationName, CPU->R1);
			break;
		case INDEXED_INDIRECT_READ:
		case INDEXED_INDIRECT_MODIFY:
		case INDEXED_INDIRECT_WRITE:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s (%02X,X)", OperationName, CPU->R1);
			break;
		case INDIRECT_INDEXED_READ:
		case INDIRECT_INDEXED_MODIFY:
		case INDIRECT_INDEXED_WRITE:
			sprintf(C1, "%02X %02X", Opcode, CPU->R1);
			sprintf(C2, "%s (%02X),Y", OperationName, CPU->R1);
			break;
	}

	fprintf(F,
		"%04X  %-8s  %-30s A:%02X X:%02X Y:%02X SP:%02X\n",
		CPU->R0, C1, C2, CPU->A, CPU->X, CPU->Y, CPU->SP);

	Machine->TraceLine++;
}

#define STALL if (Stalled) break;

void StepCPU(machine* Machine)
{
	cpu* CPU = &Machine->CPU;
	USING_CPU_REGISTERS

	u8 State = CPU->State;
	u8 Operation = CPU->Instruction.Operation;
	bool XY = CPU->Instruction.XY;
	bool PreviousIF = IF;

	bool Stalled = CPU->Stall > 0;
	if (Stalled)
		CPU->Stall--;

	switch (State) {
		// --- Reset ----------------------------------------------------------

		case 0 | RESET:
		case 1 | RESET:
			STALL;
			Read(Machine, PC);
			break;
		case 2 | RESET:
		case 3 | RESET:
		case 4 | RESET:
			STALL;
			Read(Machine, 0x100 | SP--);
			break;
		case 5 | RESET:
			STALL;
			PC = Read(Machine, 0xFFFC);
			IF = true;
			break;
		case 6 | RESET:
			STALL;
			PC |= Read(Machine, 0xFFFD) << 8;
			State = FETCH;
			break;

		// --- Fetch ----------------------------------------------------------

		case 0 | FETCH:
			STALL;
			CPU->R0 = PC;
			if (CPU->Interrupt) {
				Read(Machine, PC);
				// Start interrupt sequence for IRQ.
				State = INTERRUPT_JUMP;
			}
			else {
				CPU->Instruction = InstructionTable[Read(Machine, PC++)];
				State = CPU->Instruction.State;
				//printf("I %s\n", OperationNameTable[CPU->Instruction.Operation]);
			}
			break;

		// --- Interrupt Jump -------------------------------------------------

		// Used for NMI, IRQ, and the BRK instruction.
		case 1 | INTERRUPT_JUMP:
			STALL;
			Read(Machine, PC);
			if (Operation == BRK) PC++;
			break;
		case 2 | INTERRUPT_JUMP:
			Write(Machine, 0x100 | SP--, PC >> 8);
			break;
		case 3 | INTERRUPT_JUMP:
			Write(Machine, 0x100 | SP--, PC & 0xFF);
			break;
		case 4 | INTERRUPT_JUMP:
			switch (CPU->Interrupt) {
				case NMI:
					R1 = 0xFFFA;
					BF = Operation == BRK;
					Operate(Machine, PHP);
					BF = true;
					CPU->InternalNMI = false;
					break;

				case IRQ:
					R1 = 0xFFFE;
					BF = Operation == BRK;
					Operate(Machine, PHP);
					IF = true;
					BF = true;
					break;

				case NO_INTERRUPT: // BRK
					R1 = 0xFFFE;
					BF = true;
					Operate(Machine, PHP);
					IF = true;
					break;
			}
			Trace(Machine);
			Write(Machine, 0x100 | SP--, M);
			CPU->Interrupt = NO_INTERRUPT;
			break;
		case 5 | INTERRUPT_JUMP:
			STALL;
			PC = Read(Machine, R1);
			break;
		case 6 | INTERRUPT_JUMP:
			STALL;
			PC |= Read(Machine, R1+1) << 8;
			break;
		case 7 | INTERRUPT_JUMP:
			STALL;
			// An interrupt sequence does not poll the NMI or IRQ detectors
			// at the end, so instead of going to FETCH, handle next opcode
			// fetch here.
			CPU->R0 = PC;
			CPU->Instruction = InstructionTable[Read(Machine, PC++)];
			State = CPU->Instruction.State;
			break;

		// --- Return from Interrupt ------------------------------------------

		case 1 | INTERRUPT_RETURN:
			STALL;
			Read(Machine, PC);
			break;
		case 2 | INTERRUPT_RETURN:
			STALL;
			Read(Machine, 0x100 | SP++);
			break;
		case 3 | INTERRUPT_RETURN:
			STALL;
			M = Read(Machine, 0x100 | SP++);
			Operate(Machine, PLP);
			break;
		case 4 | INTERRUPT_RETURN:
			STALL;
			PC = Read(Machine, 0x100 | SP++);
			break;
		case 5 | INTERRUPT_RETURN:
			STALL;
			PC |= Read(Machine, 0x100 | SP) << 8;
			Trace(Machine);
			State = FETCH;
			break;

		// --- Jump to Subroutine ---------------------------------------------

		case 1 | SUBROUTINE_JUMP:
			STALL;
			R1 = Read(Machine, PC++);
			break;
		case 2 | SUBROUTINE_JUMP:
			STALL;
			Read(Machine, 0x100 | SP);
			break;
		case 3 | SUBROUTINE_JUMP:
			Write(Machine, 0x100 | SP--, PC >> 8);
			break;
		case 4 | SUBROUTINE_JUMP:
			Write(Machine, 0x100 | SP--, PC & 0xFF);
			break;
		case 5 | SUBROUTINE_JUMP:
			STALL;
			R1 |= Read(Machine, PC) << 8;
			PC = R1;
			Trace(Machine);
			State = FETCH;
			break;

		// --- Return from Subroutine -----------------------------------------

		case 1 | SUBROUTINE_RETURN:
			STALL;
			Read(Machine, PC);
			break;
		case 2 | SUBROUTINE_RETURN:
			STALL;
			Read(Machine, 0x100 | SP++);
			break;
		case 3 | SUBROUTINE_RETURN:
			STALL;
			PC = Read(Machine, 0x100 | SP++);
			break;
		case 4 | SUBROUTINE_RETURN:
			STALL;
			PC |= Read(Machine, 0x100 | SP) << 8;
			break;
		case 5 | SUBROUTINE_RETURN:
			STALL;
			Read(Machine, PC++);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Stack Push -----------------------------------------------------

		case 1 | STACK_PUSH:
			STALL;
			Read(Machine, PC);
			break;
		case 2 | STACK_PUSH:
			Operate(Machine, Operation);
			Write(Machine, 0x100 | SP--, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Stack Pull -----------------------------------------------------

		case 1 | STACK_PULL:
			STALL;
			Read(Machine, PC);
			break;
		case 2 | STACK_PULL:
			STALL;
			Read(Machine, 0x100 | SP++);
			break;
		case 3 | STACK_PULL:
			STALL;
			M = Read(Machine, 0x100 | SP);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Implied --------------------------------------------------------

		case 1 | IMPLIED:
			STALL;
			Read(Machine, PC);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Accumulator ----------------------------------------------------

		case 1 | ACCUMULATOR:
			STALL;
			Read(Machine, PC);
			M = A;
			Operate(Machine, Operation);
			Trace(Machine);
			A = M;
			State = FETCH;
			break;

		// --- Immediate ------------------------------------------------------

		case 1 | IMMEDIATE:
			STALL;
			R1 = Read(Machine, PC++);
			M = R1 & 0xFF;
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Branch ---------------------------------------------------------

		case 1 | RELATIVE_BRANCH:
			STALL;
			R1 = Read(Machine, PC++);
			// Compute new program counter.
			R2 = PC + R1;
			if (R1 & 0x80) R2 -= 0x100;
			// If branch not taken, go fetch next instruction.
			switch (Operation) {
				case BCC: if ( CF) State = FETCH; break;
				case BCS: if (!CF) State = FETCH; break;
				case BNE: if ( ZF) State = FETCH; break;
				case BEQ: if (!ZF) State = FETCH; break;
				case BPL: if ( NF) State = FETCH; break;
				case BMI: if (!NF) State = FETCH; break;
				case BVC: if ( VF) State = FETCH; break;
				case BVS: if (!VF) State = FETCH; break;
			}
			if (State == FETCH) Trace(Machine);
			break;

		case 2 | RELATIVE_BRANCH:
			STALL;
			// Dummy read next opcode.
			Read(Machine, PC);
			// Check for page-crossing branch.
			if ((R2 & 0xFF00) == (PC & 0xFF00)) {
				// Branch target is on the same page, so we're done.
				PC = R2;
				Trace(Machine);
				State = FETCH;
			}
			break;

		case 3 | RELATIVE_BRANCH:
			STALL;
			// Dummy read opcode using old PCH.
			Read(Machine, (PC & 0xFF00) | (R2 & 0x00FF));
			// Finally, we have the fixed PC.
			PC = R2;
			Trace(Machine);
			State = FETCH;
			break;

		// --- Zero Page ------------------------------------------------------

		// R1 = Zero page address

		// Common cycles.
		case 1 | ZERO_PAGE_READ:
		case 1 | ZERO_PAGE_MODIFY:
		case 1 | ZERO_PAGE_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		// Read cycles.
		case 2 | ZERO_PAGE_READ:
			STALL;
			M = Read(Machine, R1);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Modify cycles.
		case 2 | ZERO_PAGE_MODIFY:
			STALL;
			M = Read(Machine, R1);
			break;
		case 3 | ZERO_PAGE_MODIFY:
			Write(Machine, R1, M);
			Operate(Machine, Operation);
			break;
		case 4 | ZERO_PAGE_MODIFY:
			Write(Machine, R1, M);
			Trace(Machine);
			State = FETCH;
			break;

		// Write cycles.
		case 2 | ZERO_PAGE_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R1, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Zero Page Indexed ----------------------------------------------

		// Common cycles.
		case 1 | ZERO_PAGE_INDEXED_READ:
		case 1 | ZERO_PAGE_INDEXED_MODIFY:
		case 1 | ZERO_PAGE_INDEXED_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		case 2 | ZERO_PAGE_INDEXED_READ:
		case 2 | ZERO_PAGE_INDEXED_MODIFY:
		case 2 | ZERO_PAGE_INDEXED_WRITE:
			STALL;
			Read(Machine, R1);
			R2 = R1 + (XY ? Y : X);
			R2 &= 0xFF;
			break;

		// Read cycles.
		case 3 | ZERO_PAGE_INDEXED_READ:
			STALL;
			M = Read(Machine, R2);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Modify cycles.
		case 3 | ZERO_PAGE_INDEXED_MODIFY:
			STALL;
			M = Read(Machine, R2);
			break;
		case 4 | ZERO_PAGE_INDEXED_MODIFY:
			// Write back the value we just read.
			Write(Machine, R2, M);
			Operate(Machine, Operation);
			break;
		case 5 | ZERO_PAGE_INDEXED_MODIFY:
			// Write the actual result of the operation.
			Write(Machine, R2, M);
			Trace(Machine);
			State = FETCH;
			break;

		// Write cycles.
		case 3 | ZERO_PAGE_INDEXED_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R2, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Absolute Jump --------------------------------------------------

		case 1 | ABSOLUTE_JUMP:
			STALL;
			R1 = Read(Machine, PC++);
			break;
		case 2 | ABSOLUTE_JUMP:
			STALL;
			R1 |= Read(Machine, PC) << 8;
			PC = R1;
			Trace(Machine);
			State = FETCH;
			break;

		// --- Absolute -------------------------------------------------------

		// Common cycles.
		case 1 | ABSOLUTE_READ:
		case 1 | ABSOLUTE_MODIFY:
		case 1 | ABSOLUTE_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		case 2 | ABSOLUTE_READ:
		case 2 | ABSOLUTE_MODIFY:
		case 2 | ABSOLUTE_WRITE:
			STALL;
			R1 |= Read(Machine, PC++) << 8;
			break;

		// Read cycles.
		case 3 | ABSOLUTE_READ:
			STALL;
			M = Read(Machine, R1);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Read cycles.
		case 3 | ABSOLUTE_MODIFY:
			STALL;
			M = Read(Machine, R1);
			break;
		case 4 | ABSOLUTE_MODIFY:
			Write(Machine, R1, M);
			Operate(Machine, Operation);
			break;
		case 5 | ABSOLUTE_MODIFY:
			Write(Machine, R1, M);
			Trace(Machine);
			State = FETCH;
			break;

		// Write cycles.
		case 3 | ABSOLUTE_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R1, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Absolute Indexed -----------------------------------------------

		// Common cycles.
		case 1 | ABSOLUTE_INDEXED_READ:
		case 1 | ABSOLUTE_INDEXED_MODIFY:
		case 1 | ABSOLUTE_INDEXED_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		case 2 | ABSOLUTE_INDEXED_READ:
		case 2 | ABSOLUTE_INDEXED_MODIFY:
		case 2 | ABSOLUTE_INDEXED_WRITE:
			STALL;
			R1 |= Read(Machine, PC++) << 8;
			R2 = R1 + (XY ? Y : X);
			break;

		case 3 | ABSOLUTE_INDEXED_READ:
		case 3 | ABSOLUTE_INDEXED_MODIFY:
		case 3 | ABSOLUTE_INDEXED_WRITE:
			STALL;
			M = Read(Machine, (R1 & 0xFF00) | (R2 & 0x00FF));
			if ((R1 ^ R2) < 0x100) {
				// No page boundary crossing, if reading then we're done.
				if ((State & 0xF8) == ABSOLUTE_INDEXED_READ) {
					Operate(Machine, Operation);
					Trace(Machine);
					State = FETCH;
				}
			}
			break;

		// Read cycles.
		case 4 | ABSOLUTE_INDEXED_READ:
			STALL;
			M = Read(Machine, R2);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Modify cycles.
		case 4 | ABSOLUTE_INDEXED_MODIFY:
			STALL;
			M = Read(Machine, R2);
			break;
		case 5 | ABSOLUTE_INDEXED_MODIFY:
			Write(Machine, R2, M);
			Operate(Machine, Operation);
			break;
		case 6 | ABSOLUTE_INDEXED_MODIFY:
			Write(Machine, R2, M);
			State = FETCH;
			Trace(Machine);
			break;

		// Write cycles.
		case 4 | ABSOLUTE_INDEXED_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R2, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Indirect Jump --------------------------------------------------

		case 1 | INDIRECT_JUMP:
			STALL;
			R1 = Read(Machine, PC++);
			break;
		case 2 | INDIRECT_JUMP:
			STALL;
			R1 |= Read(Machine, PC++) << 8;
			break;
		case 3 | INDIRECT_JUMP:
			STALL;
			R2 = Read(Machine, R1);
			break;
		case 4 | INDIRECT_JUMP:
			STALL;
			R2 |= Read(Machine, (R1 & 0xFF00) | ((R1 + 1) & 0x00FF)) << 8;
			PC = R2;
			Trace(Machine);
			State = FETCH;
			break;

		// --- Indexed Indirect -----------------------------------------------

		// Common cycles.
		case 1 | INDEXED_INDIRECT_READ:
		case 1 | INDEXED_INDIRECT_MODIFY:
		case 1 | INDEXED_INDIRECT_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		case 2 | INDEXED_INDIRECT_READ:
		case 2 | INDEXED_INDIRECT_MODIFY:
		case 2 | INDEXED_INDIRECT_WRITE:
			STALL;
			Read(Machine, R1);
			R2 = (R1 + X) & 0xFF;
			break;

		case 3 | INDEXED_INDIRECT_READ:
		case 3 | INDEXED_INDIRECT_MODIFY:
		case 3 | INDEXED_INDIRECT_WRITE:
			STALL;
			R3 = Read(Machine, R2);
			break;

		case 4 | INDEXED_INDIRECT_READ:
		case 4 | INDEXED_INDIRECT_MODIFY:
		case 4 | INDEXED_INDIRECT_WRITE:
			STALL;
			R3 |= Read(Machine, (R2+1) & 0xFF) << 8;
			break;

		// Read cycles.
		case 5 | INDEXED_INDIRECT_READ:
			STALL;
			M = Read(Machine, R3);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Modify cycles.
		case 5 | INDEXED_INDIRECT_MODIFY:
			STALL;
			M = Read(Machine, R3);
			break;
		case 6 | INDEXED_INDIRECT_MODIFY:
			Write(Machine, R3, M);
			Operate(Machine, Operation);
			break;
		case 7 | INDEXED_INDIRECT_MODIFY:
			Write(Machine, R3, M);
			State = FETCH;
			Trace(Machine);
			break;

		// Write cycles.
		case 5 | INDEXED_INDIRECT_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R3, M);
			Trace(Machine);
			State = FETCH;
			break;

		// --- Indirect Indexed -----------------------------------------------

		// Common cycles.
		case 1 | INDIRECT_INDEXED_READ:
		case 1 | INDIRECT_INDEXED_MODIFY:
		case 1 | INDIRECT_INDEXED_WRITE:
			STALL;
			R1 = Read(Machine, PC++);
			break;

		case 2 | INDIRECT_INDEXED_READ:
		case 2 | INDIRECT_INDEXED_MODIFY:
		case 2 | INDIRECT_INDEXED_WRITE:
			STALL;
			R2 = Read(Machine, R1);
			break;

		case 3 | INDIRECT_INDEXED_READ:
		case 3 | INDIRECT_INDEXED_MODIFY:
		case 3 | INDIRECT_INDEXED_WRITE:
			STALL;
			R2 |= Read(Machine, (R1+1) & 0xFF) << 8;
			R3 = R2 + Y;
			break;

		case 4 | INDIRECT_INDEXED_READ:
		case 4 | INDIRECT_INDEXED_MODIFY:
		case 4 | INDIRECT_INDEXED_WRITE:
			STALL;
			M = Read(Machine, (R2 & 0xFF00) | (R3 & 0x00FF));
			if ((R2 ^ R3) < 0x100) {
				// No page boundary crossing, if reading then we're done.
				if ((State & 0xF8) == INDIRECT_INDEXED_READ) {
					Operate(Machine, Operation);
					Trace(Machine);
					State = FETCH;
				}
			}
			break;

		// Read cycles.
		case 5 | INDIRECT_INDEXED_READ:
			STALL;
			M = Read(Machine, R3);
			Operate(Machine, Operation);
			Trace(Machine);
			State = FETCH;
			break;

		// Modify cycles.
		case 5 | INDIRECT_INDEXED_MODIFY:
			STALL;
			M = Read(Machine, R3);
			break;
		case 6 | INDIRECT_INDEXED_MODIFY:
			Write(Machine, R3, M);
			Operate(Machine, Operation);
			break;
		case 7 | INDIRECT_INDEXED_MODIFY:
			Write(Machine, R3, M);
			Trace(Machine);
			State = FETCH;
			break;

		// Write cycles.
		case 5 | INDIRECT_INDEXED_WRITE:
			Operate(Machine, Operation);
			Write(Machine, R3, M);
			Trace(Machine);
			State = FETCH;
			break;
	}

	// Poll for interrupts at the end of an instruction, or during branch
	// cycles 0 (opcode fetch) or 2 (taken branch, before PCH fixup).
	bool PollForInterrupts =
		State == FETCH ||
		State == RELATIVE_BRANCH+0 ||
		State == RELATIVE_BRANCH+2 ||
		State == INTERRUPT_JUMP+0 ||
		State == INTERRUPT_JUMP+1 ||
		State == INTERRUPT_JUMP+2 ||
		State == INTERRUPT_JUMP+3;

	if (PollForInterrupts) {
		if (CPU->InternalNMI) {
			// Trigger NMI.
			CPU->Interrupt = NMI;
		}
		else if (CPU->InternalIRQ) {
			// CLI, SEI and PLP modify the interrupt flag after polling for interrupts.
			if (Operation == CLI || Operation == SEI || Operation == PLP) {
				// Trigger IRQ, if interrupt flag was set.
				if (!PreviousIF) CPU->Interrupt = IRQ;
			}
			else {
				// Trigger IRQ, if interrupt flag is set.
				if (!IF) CPU->Interrupt = IRQ;
			}
		}
	}

	// If still executing operation, go to next step.
	if (!Stalled && State != FETCH) State++;

	// Set new state and go to next cycle.
	CPU->State = State;
	CPU->Cycle += 1;
}

void StepCPUPhase2(machine* Machine)
{
	cpu* CPU = &Machine->CPU;

	// --- PHI 2 ---

	// Update NMI edge detector.
	if (CPU->NMI && !CPU->PreviousNMI) {
		// NMI request edge detected, raise internal NMI
		// flag that takes effect from the next cycle.
		CPU->InternalNMI = true;
	}
	CPU->PreviousNMI = CPU->NMI;

	// Update IRQ level detector.
	CPU->InternalIRQ = CPU->IRQ;
}