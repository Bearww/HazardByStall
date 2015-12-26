#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <string>
#include <cstring>
#include <vector>
#include <queue>
#include <map>

#include "Instruction.h"
#include "Settings.h"

using namespace std;

const string InputName  = "test.txt";
const string OutputName = "result.txt";

map<string, int> Rcode, Icode;

int reg[REGNUM];
int mem[MEMNUM];
vector<Instruction> instMem;
queue<Instruction> instQueue;
vector<vector<int> > istate;
map<string, int> label;
Instruction *fetch, *decode, *execute, *memory, *writeback;

// Record messages when executing
void Log(const string);

// Initialize memory and register
void Init();
// Build operation code
void BuildName();
// Read file from InputName
void ReadFile();

bool isLabel(const string &);
bool isOperation(const string &);
bool isRegister(const string &);
bool isAddress(const string &);
string filterComma(const string &);
string getLabelName(const string &);
int getRegister(const string &);
int getAddress(const string &);
int getImmediate(const string &);
int getOperationType(const string &);
int getOpcode(const string &);
int getFunctionCode(const string &);
void setInstruction(int, Instruction &, const string &);
void printInsturctions();
void instructionFormat(const Instruction &);

void loadInstructions();
void pipeline();
void stageIF();
void stageID();
void stageEX();
void stageMEM();
void stageWB();
bool trackBranch();
bool trackRegister(const int);

int main()
{
    Init();
    ReadFile();
    //printInsturctions();

    return 0;
}

void Log(const string msg)
{
    ofstream log("Log.txt", ios::app);
    if(!log)
        return;

    long pos = log.tellp();
    log.seekp(pos);

    time_t t;
    tm * timeinfo;

    time (&t);
    timeinfo = localtime (&t);
    // Format: 2015-11-23 22:45:18 msg
    log << timeinfo->tm_year + 1900 << "-" << timeinfo->tm_mon << "-"
        << timeinfo->tm_mday << " " << timeinfo->tm_hour
        << ":" << timeinfo->tm_min << ":" << timeinfo->tm_sec << " "
        << msg << endl;
    log.close();
}

void Init() {
    Log("Initialize memory and register.");
    for(int i = 1; i < REGNUM; i++)
        reg[i] = 1;
    for(int i = 1; i < MEMNUM; i++)
        mem[i] = 1;

    BuildName();
}

void BuildName() {
    Log("Build operation code.");
    string Rname[] = {"add", "sub"};
    int func[] = {0x20, 0x22};
    for(int i = 0; i < 2; i++)
        Rcode[Rname[i]] = func[i];

    string Iname[] = {"addi", "lw", "sw", "beq"};
    int opcode[] = {0x8, 0x23, 0x2B, 0x4};
    for(int i = 0; i < 4; i++)
        Icode[Iname[i]] = opcode[i];

    //Jstr[0x2] = "j";
    //Jstr[0x3] = "jal";
}

void ReadFile()
{
    Log("Try to open the input file.");
    ifstream fin(InputName.c_str());

    if(!fin) {
        Log("Open file error.");
        return;
    }

    string in_str, code;
    for(int i = 0, times = 0; getline(fin, in_str); i++, times = 0) {
        Log("Fetch instruction");
        bool isOp = false;
        Instruction inst;
        stringstream ss(in_str);cout << in_str << endl;
        while(ss >> code) {
            // Filter the code begin or end with comma
            code = filterComma(code);
            // If code is a label, record address of label
            if(isLabel(code))
                label[getLabelName(code)] = TAR2ADD(ADDR, i);
            // If code is a operation, record type and function/opcode
            else if(isOperation(code)) {
                isOp = true;
                inst.type = getOperationType(code);
                if(inst.type == R)
                    inst.inst.R.func = getFunctionCode(code);
                if(inst.type == I)
                    inst.inst.I.op = getOpcode(code);
            }
            // If code is register, address or immediate value
            else
                setInstruction(times++, inst, code);
        }
        // If instruction is not label
        instMem.push_back(inst);
    }
    Log("Fetch instructions is done.");
    fin.close();
}

bool isLabel(const string &code)
{
    // Check code last character is ':'
    if(code.size() < 1) return false;
    return (code[code.size() - 1] == ':');
}

bool isOperation(const string &code)
{
    map<string, int>::iterator it;
    // Check code is R type
    for(it = Rcode.begin(); it != Rcode.end(); it++)
        if(strcmp(it->first, code))
            return true;

    // Check code is I type
    for(it = Icode.begin(); it != Icode.end(); it++)
        if(strcmp(it->first, code))
            return true;

    // Code is neither R type nor I type
    return false;
}

bool isRegister(const string &code)
{
    // Check code has '$'
    return (code.find("$") != string::npos);
}

bool isAddress(const string &code)
{
    // Check code has '('
    return (code.find("(") != string::npos);
}

string filterComma(const string &code)
{
    int pos = 0, sz = code.size();
    // Filter the code begin with comma
    if(code[0] == ',')
        pos++, sz--;
    // Filter the code end with comma
    if(code.size() && code[code.size() - 1] == ',')
        sz--;
    return code.substr(pos, sz);
}

string getLabelName(const string &label)
{
    if(isLabel(label))
        return label.substr(0, label.size() - 1);
    return "";
}

int getRegister(const string &reg)
{
    if(isRegister(reg)) {
        if(reg.find("$zero") != string::npos)
            return 0;
        int sum = 0, pos = reg.size(), sz = reg.size();
        if(reg[sz - 1] == ')')
            sz--;
        for(; pos > 0; pos--)
            if(reg[pos] == '$')
               break;

        for(int i = pos + 1; i < sz; i++) {
            sum *= 10;
            sum += reg[i] - '0';
        }
        return sum;
    }
    return UNKNOW;
}

