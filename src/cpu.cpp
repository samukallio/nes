#define _CRT_SECURE_NO_WARNINGS
#include <stdarg.h>
#include <stdio.h>

#include "nes.h"

#define USING_CPU_REGISTERS \
	u8   &A  = CPU->A ; \
	u8   &X  = CPU->X ; \
	u8   &Y  = CPU->Y ; \
	u8   &SP = CPU->SP; \
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
	RESET             = 0x00 << 3,
	FETCH             = 0x01 << 3,
	FETCH_NO_POLL     = 0x02 << 3,
	INTERRUPT_JUMP    = 0x03 << 3,
	INTERRUPT_RETURN  = 0x04 << 3,
	SUBROUTINE_JUMP   = 0x05 << 3,
	SUBROUTINE_RETURN = 0x06 << 3,
	STACK_PUSH        = 0x07 << 3,
	STACK_PULL        = 0x08 << 3,
	IMPLIED           = 0x09 << 3,
	ACCUMULATOR       = 0x0A << 3,
	IMMEDIATE         = 0x0B << 3,
	BRANCH            = 0x0C << 3,
	ABSOLUTE_JUMP     = 0x0D << 3,
	INDIRECT_JUMP     = 0x0E << 3,
	ZERO_PAGE         = 0x0F << 3,
	ZERO_PAGE_X       = 0x10 << 3,
	ZERO_PAGE_Y       = 0x11 << 3,
	ABSOLUTE          = 0x12 << 3,
	ABSOLUTE_X        = 0x13 << 3,
	ABSOLUTE_Y        = 0x14 << 3,
	INDEXED_INDIRECT  = 0x15 << 3,
	INDIRECT_INDEXED  = 0x16 << 3,
	READ              = 0x17 << 3,
	MODIFY            = 0x18 << 3,
	WRITE             = 0x19 << 3,
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

