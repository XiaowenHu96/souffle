/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVMCode.cpp
 *
 * Implementation of LVMCode
 *
 ***********************************************************************/

#include "LVMCode.h"

namespace souffle {

void LVMCode::print() const {
    size_t ip = 0;
    const auto& code = this->getCode();
    size_t stratumLevel = 0;
    while (true) {
        switch (code[ip]) {
        case LVM_Number:
            printf("%ld\tLVM_Number\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        case LVM_ElementAccess:
            printf("%ld\tLVM_ElementAccess\t%d\t%d\n",
                   ip, code[ip+1], code[ip+2]);
            ip += 3;
            break;
        case LVM_AutoIncrement:
            printf("%ld\tLVM_AutoIncrement\t\n", ip);
            ip += 1;
            break;
        case LVM_OP_ORD:
            printf("%ld\tLVM_OP_PRD\t\n", ip);
            ip += 1;
            break;
        case LVM_OP_STRLEN: {
            printf("%ld\tLVM_OP_STRLEN\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_NEG: {
            printf("%ld\tLVM_OP_NEG\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_BNOT: {
            printf("%ld\tLVM_OP_BNOT\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_LNOT: {
            printf("%ld\tLVM_OP_LNOT\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_TONUMBER: {
            printf("%ld\tLVM_OP_TONUMBER\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_TOSTRING: {
            printf("%ld\tLVM_OP_TOSTRING\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_ADD: {
            printf("%ld\tLVM_OP_ADD\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_SUB: {
            printf("%ld\tLVM_OP_SUB\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_MUL: {
            printf("%ld\tLVM_OP_MUL\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_DIV: {
            printf("%ld\tLVM_OP_DIV\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_EXP: {
            printf("%ld\tLVM_OP_EXP\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_MOD: {
            printf("%ld\tLVM_OP_MOD\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_BAND: {
            printf("%ld\tLVM_OP_BAND\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_BOR: {
            printf("%ld\tLVM_OP_BOR\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_BXOR: {
            printf("%ld\tLVM_OP_BXOR\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_LAND: {
            printf("%ld\tLVM_OP_LAND\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_LOR: {
            printf("%ld\tLVM_OP_LOR\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_MAX: {
            printf("%ld\tLVM_OP_MAX\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_MIN: {
            printf("%ld\tLVM_OP_MIN\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_CAT: {
            printf("%ld\tLVM_OP_CAT\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_SUBSTR: {
            printf("%ld\tLVM_OP_SUBSTR\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_EQ: {
            printf("%ld\tLVM_OP_EQ\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_NE: {
            printf("%ld\tLVM_OP_NE\t\n", ip);
            ip += 1;
            break;

        }
        case LVM_OP_LT: {
            printf("%ld\tLVM_OP_LT\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_LE: {
            printf("%ld\tLVM_OP_LE\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_GT: {
            printf("%ld\tLVM_OP_GT\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_GE: {
            printf("%ld\tLVM_OP_GE\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_MATCH: {
            printf("%ld\tLVM_OP_MATCH\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_NOT_MATCH: {
            printf("%ld\tLVM_OP_NOT_MATCH\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_CONTAINS: {
            printf("%ld\tLVM_OP_CONTAINS\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_OP_NOT_CONTAINS: {
            printf("%ld\tLVM_OP_CONTAINS\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_UserDefinedOperator: { //TODO Later
            printf("%ld\tLVM_UserDefinedOperator\n", ip);
            printf("\t%s\t%s\t\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_PackRecord: {
            printf("%ld\tLVM_PackRecord\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        }
        case LVM_Argument: {
            printf("%ld\tLVM_Argument\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        }
        case LVM_Conjunction: {
            printf("%ld\tLVM_Conjunction\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_Negation: {
            printf("%ld\tLVM_Negation\t\n", ip);
            ip += 1;
            break;
        }
        case LVM_EmptinessCheck: {
            printf("%ld\tLVM_EmptinessCheck\t\n", ip);
            printf("\t%s\n",
                   symbolTable.resolve(code[ip+1]).c_str());
            ip += 2;
            break;
        }
        case LVM_ExistenceCheck: {
            printf("%ld\tLVM_ExistenceCheck\t\n", ip);
            printf("\t%s\t%s\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_ProvenanceExistenceCheck: { //TODO Later
            printf("%ld\tLVM_ProvenanceExitenceChekck\t\n", ip);
            printf("\t%s\t%s\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_Constraint: {
            ip += 1;
            break;
        }
        case LVM_Scan:
            printf("%ld\tLVM_Scan\t\n", ip);
            ip += 1;
            break;
        case LVM_IndexScan:
            printf("%ld\tLVM_IndexScan\t\n", ip);
            ip += 1;
            break;
        case LVM_Search: {
            printf("%ld\tLVM_Search\t\n", ip);
            ip += 3;
            break;
        }
        case LVM_UnpackRecord:
            printf("%ld\tLVM_UnpackRecord\t%d %d %d %d\t\n",
                   ip, code[ip+1], code[ip+2], code[ip+3], code[ip+4]);
            ip += 5;
            break;
        case LVM_Filter:
            printf("%ld\tLVM_Filter\t\n", ip);
            ip += 2;
            break;
        case LVM_Project:
            printf("%ld\tLVM_Project\t%d\t\n", ip, code[ip+1]);
            printf("\t%s\t\n",
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        case LVM_Return: {
            printf("%ld\tLVM_Return\t%d\t\n", ip, code[ip+1]);
            ip += 3;
            break;
        }
        case LVM_Sequence: {
            printf("%ld\tLVM_Sequence\n", ip);
            ip += 1;
            break;
        }
        case LVM_Parallel: {
            printf("%ld\tLVM_Parallel\t%d\t\n", ip, code[ip+1]);
            for (int i = 0; i < code[ip+1]; ++i) {
                printf("%d\t", code[ip+i]);
            }
            putchar('\n');
            ip += (1 + code[ip+1] + 1);
            break;
        }
        case LVM_Stop_Parallel: {
            printf("%ld\tLVM_Stop_Parallel\t%d\t\n", ip, code[ip+1]);
            ip += 2;
            break;
        }
        case LVM_Loop: {
            printf("%ld\tLVM_LOOP\n",ip);
            ip += 1;
            break;
        }
        case LVM_IncIterationNumber: {
            printf("%ld\tLVM_IncIterationNumber\n",ip);
            ip += 1;
            break;
        };
        case LVM_ResetIterationNumber: {
            printf("%ld\tLVM_ResetIterationNumber\n",ip);
            ip += 1;
            break;
        };
        case LVM_Exit: {
            printf("%ld\tLVM_Exit\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        }
        case LVM_LogTimer: {
            printf("%ld\tLVM_LogTimer\t\n", ip);
            ip += 5;
            break;
         }
         case LVM_StopLogTimer: {
            printf("%ld\tLVM_StopLogTimer\t\n", ip);
            ip += 2;
            break;                      
         }
         case LVM_DebugInfo: {
            printf("%ld\tLVM_DebugInfo\t\n", ip);
            ip += 2;
            break;
        }
        case LVM_Stratum:
            printf("%ld\tLVM_Stratum\t%ld\n", ip, stratumLevel++);
            ip += 1;
            break;
        case LVM_Create: {
            printf("%ld\tLVM_Create\t Name:%s Arity:%d Struct:%d\n", ip,
                   symbolTable.resolve(code[ip+1]).c_str(),
                   code[ip+2], code[ip+3]);
            for (int i = 0; i < code[ip+2]; ++i) {
                printf("\t%s", symbolTable.resolve(code[ip+4+i]).c_str());
            }
            putchar('\n');
            ip += 3 + code[ip+2] + 1;
            break;
        }
        case LVM_Clear: {
            printf("%ld\tLVM_Clear\t\n", ip);
            printf("\t%s\t\n",
                   symbolTable.resolve(code[ip+1]).c_str());
            ip += 2;
            break;
        }
        case LVM_Drop: {
            printf("%ld\tLVM_Drop\t\n", ip);
            printf("\t%s\t\n",
                   symbolTable.resolve(code[ip+1]).c_str());
            ip += 2;
            break;
        }
        case LVM_LogSize: {
            printf("%ld\tLVM_LogSize\t\n", ip);
            printf("\t%s\t\n",
                   symbolTable.resolve(code[ip+1]).c_str());
            ip += 3;
            break;
        }
        case LVM_Load: {
            printf("%ld\tLVM_Load\t\n", ip);
            printf("\t%s\tIOidx:%d\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   code[ip+2]);
            ip += 3;
            break;
        }
        case LVM_Store:
            printf("%ld\tLVM_Store\t\n", ip);
            printf("\t%s\tIOidx:%d\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   code[ip+2]);
            ip += 3;
            break;
        case LVM_Fact: {
            printf("%ld\tLVM_Fact\t\n", ip);
            printf("\t%s\t%d\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   code[ip+2]);
            ip += 3;
            break;
        }
        case LVM_Merge: {
            printf("%ld\tLVM_Merge\t\n", ip);
            printf("\t%s\t%s\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_Swap: {
            printf("%ld\tLVM_Swap\t\n", ip);
            printf("\t%s\t%s\n",
                   symbolTable.resolve(code[ip+1]).c_str(),
                   symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_Query:
            printf("%ld\tLVM_Query\t\n", ip);
            ip += 1;
            break;
        case LVM_Goto:
            printf("%ld\tLVM_GOTO\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        case LVM_Jmpnz:
            printf("%ld\tLVM_Jmpnz\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        case LVM_Jmpez:
            printf("%ld\tLVM_Jmpez\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        case LVM_Aggregate:
            printf("%ld\tLVM_Aggregate\t%d\n", ip, code[ip+1]);
            ip += 1;
            break;
        case LVM_Aggregate_COUNT: {
            printf("%ld\tLVM_Aggregate_COUNT\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        };
        case LVM_Aggregate_Return: {
            printf("%ld\tLVM_Aggregate_Return\t%d\n", ip, code[ip+1]);
            ip += 2;
            break;
        };
        case LVM_ITER_TypeIndexScan:
            printf("%ld\tLVM_ITER_TypeIndexScan\t%s\n", ip, symbolTable.resolve(code[ip+2]).c_str());
            ip += 4;
            break;
        case LVM_ITER_TypeScan: {
            printf("%ld\tLVM_ITER_TypeScan\t%s\n", ip, symbolTable.resolve(code[ip+2]).c_str());
            ip += 3;
            break;
        }
        case LVM_ITER_NotAtEnd:
            printf("%ld\tLVM_NotAtEnd\t%d\tType:%d\n", ip, code[ip+1], code[ip+2]);
            ip += 3;
            break;
        case LVM_ITER_Select:
            printf("%ld\tLVM_ITER_Select\t\n", ip);
            printf("\t%d\t%d\t%d\n",
                   code[ip+1],
                   code[ip+2],
                   code[ip+3]);
            ip += 4;
            break;
        case LVM_ITER_Inc:
            printf("%ld\tLVM_ITER_Inc\tIter:%d\tType:%d\n", ip, code[ip+1], code[ip+2]);
            ip += 3;
            break;
         case LVM_NOP:
            printf("%ld\tLVM_NOP\n", ip);
            ip += 1;
            break;
         case LVM_STOP:
            printf("%ld\tLVM_STOP\t\n", ip);
            return;
        default:
            printf("Unkown Operator id:%d\n", code[ip]);
            return;
        }
    }
}

}; // end of namespace souffle
