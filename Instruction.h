#ifndef INSTRUCTION_H_INCLUDED
#define INSTRUCTION_H_INCLUDED

#include "Settings.h"

const int R = 0b001;
const int I = 0b010;
const int J = 0b100;

struct Rtype {
    unsigned int func   :6;
    unsigned int shamt  :5;
    unsigned int rd     :5;
    unsigned int rt     :5;
    unsigned int rs     :5;
    unsigned int op     :6;
};

struct Itype {
    unsigned int addr   :16;
    unsigned int rt     :5;
    unsigned int rs     :5;
    unsigned int op     :6;
};

struct Jump {
    unsigned int addr   :26;
    unsigned int op     :6;
};

union Type {
    Rtype R;
    Itype I;
    Jump  J;
};

struct Instruction {
    int type;
    Type inst;

    Instruction():type(UNKNOW) {}
};

#endif // INSTRUCTION_H_INCLUDED