const cpu_instruction InstructionTable[256] =
{
	{ 0x00, BRK, INTERRUPT_JUMP           },
	{ 0x01, ORA, INDEXED_INDIRECT, READ   },
	{ 0x02, KIL, IMPLIED                  },
	{ 0x03, SLO, INDEXED_INDIRECT, MODIFY },
	{ 0x04, NOP, ZERO_PAGE, READ          },
	{ 0x05, ORA, ZERO_PAGE, READ          },
	{ 0x06, ASL, ZERO_PAGE, MODIFY        },
	{ 0x07, SLO, ZERO_PAGE, MODIFY        },
	{ 0x08, PHP, STACK_PUSH               },
	{ 0x09, ORA, IMMEDIATE                },
	{ 0x0A, ASL, ACCUMULATOR              },
	{ 0x0B, ANC, IMMEDIATE                },
	{ 0x0C, NOP, ABSOLUTE, READ           },
	{ 0x0D, ORA, ABSOLUTE, READ           },
	{ 0x0E, ASL, ABSOLUTE, MODIFY         },
	{ 0x0F, SLO, ABSOLUTE, MODIFY         },
	{ 0x10, BPL, BRANCH                   },
	{ 0x11, ORA, INDIRECT_INDEXED, READ   },
	{ 0x12, KIL, IMPLIED                  },
	{ 0x13, SLO, INDIRECT_INDEXED, MODIFY },
	{ 0x14, NOP, ZERO_PAGE_X, READ        },
	{ 0x15, ORA, ZERO_PAGE_X, READ        },
	{ 0x16, ASL, ZERO_PAGE_X, MODIFY      },
	{ 0x17, SLO, ZERO_PAGE_X, MODIFY      },
	{ 0x18, CLC, IMPLIED                  },
	{ 0x19, ORA, ABSOLUTE_Y, READ         },
	{ 0x1A, NOP, IMPLIED                  },
	{ 0x1B, SLO, ABSOLUTE_Y, MODIFY       },
	{ 0x1C, NOP, ABSOLUTE_X, READ         },
	{ 0x1D, ORA, ABSOLUTE_X, READ         },
	{ 0x1E, ASL, ABSOLUTE_X, MODIFY       },
	{ 0x1F, SLO, ABSOLUTE_X, MODIFY       },
	{ 0x20, JSR, SUBROUTINE_JUMP          },
	{ 0x21, AND, INDEXED_INDIRECT, READ   },
	{ 0x22, KIL, IMPLIED                  },
	{ 0x23, RLA, INDEXED_INDIRECT, MODIFY },
	{ 0x24, BIT, ZERO_PAGE, READ          },
	{ 0x25, AND, ZERO_PAGE, READ          },
	{ 0x26, ROL, ZERO_PAGE, MODIFY        },
	{ 0x27, RLA, ZERO_PAGE, MODIFY        },
	{ 0x28, PLP, STACK_PULL               },
	{ 0x29, AND, IMMEDIATE                },
	{ 0x2A, ROL, ACCUMULATOR              },
	{ 0x2B, ANC, IMMEDIATE                },
	{ 0x2C, BIT, ABSOLUTE, READ           },
	{ 0x2D, AND, ABSOLUTE, READ           },
	{ 0x2E, ROL, ABSOLUTE, MODIFY         },
	{ 0x2F, RLA, ABSOLUTE, MODIFY         },
	{ 0x30, BMI, BRANCH                   },
	{ 0x31, AND, INDIRECT_INDEXED, READ   },
	{ 0x32, KIL, IMPLIED                  },
	{ 0x33, RLA, INDIRECT_INDEXED, MODIFY },
	{ 0x34, NOP, ZERO_PAGE_X, READ        },
	{ 0x35, AND, ZERO_PAGE_X, READ        },
	{ 0x36, ROL, ZERO_PAGE_X, MODIFY      },
	{ 0x37, RLA, ZERO_PAGE_X, MODIFY      },
	{ 0x38, SEC, IMPLIED                  },
	{ 0x39, AND, ABSOLUTE_Y, READ         },
	{ 0x3A, NOP, IMPLIED                  },
	{ 0x3B, RLA, ABSOLUTE_Y, MODIFY       },
	{ 0x3C, NOP, ABSOLUTE_X, READ         },
	{ 0x3D, AND, ABSOLUTE_X, READ         },
	{ 0x3E, ROL, ABSOLUTE_X, MODIFY       },
	{ 0x3F, RLA, ABSOLUTE_X, MODIFY       },
	{ 0x40, RTI, INTERRUPT_RETURN         },
	{ 0x41, EOR, INDEXED_INDIRECT, READ   },
	{ 0x42, KIL, IMPLIED                  },
	{ 0x43, SRE, INDEXED_INDIRECT, MODIFY },
	{ 0x44, NOP, ZERO_PAGE, READ          },
	{ 0x45, EOR, ZERO_PAGE, READ          },
	{ 0x46, LSR, ZERO_PAGE, MODIFY        },
	{ 0x47, SRE, ZERO_PAGE, MODIFY        },
	{ 0x48, PHA, STACK_PUSH               },
	{ 0x49, EOR, IMMEDIATE                },
	{ 0x4A, LSR, ACCUMULATOR              },
	{ 0x4B, ALR, IMMEDIATE                },
	{ 0x4C, JMP, ABSOLUTE_JUMP            },
	{ 0x4D, EOR, ABSOLUTE, READ           },
	{ 0x4E, LSR, ABSOLUTE, MODIFY         },
	{ 0x4F, SRE, ABSOLUTE, MODIFY         },
	{ 0x50, BVC, BRANCH                   },
	{ 0x51, EOR, INDIRECT_INDEXED, READ   },
	{ 0x52, KIL, IMPLIED                  },
	{ 0x53, SRE, INDIRECT_INDEXED, MODIFY },
	{ 0x54, NOP, ZERO_PAGE_X, READ        },
	{ 0x55, EOR, ZERO_PAGE_X, READ        },
	{ 0x56, LSR, ZERO_PAGE_X, MODIFY      },
	{ 0x57, SRE, ZERO_PAGE_X, MODIFY      },
	{ 0x58, CLI, IMPLIED                  },
	{ 0x59, EOR, ABSOLUTE_Y, READ         },
	{ 0x5A, NOP, IMPLIED                  },
	{ 0x5B, SRE, ABSOLUTE_Y, MODIFY       },
	{ 0x5C, NOP, ABSOLUTE_X, READ         },
	{ 0x5D, EOR, ABSOLUTE_X, READ         },
	{ 0x5E, LSR, ABSOLUTE_X, MODIFY       },
	{ 0x5F, SRE, ABSOLUTE_X, MODIFY       },
	{ 0x60, RTS, SUBROUTINE_RETURN        },
	{ 0x61, ADC, INDEXED_INDIRECT, READ   },
	{ 0x62, KIL, IMPLIED                  },
	{ 0x63, RRA, INDEXED_INDIRECT, MODIFY },
	{ 0x64, NOP, ZERO_PAGE, READ          },
	{ 0x65, ADC, ZERO_PAGE, READ          },
	{ 0x66, ROR, ZERO_PAGE, MODIFY        },
	{ 0x67, RRA, ZERO_PAGE, MODIFY        },
	{ 0x68, PLA, STACK_PULL               },
	{ 0x69, ADC, IMMEDIATE                },
	{ 0x6A, ROR, ACCUMULATOR              },
	{ 0x6B, ARR, IMMEDIATE                },
	{ 0x6C, JMP, INDIRECT_JUMP            },
	{ 0x6D, ADC, ABSOLUTE, READ           },
	{ 0x6E, ROR, ABSOLUTE, MODIFY         },
	{ 0x6F, RRA, ABSOLUTE, MODIFY         },
	{ 0x70, BVS, BRANCH                   },
	{ 0x71, ADC, INDIRECT_INDEXED, READ   },
	{ 0x72, KIL, IMPLIED                  },
	{ 0x73, RRA, INDIRECT_INDEXED, MODIFY },
	{ 0x74, NOP, ZERO_PAGE_X, READ        },
	{ 0x75, ADC, ZERO_PAGE_X, READ        },
	{ 0x76, ROR, ZERO_PAGE_X, MODIFY      },
	{ 0x77, RRA, ZERO_PAGE_X, MODIFY      },
	{ 0x78, SEI, IMPLIED                  },
	{ 0x79, ADC, ABSOLUTE_Y, READ         },
	{ 0x7A, NOP, IMPLIED                  },
	{ 0x7B, RRA, ABSOLUTE_Y, MODIFY       },
	{ 0x7C, NOP, ABSOLUTE_X, READ         },
	{ 0x7D, ADC, ABSOLUTE_X, READ         },
	{ 0x7E, ROR, ABSOLUTE_X, MODIFY       },
	{ 0x7F, RRA, ABSOLUTE_X, MODIFY       },
	{ 0x80, NOP, IMMEDIATE                },
	{ 0x81, STA, INDEXED_INDIRECT, WRITE  },
	{ 0x82, NOP, IMMEDIATE                },
	{ 0x83, SAX, INDEXED_INDIRECT, WRITE  },
	{ 0x84, STY, ZERO_PAGE, WRITE         },
	{ 0x85, STA, ZERO_PAGE, WRITE         },
	{ 0x86, STX, ZERO_PAGE, WRITE         },
	{ 0x87, SAX, ZERO_PAGE, WRITE         },
	{ 0x88, DEY, IMPLIED                  },
	{ 0x89, NOP, IMMEDIATE                },
	{ 0x8A, TXA, IMPLIED                  },
	{ 0x8B, XAA, IMMEDIATE                },
	{ 0x8C, STY, ABSOLUTE, WRITE          },
	{ 0x8D, STA, ABSOLUTE, WRITE          },
	{ 0x8E, STX, ABSOLUTE, WRITE          },
	{ 0x8F, SAX, ABSOLUTE, WRITE          },
	{ 0x90, BCC, BRANCH                   },
	{ 0x91, STA, INDIRECT_INDEXED, WRITE  },
	{ 0x92, KIL, IMPLIED                  },
	{ 0x93, AHX, INDIRECT_INDEXED, READ   },
	{ 0x94, STY, ZERO_PAGE_X, WRITE       },
	{ 0x95, STA, ZERO_PAGE_X, WRITE       },
	{ 0x96, STX, ZERO_PAGE_Y, WRITE       },
	{ 0x97, SAX, ZERO_PAGE_Y, WRITE       },
	{ 0x98, TYA, IMPLIED                  },
	{ 0x99, STA, ABSOLUTE_Y, WRITE        },
	{ 0x9A, TXS, IMPLIED                  },
	{ 0x9B, TAS, ABSOLUTE_Y, READ         },
	{ 0x9C, SHY, ABSOLUTE_X, WRITE        },
	{ 0x9D, STA, ABSOLUTE_X, WRITE        },
	{ 0x9E, SHX, ABSOLUTE_Y, WRITE        },
	{ 0x9F, AHX, ABSOLUTE_Y, READ         },
	{ 0xA0, LDY, IMMEDIATE                },
	{ 0xA1, LDA, INDEXED_INDIRECT, READ   },
	{ 0xA2, LDX, IMMEDIATE                },
	{ 0xA3, LAX, INDEXED_INDIRECT, READ   },
	{ 0xA4, LDY, ZERO_PAGE, READ          },
	{ 0xA5, LDA, ZERO_PAGE, READ          },
	{ 0xA6, LDX, ZERO_PAGE, READ          },
	{ 0xA7, LAX, ZERO_PAGE, READ          },
	{ 0xA8, TAY, IMPLIED                  },
	{ 0xA9, LDA, IMMEDIATE                },
	{ 0xAA, TAX, IMPLIED                  },
	{ 0xAB, LAX, IMMEDIATE                },
	{ 0xAC, LDY, ABSOLUTE, READ           },
	{ 0xAD, LDA, ABSOLUTE, READ           },
	{ 0xAE, LDX, ABSOLUTE, READ           },
	{ 0xAF, LAX, ABSOLUTE, READ           },
	{ 0xB0, BCS, BRANCH                   },
	{ 0xB1, LDA, INDIRECT_INDEXED, READ   },
	{ 0xB2, KIL, IMPLIED                  },
	{ 0xB3, LAX, INDIRECT_INDEXED, READ   },
	{ 0xB4, LDY, ZERO_PAGE_X, READ        },
	{ 0xB5, LDA, ZERO_PAGE_X, READ        },
	{ 0xB6, LDX, ZERO_PAGE_Y, READ        },
	{ 0xB7, LAX, ZERO_PAGE_Y, READ        },
	{ 0xB8, CLV, IMPLIED                  },
	{ 0xB9, LDA, ABSOLUTE_Y, READ         },
	{ 0xBA, TSX, IMPLIED                  },
	{ 0xBB, LAS, ABSOLUTE_Y, READ         },
	{ 0xBC, LDY, ABSOLUTE_X, READ         },
	{ 0xBD, LDA, ABSOLUTE_X, READ         },
	{ 0xBE, LDX, ABSOLUTE_Y, READ         },
	{ 0xBF, LAX, ABSOLUTE_Y, READ         },
	{ 0xC0, CPY, IMMEDIATE                },
	{ 0xC1, CMP, INDEXED_INDIRECT, READ   },
	{ 0xC2, NOP, IMMEDIATE                },
	{ 0xC3, DCP, INDEXED_INDIRECT, MODIFY },
	{ 0xC4, CPY, ZERO_PAGE, READ          },
	{ 0xC5, CMP, ZERO_PAGE, READ          },
	{ 0xC6, DEC, ZERO_PAGE, MODIFY        },
	{ 0xC7, DCP, ZERO_PAGE, MODIFY        },
	{ 0xC8, INY, IMPLIED                  },
	{ 0xC9, CMP, IMMEDIATE                },
	{ 0xCA, DEX, IMPLIED                  },
	{ 0xCB, AXS, IMMEDIATE                },
	{ 0xCC, CPY, ABSOLUTE, READ           },
	{ 0xCD, CMP, ABSOLUTE, READ           },
	{ 0xCE, DEC, ABSOLUTE, MODIFY         },
	{ 0xCF, DCP, ABSOLUTE, MODIFY         },
	{ 0xD0, BNE, BRANCH                   },
	{ 0xD1, CMP, INDIRECT_INDEXED, READ   },
	{ 0xD2, KIL, IMPLIED                  },
	{ 0xD3, DCP, INDIRECT_INDEXED, MODIFY },
	{ 0xD4, NOP, ZERO_PAGE_X, READ        },
	{ 0xD5, CMP, ZERO_PAGE_X, READ        },
	{ 0xD6, DEC, ZERO_PAGE_X, MODIFY      },
	{ 0xD7, DCP, ZERO_PAGE_X, MODIFY      },
	{ 0xD8, CLD, IMPLIED                  },
	{ 0xD9, CMP, ABSOLUTE_Y, READ         },
	{ 0xDA, NOP, IMPLIED                  },
	{ 0xDB, DCP, ABSOLUTE_Y, MODIFY       },
	{ 0xDC, NOP, ABSOLUTE_X, READ         },
	{ 0xDD, CMP, ABSOLUTE_X, READ         },
	{ 0xDE, DEC, ABSOLUTE_X, MODIFY       },
	{ 0xDF, DCP, ABSOLUTE_X, MODIFY       },
	{ 0xE0, CPX, IMMEDIATE                },
	{ 0xE1, SBC, INDEXED_INDIRECT, READ   },
	{ 0xE2, NOP, IMMEDIATE                },
	{ 0xE3, ISC, INDEXED_INDIRECT, MODIFY },
	{ 0xE4, CPX, ZERO_PAGE, READ          },
	{ 0xE5, SBC, ZERO_PAGE, READ          },
	{ 0xE6, INC, ZERO_PAGE, MODIFY        },
	{ 0xE7, ISC, ZERO_PAGE, MODIFY        },
	{ 0xE8, INX, IMPLIED                  },
	{ 0xE9, SBC, IMMEDIATE                },
	{ 0xEA, NOP, IMPLIED                  },
	{ 0xEB, SBC, IMMEDIATE                },
	{ 0xEC, CPX, ABSOLUTE, READ           },
	{ 0xED, SBC, ABSOLUTE, READ           },
	{ 0xEE, INC, ABSOLUTE, MODIFY         },
	{ 0xEF, ISC, ABSOLUTE, MODIFY         },
	{ 0xF0, BEQ, BRANCH                   },
	{ 0xF1, SBC, INDIRECT_INDEXED, READ   },
	{ 0xF2, KIL, IMPLIED                  },
	{ 0xF3, ISC, INDIRECT_INDEXED, MODIFY },
	{ 0xF4, NOP, ZERO_PAGE_X, READ        },
	{ 0xF5, SBC, ZERO_PAGE_X, READ        },
	{ 0xF6, INC, ZERO_PAGE_X, MODIFY      },
	{ 0xF7, ISC, ZERO_PAGE_X, MODIFY      },
	{ 0xF8, SED, IMPLIED                  },
	{ 0xF9, SBC, ABSOLUTE_Y, READ         },
	{ 0xFA, NOP, IMPLIED                  },
	{ 0xFB, ISC, ABSOLUTE_Y, MODIFY       },
	{ 0xFC, NOP, ABSOLUTE_X, READ         },
	{ 0xFD, SBC, ABSOLUTE_X, READ         },
	{ 0xFE, INC, ABSOLUTE_X, MODIFY       },
	{ 0xFF, ISC, ABSOLUTE_X, MODIFY       },
};

