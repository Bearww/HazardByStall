#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <string>
#include <cstring>

#define DATAPATH    0
#define FORWARDING  0

#define ADDR 0x80000

#define TAR2ADD(pc, i)   ((pc) + (i << 2))
#define ADD2TAR(pc, add) (((add) - (pc)) >> 2)

#define UNKNOW  0
#define FAIL    -1

#define IF  0b00001
#define ID  0b00010
#define EX  0b00100
#define MEM 0b01000
#define WB  0b10000

#define REGNUM 32
#define MEMNUM 32

using namespace std;

bool strcmp(const string &s1, const string &s2)
{
    return (strcmp(s1.c_str(), s2.c_str()) == 0);
}

#endif // SETTINGS_H_INCLUDED