int getAddress(const string &addr)
{
    int tar = 0;
    for(size_t i = 0; i < addr.size() && addr[i] != '('; i++) {
        tar *= 10;
        tar += addr[i] - '0';
    }
    return ADD2TAR(0, tar);
}

int getImmediate(const string &imm)
{
    // Return if the branch operation
    if(label[imm] > 0)
        return label[imm];

    int base = 10, sum = 0;
    // Immediate value is 2-base or 16-base
    if(imm.size() > 2 && imm[0] == '0') {
        if(imm[1] == 'x' || imm[1] == 'X')
            base = 16;
        if(imm[1] == 'b' || imm[1] == 'B')
            base = 2;
    }

    for(size_t i =(base == 10 ? 0 : 2); i < imm.size(); i++) {
        sum *= base;
        if(imm[i] >= 'A' && imm[i] <= 'F')
            sum += imm[i] - 'A' + 10;
        else if(imm[i] >= 'a' && imm[i] <= 'f')
            sum += imm[i] - 'a' + 10;
        else
            sum += imm[i] - '0';
    }
    return sum;
}

int getOperationType(const string &code)
{
    map<string, int>::iterator it;
    // Return if code is R type
    for(it = Rcode.begin(); it != Rcode.end(); it++)
        if(strcmp(it->first, code))
            return R;

    // Return if code is I type
    for(it = Icode.begin(); it != Icode.end(); it++)
        if(strcmp(it->first, code))
            return I;

    // Code is neither R type nor I type
    return UNKNOW;
}

int getOpcode(const string &code)
{
    return (Icode[code] ? Icode[code] : UNKNOW);
}

int getFunctionCode(const string &code)
{
    return (Rcode[code] ? Rcode[code] : UNKNOW);
}

void setInstruction(int times, Instruction &inst, const string &code)
{
    // Destination register
    if(times == 0) {
        if(inst.type == R)
            inst.inst.R.rd = getRegister(code);
        if(inst.type == I)
            inst.inst.I.rt = getRegister(code);
    }
    // R type: rs, I type:rs/addr(rs)
    else if(times == 1) {
        if(inst.type == R)
            inst.inst.R.rs = getRegister(code);
        if(inst.type == I) {
            inst.inst.I.rs = getRegister(code);
            if(isAddress(code))
                inst.inst.I.addr = getAddress(code);
        }
    }
    // R type: rt, I type:immediate/address
    else {
        if(inst.type == R)
            inst.inst.R.rt = getRegister(code);
        if(inst.type == I)
            inst.inst.I.addr = getImmediate(code);
    }
}

void printInsturctions()
{
    for(size_t i = 0; i < instMem.size(); i++)
        instructionFormat(instMem[i]);
}

void instructionFormat(const Instruction &inst)
{
    if(inst.type == R)
        cout << inst.inst.R.op << " "
            << inst.inst.R.rs << " "
            << inst.inst.R.rt << " "
            << inst.inst.R.rd << " "
            << inst.inst.R.shamt << " "
            << inst.inst.R.func << " "
            << endl;
    if(inst.type == I)
        cout << inst.inst.I.op << " "
            << inst.inst.I.rs << " "
            << inst.inst.I.rt << " "
            << inst.inst.I.addr << " "
            << endl;
}

void loadInstructions()
{
    for(size_t i = 0; i < instMem.size(); i++)
        instQueue.push(instMem[i]);
}

void pipeline()
{
    for(int cycle = 1; !instQueue.empty(); cycle++) {
        stageWB();
        stageMEM();
        stageEX();
        stageID();
        stageIF();
    }
}

void stageIF()
{
    // Fetch a instruction from the instruction queue if IF is ready
    if(!fetch) {
        // Return when no any instruction in queue
        if(instQueue.empty())
            return;
        fetch = &(instQueue.front());
    }
}

void stageID()
{
    // ID get IF
    if(!decode) {
        // Get IF
        // Track EX is branch or not
        if(!trackBranch()) {
            decode = fetch;
            fetch = NULL;
            instQueue.pop();
        }
    }
}

void stageEX()
{
    // Execute operation or calculate address
    bool isFetchable = false;
    // EX get ID
    // If forwarding and EX is done

    // If datapath

    // If no datapath and WB is done
    if(!DATAPATH)
        if(decode->type == R)
            // Track rs, rt
            if(!trackRegister(decode->inst.R.rs)
               && !trackRegister(decode->inst.R.rt))
                isFetchable = true;
        if(decode->type == I)
            // Track rs
            if(!trackRegister(decode->inst.I.rs))
                isFetchable = true;

    // If instruction is branch, change PC and instruction queue
    if(isFetchable) {
    }
}

void stageMEM()
{
    // Access memory operand

    // MEM get EX
    if(!execute) {
        memory = execute;
        execute = NULL;
    }

    // Store register to memory
}

void stageWB()
{
    // Write result back to register

    // WB get MEM
    if(!memory) {
        writeback = memory;
        memory = NULL;
    }
}

bool trackBranch()
{
    // If either ID or EX are branch instruction
    if(!decode && decode->type == I
       && decode->inst.I.op == Icode["beq"])
        return true;
    if(!execute && execute->type == I
       && execute->inst.I.op == Icode["beq"])
        return true;

    return false;
}

bool trackRegister(const int treg)
{
    // Track EX destination register
    if(!execute) {
        if(execute->type == R && execute->inst.R.rd == treg)
            return true;
        if(execute->type == I && execute->inst.I.rt == treg)
            return true;
    }
    // Track MEM destination register
    if(!memory) {
        if(memory->type == R && memory->inst.R.rd == treg)
            return true;
        if(memory->type == I && memory->inst.I.rt == treg)
            return true;
    }
    return false;
}