static void Operate(machine* Machine, u8 Operation)
{
	cpu* CPU = &Machine->CPU;
	USING_CPU_REGISTERS

	// Operand register.
	u8& M  = CPU->Operand;
	// Temporary registers.
	u32 R;

	switch (Operation) {
		case ADC:
			R = A + M + CF;
			VF = ~(A ^ M) & (A ^ R) & 0x80;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = R > 0xFF;
			A = R;
			break;

		case ALR: // unofficial
			A &= M;
			CF = A & 0x01;
			A >>= 1;
			ZF = A == 0;
			NF = false;
			break;

		case ANC: // unofficial
			A &= M;
			ZF = A == 0;
			NF = A & 0x80;
			CF = NF;
			break;

		case AND:
			A &= M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case ARR: // unofficial
			A &= M;
			R = (A >> 1) | CF << 7;
			A = R;
			CF = A & 0x40;
			VF = ((A >> 1) ^ A) & 0x20;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case ASL:
			R = M << 1;
			M = R;
			ZF = M == 0;
			NF = M & 0x80;
			CF = R > 0xFF;
			break;

		case AXS: // unofficial
			R = (A & X) + (M ^ 0xFF) + 1;
			X = R;
			ZF = X == 0;
			NF = X & 0x80;
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

		case ISC: // unofficial
			M++;
			R = A + (M ^ 0xFF) + CF;
			VF = ~(A ^ (M ^ 0xFF)) & (A ^ R) & 0x80;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = R > 0xFF;
			A = R;
			break;

		case LAX: // unofficial
			A = M;
			X = A;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case LDA:
			A = M;
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

		case LSR:
			CF = M & 0x01;
			M >>= 1;
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
			R = (M << 1) | u8(CF);
			M = R;
			CF = R > 0xFF;
			A &= M;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case ROL:
			R = (M << 1) | u8(CF);
			M = R;
			CF = R > 0xFF;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case ROR:
			R = (M >> 1) | CF << 7;
			CF = M & 1;
			M = R;
			ZF = M == 0;
			NF = M & 0x80;
			break;

		case RRA: // unofficial
			R = (M >> 1) | CF << 7;
			CF = M & 1;
			M = R;
			R = A + M + CF;
			VF = ~(A ^ M) & (A ^ R) & 0x80;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = R > 0xFF;
			A = R;
			break;

		case SAX: // unofficial
			M = A & X;
			break;

		case SBC:
			R = A + (M ^ 0xFF) + CF;
			VF = ~(A ^ (M ^ 0xFF)) & (A ^ R) & 0x80;
			ZF = !(R & 0xFF);
			NF = R & 0x80;
			CF = R > 0xFF;
			A = R;
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

		case SLO: // unofficial
			R = M << 1;
			M = R;
			A |= M;
			ZF = A == 0;
			NF = A & 0x80;
			CF = R > 0xFF;
			break;

		case SRE: // unofficial
			CF = M & 0x01;
			M >>= 1;
			A ^= M;
			ZF = A == 0;
			NF = A & 0x80;
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

		case TSX:
			X = SP;
			ZF = X == 0;
			NF = X & 0x80;
			break;

		case TXA:
			A = X;
			ZF = A == 0;
			NF = A & 0x80;
			break;

		case TXS:
			SP = X;
			break;

		case TYA:
			A = Y;
			ZF = A == 0;
			NF = A & 0x80;
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

	char C1[16] = {0};
	char C2[16] = {0};

	switch (CPU->Instruction.InitialState & ~7) {
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
		case BRANCH:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s $%02X", OperationName, CPU->Address);
			break;
		case IMMEDIATE:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s #$%02X", OperationName, CPU->Immediate);
			break;
		case ZERO_PAGE:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s $%02X", OperationName, CPU->Immediate);
			break;
		case ZERO_PAGE_X:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s $%02X,X", OperationName, CPU->Immediate);
			break;
		case ZERO_PAGE_Y:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s $%02X,Y", OperationName, CPU->Immediate);
			break;
		case SUBROUTINE_JUMP:
		case ABSOLUTE_JUMP:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->Immediate & 0xFF, CPU->Immediate >> 8);
			sprintf(C2, "%s $%04X", OperationName, CPU->Immediate);
			break;
		case ABSOLUTE:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->Immediate & 0xFF, CPU->Immediate >> 8);
			sprintf(C2, "%s $%04X", OperationName, CPU->Immediate);
			break;
		case ABSOLUTE_X:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->Immediate & 0xFF, CPU->Immediate >> 8);
			sprintf(C2, "%s $%04X,X", OperationName, CPU->Immediate);
			break;
		case ABSOLUTE_Y:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->Immediate & 0xFF, CPU->Immediate >> 8);
			sprintf(C2, "%s $%04X,Y", OperationName, CPU->Immediate);
			break;
		case INDIRECT_JUMP:
			sprintf(C1, "%02X %02X %02X", Opcode, CPU->Immediate & 0xFF, CPU->Immediate >> 8);
			sprintf(C2, "%s ($%04X)", OperationName, CPU->Immediate);
			break;
		case INDEXED_INDIRECT:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s (%02X,X)", OperationName, CPU->Immediate);
			break;
		case INDIRECT_INDEXED:
			sprintf(C1, "%02X %02X", Opcode, CPU->Immediate);
			sprintf(C2, "%s (%02X),Y", OperationName, CPU->Immediate);
			break;
	}

	fprintf(F,
		"%04X  %-8s  %-30s A:%02X X:%02X Y:%02X SP:%02X\n",
		CPU->InstructionPC, C1, C2, CPU->A, CPU->X, CPU->Y, CPU->SP);

	Machine->TraceLine++;
}

#define STALL if (CPU->Stall > 0) { CPU->Stall--; break; }

static inline bool IsSamePage(u16 A, u16 B)
{
	return (A & 0xFF00) == (B & 0xFF00);
}

void StepCPU(machine* M)
{
	cpu* CPU = &M->CPU;
	USING_CPU_REGISTERS

	// Previous state of the interrupt flag.  On the real hardware, the CLI, SEI,
	// and PLP instructions change the interrupt flag after polling for interrupts.
	// In the current implementation, these instructions change the interrupt flag
	// before polling, but the polling then looks at the previous flag state.
	bool PreviousIF = IF;

	auto& Instruction   = CPU->Instruction;
	u8&   State         = CPU->State;
	u16&  InstructionPC = CPU->InstructionPC;
	u16&  Immediate     = CPU->Immediate;
	u16&  Indirect      = CPU->Indirect;
	u16&  Address       = CPU->Address;
	u8&   Operand       = CPU->Operand;

	switch (State) {
		// --- Reset ----------------------------------------------------------

		case RESET:
		case RESET +1:
			STALL;
			Read(M, PC);
			State++;
			break;
		case RESET +2:
		case RESET +3:
		case RESET +4:
			STALL;
			Read(M, 0x100 | SP--);
			State++;
			break;
		case RESET +5:
			STALL;
			PC = Read(M, 0xFFFC);
			BF = true;
			IF = true;
			State++;
			break;
		case RESET +6:
			STALL;
			PC |= Read(M, 0xFFFD) << 8;
			State = FETCH;
			break;

		// --- Fetch ----------------------------------------------------------

		case FETCH:
		case FETCH_NO_POLL:
			STALL;
			if (CPU->Interrupt) {
				Read(M, PC);
				// Start interrupt sequence for IRQ.
				State = INTERRUPT_JUMP;
			}
			else {
				InstructionPC = PC;
				Instruction = InstructionTable[Read(M, PC++)];
				State = Instruction.InitialState;
				//printf("I %s\n", OperationNameTable[Instruction.Operation]);
			}
			break;

		// --- Interrupt Jump -------------------------------------------------

		// Used for NMI, IRQ, and the BRK instruction.
		case INTERRUPT_JUMP:
			STALL;
			Read(M, PC);
			if (Instruction.Operation == BRK) PC++;
			State++;
			break;
		case INTERRUPT_JUMP +1:
			Write(M, 0x100 | SP--, PC >> 8);
			State++;
			break;
		case INTERRUPT_JUMP +2:
			Write(M, 0x100 | SP--, PC & 0xFF);
			State++;
			break;
		case INTERRUPT_JUMP +3:
			switch (CPU->Interrupt) {
				case NMI:
					Address = 0xFFFA;
					BF = false;
					Operate(M, PHP);
					BF = true;
					IF = true;
					break;
				case IRQ:
					Address = CPU->InternalNMI ? 0xFFFA : 0xFFFE;
					BF = false;
					Operate(M, PHP);
					IF = true;
					BF = true;
					break;
				case NO_INTERRUPT: // BRK
					Address = CPU->InternalNMI ? 0xFFFA : 0xFFFE;
					BF = true;
					Operate(M, PHP);
					IF = true;
					break;
			}
			Trace(M);
			Write(M, 0x100 | SP--, Operand);
			CPU->Interrupt = NO_INTERRUPT;
			CPU->InternalNMI = false;
			State++;
			break;
		case INTERRUPT_JUMP +4:
			STALL;
			PC = Read(M, Address);
			State++;
			break;
		case INTERRUPT_JUMP +5:
			STALL;
			PC |= Read(M, Address+1) << 8;
			// An interrupt sequence does not poll the NMI or IRQ detectors at the end.
			State = FETCH_NO_POLL;
			break;

		// --- Return from Interrupt ------------------------------------------

		case INTERRUPT_RETURN:
			STALL;
			Read(M, PC);
			State++;
			break;
		case INTERRUPT_RETURN +1:
			STALL;
			Read(M, 0x100 | SP++);
			State++;
			break;
		case INTERRUPT_RETURN +2:
			STALL;
			Operand = Read(M, 0x100 | SP++);
			Operate(M, PLP);
			State++;
			break;
		case INTERRUPT_RETURN +3:
			STALL;
			PC = Read(M, 0x100 | SP++);
			State++;
			break;
		case INTERRUPT_RETURN +4:
			STALL;
			PC |= Read(M, 0x100 | SP) << 8;
			Trace(M);
			State = FETCH;
			break;

		// --- Jump to Subroutine ---------------------------------------------

		case SUBROUTINE_JUMP:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case SUBROUTINE_JUMP +1:
			STALL;
			Read(M, 0x100 | SP);
			State++;
			break;
		case SUBROUTINE_JUMP +2:
			Write(M, 0x100 | SP--, PC >> 8);
			State++;
			break;
		case SUBROUTINE_JUMP +3:
			Write(M, 0x100 | SP--, PC & 0xFF);
			State++;
			break;
		case SUBROUTINE_JUMP +4:
			STALL;
			Immediate |= Read(M, PC) << 8;
			PC = Immediate;
			Trace(M);
			State = FETCH;
			break;

		// --- Return from Subroutine -----------------------------------------

		case SUBROUTINE_RETURN:
			STALL;
			Read(M, PC);
			State++;
			break;
		case SUBROUTINE_RETURN +1:
			STALL;
			Read(M, 0x100 | SP++);
			State++;
			break;
		case SUBROUTINE_RETURN +2:
			STALL;
			PC = Read(M, 0x100 | SP++);
			State++;
			break;
		case SUBROUTINE_RETURN +3:
			STALL;
			PC |= Read(M, 0x100 | SP) << 8;
			State++;
			break;
		case SUBROUTINE_RETURN +4:
			STALL;
			Read(M, PC++);
			Trace(M);
			State = FETCH;
			break;

		// --- Stack Push -----------------------------------------------------

		case STACK_PUSH:
			STALL;
			Read(M, PC);
			State++;
			break;
		case STACK_PUSH +1:
			Operate(M, Instruction.Operation);
			Write(M, 0x100 | SP--, Operand);
			Trace(M);
			State = FETCH;
			break;

		// --- Stack Pull -----------------------------------------------------

		case STACK_PULL:
			STALL;
			Read(M, PC);
			State++;
			break;
		case STACK_PULL +1:
			STALL;
			Read(M, 0x100 | SP++);
			State++;
			break;
		case STACK_PULL +2:
			STALL;
			Operand = Read(M, 0x100 | SP);
			Operate(M, Instruction.Operation);
			Trace(M);
			State = FETCH;
			break;

		// --- Implied --------------------------------------------------------

		case IMPLIED:
			STALL;
			Read(M, PC);
			Operate(M, Instruction.Operation);
			Trace(M);
			State = FETCH;
			break;

		// --- Accumulator ----------------------------------------------------

		case ACCUMULATOR:
			STALL;
			Read(M, PC);
			Operand = A;
			Operate(M, Instruction.Operation);
			Trace(M);
			A = Operand;
			State = FETCH;
			break;

		// --- Immediate ------------------------------------------------------

		case IMMEDIATE:
			STALL;
			Immediate = Read(M, PC++);
			Operand = Immediate & 0xFF;
			Operate(M, Instruction.Operation);
			Trace(M);
			State = FETCH;
			break;

		// --- Branch ---------------------------------------------------------

		case BRANCH:
			STALL;
			Immediate = Read(M, PC++);
			// Compute new program counter.
			Address = PC + Immediate;
			if (Immediate & 0x80) Address -= 0x100;
			State++;
			// If branch not taken, go fetch next instruction.
			switch (Instruction.Operation) {
				case BCC: if ( CF) State = FETCH; break;
				case BCS: if (!CF) State = FETCH; break;
				case BNE: if ( ZF) State = FETCH; break;
				case BEQ: if (!ZF) State = FETCH; break;
				case BPL: if ( NF) State = FETCH; break;
				case BMI: if (!NF) State = FETCH; break;
				case BVC: if ( VF) State = FETCH; break;
				case BVS: if (!VF) State = FETCH; break;
			}
			if (State == FETCH) Trace(M);
			break;
		case BRANCH +1:
			STALL;
			// Dummy read next opcode.
			Read(M, PC);
			State++;
			// Check for page-crossing branch.
			if (IsSamePage(Address, PC)) {
				// Branch target is on the same page, so we're done.
				PC = Address;
				Trace(M);
				State = FETCH_NO_POLL;
			}
			break;
		case BRANCH +2:
			STALL;
			// Dummy read opcode using old PCH.
			Read(M, (PC & 0xFF00) | (Address & 0x00FF));
			// Finally, we have the fixed PC.
			PC = Address;
			Trace(M);
			State = FETCH;
			break;

		// --- Absolute Jump --------------------------------------------------

		case ABSOLUTE_JUMP:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ABSOLUTE_JUMP +1:
			STALL;
			Immediate |= Read(M, PC) << 8;
			PC = Immediate;
			Trace(M);
			State = FETCH;
			break;

		// --- Indirect Jump --------------------------------------------------

		case INDIRECT_JUMP:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case INDIRECT_JUMP +1:
			STALL;
			Immediate |= Read(M, PC++) << 8;
			State++;
			break;
		case INDIRECT_JUMP +2:
			STALL;
			Address = Read(M, Immediate);
			State++;
			break;
		case INDIRECT_JUMP +3:
			STALL;
			Address |= Read(M, (Immediate & 0xFF00) | ((Immediate + 1) & 0x00FF)) << 8;
			PC = Address;
			Trace(M);
			State = FETCH;
			break;

		// --- Zero Page ------------------------------------------------------

		case ZERO_PAGE:
			STALL;
			Immediate = Read(M, PC++);
			Address = Immediate;
			State = Instruction.MemoryOperationState;
			break;

		// --- Zero Page Indexed X --------------------------------------------

		case ZERO_PAGE_X:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ZERO_PAGE_X +1:
			STALL;
			Read(M, Immediate);
			Address = (Immediate + X) & 0xFF;
			State = Instruction.MemoryOperationState;
			break;

		// --- Zero Page Indexed Y --------------------------------------------

		case ZERO_PAGE_Y:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ZERO_PAGE_Y +1:
			STALL;
			Read(M, Immediate);
			Address = (Immediate + Y) & 0xFF;
			State = Instruction.MemoryOperationState;
			break;

		// --- Absolute -------------------------------------------------------

		case ABSOLUTE:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ABSOLUTE +1:
			STALL;
			Immediate |= Read(M, PC++) << 8;
			Address = Immediate;
			State = Instruction.MemoryOperationState;
			break;

		// --- Absolute Indexed X ---------------------------------------------

		case ABSOLUTE_X:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ABSOLUTE_X +1:
			STALL;
			Immediate |= Read(M, PC++) << 8;
			Address = Immediate + X;
			State++;
			// If there is no page boundary crossing and we're reading, then we can
			// proceed with the read now.  Otherwise, there is a dummy read.
			if (IsSamePage(Immediate, Address) && Instruction.MemoryOperationState == READ)
				State = READ;
			break;
		case ABSOLUTE_X +2:
			STALL;
			Read(M, (Immediate & 0xFF00) | (Address & 0x00FF));
			State = Instruction.MemoryOperationState;
			break;

		// --- Absolute Indexed Y ---------------------------------------------

		case ABSOLUTE_Y:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case ABSOLUTE_Y +1:
			STALL;
			Immediate |= Read(M, PC++) << 8;
			Address = Immediate + Y;
			State++;
			// If there is no page boundary crossing and we're reading, then we can
			// proceed with the read now.  Otherwise, there is a dummy read.
			if (IsSamePage(Immediate, Address) && Instruction.MemoryOperationState == READ)
				State = READ;
			break;
		case ABSOLUTE_Y +2:
			STALL;
			Read(M, (Immediate & 0xFF00) | (Address & 0x00FF));
			State = Instruction.MemoryOperationState;
			break;

		// --- Indexed Indirect -----------------------------------------------

		case INDEXED_INDIRECT:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case INDEXED_INDIRECT +1:
			STALL;
			Read(M, Immediate);
			Indirect = (Immediate + X) & 0xFF;
			State++;
			break;
		case INDEXED_INDIRECT +2:
			STALL;
			Address = Read(M, Indirect);
			State++;
			break;
		case INDEXED_INDIRECT +3:
			STALL;
			Address |= Read(M, (Indirect+1) & 0xFF) << 8;
			State = Instruction.MemoryOperationState;
			break;

		// --- Indirect Indexed -----------------------------------------------

		case INDIRECT_INDEXED:
			STALL;
			Immediate = Read(M, PC++);
			State++;
			break;
		case INDIRECT_INDEXED +1:
			STALL;
			Indirect = Read(M, Immediate);
			State++;
			break;
		case INDIRECT_INDEXED +2:
			STALL;
			Indirect |= Read(M, (Immediate+1) & 0xFF) << 8;
			Address = Indirect + Y;
			State++;
			// If there is no page boundary crossing and we're reading, then we can
			// proceed with the read now.  Otherwise, there is a dummy read.
			if (IsSamePage(Indirect, Address) && Instruction.MemoryOperationState == READ)
				State = READ;
			break;
		case INDIRECT_INDEXED +3:
			STALL;
			Read(M, (Indirect & 0xFF00) | (Address & 0x00FF));
			State = Instruction.MemoryOperationState;
			break;

		// --- Read Operation -------------------------------------------------

		case READ:
			STALL;
			Operand = Read(M, Address);
			Operate(M, Instruction.Operation);
			Trace(M);
			State = FETCH;
			break;

		// --- Read-Modify-Write Operation ------------------------------------

		case MODIFY:
			STALL;
			Operand = Read(M, Address);
			State++;
			break;
		case MODIFY +1:
			Write(M, Address, Operand);
			Operate(M, Instruction.Operation);
			State++;
			break;
		case MODIFY +2:
			Write(M, Address, Operand);
			Trace(M);
			State = FETCH;
			break;

		// --- Write Operation ------------------------------------------------

		case WRITE:
			Operate(M, Instruction.Operation);
			Write(M, Address, Operand);
			Trace(M);
			State = FETCH;
			break;
	}

	// Poll for interrupts at the end of an instruction, or before branch cycles 0
	// (operand fetch), 1 (taken branch) and 2 (taken branch with page crossing).
	// The nesdev.org documentation states that interrupts are not polled before
	// BRANCH+1, but this seems to be necessary to pass the cpu_interrupts_v2 test
	// (and I haven't had any tests fail because of it).
	if (State == FETCH || State == BRANCH+0 || State == BRANCH+1 || State == BRANCH+2) {
		if (CPU->InternalNMI) {
			// Trigger NMI.
			CPU->Interrupt = NMI;
		}
		else if (CPU->InternalIRQ) {
			// CLI, SEI and PLP modify the interrupt flag after polling for interrupts.
			if (Instruction.Operation == CLI ||
			    Instruction.Operation == SEI ||
			    Instruction.Operation == PLP) {
				// Trigger IRQ, if interrupt flag was set.
				if (!PreviousIF) CPU->Interrupt = IRQ;
			}
			else {
				// Trigger IRQ, if interrupt flag is set.
				if (!IF) CPU->Interrupt = IRQ;
			}
		}
	}

	CPU->Cycle++;
}

void StepCPUPhase2(machine* Machine)
{
	cpu* CPU = &Machine->CPU;

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