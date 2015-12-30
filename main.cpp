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

const string InputName  = "input//in2.txt";
const string OutputName = "result.txt";

map<string, int> Rcode, Icode;

int reg[REGNUM];
int mem[MEMNUM];
int exResult, memResult;
vector<Instruction> instMem;
queue<Instruction> instQueue;
vector<pair<string, int> > pipelineState;
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
int getImmediate(int, const string &);
int getOperationType(const string &);
int getOpcode(const string &);
int getFunctionCode(const string &);
void printInstructionName(const Instruction *);
void setInstruction(int, int, Instruction &, const string &);
void printInsturctions();
void instructionFormat(const Instruction &);

void loadInstructions();
void loadInstructions(int);
void pipeline();
bool stageIF();
bool stageID();
bool stageEX();
bool stageMEM();
bool stageWB();
bool trackBranch();
bool trackRegister(const int);
int executeRtype(const Instruction &);
int executeItype(const Instruction &);
void printStage(int);
void printStage(const string, int);
void printPipelined();

int main()
{
    Init();
    ReadFile();
    printInsturctions();
    pipeline();
    printPipelined();

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
    for(int i = 0; i < MEMNUM; i++)
        mem[i] = 1;

    BuildName();
    fetch = decode = execute = memory = writeback = NULL;
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
        if(in_str == "")
            continue;

        Log("Fetch instruction");
        bool isOp = false;
        Instruction inst;
        stringstream ss(in_str);

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
                setInstruction(i, times++, inst, code);
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

int getImmediate(int pc, const string &imm)
{
    // Return if the branch operation
    if(label[imm] > 0)
        return pc - ADD2TAR(ADDR, label[imm]);

    int sum = 0;
    for(size_t i = (imm[0] == '-' ? 1 : 0); i < imm.size(); i++) {
        sum *= 10;
        sum += imm[i] - '0';
    }
    sum *= (imm[0] == '-' ? -1 : 1);

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

string getInsturctionName(const Instruction *inst)
{
    if(inst->type == R)
        for(map<string, int>::iterator it = Rcode.begin();
            it != Rcode.end(); it++)
            if(it->second == inst->inst.R.func)
                return it->first;

    if(inst->type == I)
        for(map<string, int>::iterator it = Icode.begin();
            it != Icode.end(); it++)
            if(it->second == inst->inst.I.op)
                return it->first;
    return "";
}

void setInstruction(int pc, int times, Instruction &inst, const string &code)
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
            inst.inst.I.addr = getImmediate(pc, code);
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
    Log("Load instructions");
    for(size_t i = 0; i < instMem.size(); i++)
        instQueue.push(instMem[i]);
}

void loadInstructions(int count)
{
    // Count < 0, PC decrease
    if(count < 0) {
        int pc = instQueue.size() - count;
        // Clear instruction queue
        while(!instQueue.empty())
            instQueue.pop();
        // Insert instructions
        for(int i = instMem.size() - pc - 1; i < (int)instMem.size(); i++)
            instQueue.push(instMem[i]);
    }
    // Count > 0, PC increase
    if(count > 0)
        for(int i = 1; !instQueue.empty() && i < count; i++)
            instQueue.pop();

    // Refetch from instruction queue
    fetch = NULL;
}

void pipeline()
{
    bool s_if, s_id, s_ex, s_mem, s_wb;
    loadInstructions();
    if(!instQueue.empty())
        s_if = true;

    Log("Start execute");
    for(int cycle = 1; s_if || s_id || s_ex || s_mem || s_wb; cycle++) {
        //Log("Cycle");
        printStage(cycle);
        s_wb  = stageWB();
        s_mem = stageMEM();
        s_ex  = stageEX();
        s_id  = stageID();
        s_if  = stageIF();
    }
}

bool stageIF()
{
    // Fetch a instruction from the instruction queue if IF is ready
    if(!fetch) {
        // Return false when no any instruction in queue
        if(instQueue.empty())
            return false;

        // Fetch instruction
        fetch = &(instQueue.front());
        instQueue.pop();
    }

    if(fetch)
        // Print IF stage
        printStage(getInsturctionName(fetch), IF);

    return true;
}

bool stageID()
{
    // ID get IF
    if(!decode) {
        // Get IF
        // Track EX is branch or not
        if(fetch && !trackBranch()) {
            decode = fetch;
            fetch = NULL;
        }
    }

    if(decode) {
        // Print ID stage
        printStage(getInsturctionName(decode), ID);
        return true;
    }

    return false;
}

bool stageEX()
{
    bool isFetchable = true;

    // If no datapath
    if(!DATAPATH && decode) {
        if(decode->type == R)
            // Track rs, rt register
            if(trackRegister(decode->inst.R.rs)
               || trackRegister(decode->inst.R.rt))
                isFetchable = false;
        if(decode->type == I)
            // Track rs register
            if(trackRegister(decode->inst.I.rs)
               || trackRegister(decode->inst.I.rt))
                isFetchable = false;
    }

    // EX get ID
    if(!execute && isFetchable) {
        execute = decode;
        decode = NULL;
    }

    // Execute operation or calculate address
    if(execute) {
        // Print EX stage
        printStage(getInsturctionName(execute), EX);
        // Exectue instruction
        // Add, Sub
        if(execute->type == R)
            exResult = executeRtype(*execute);

        // lw, sw, beq
        if(execute->type == I)
            exResult = executeItype(*execute);

        // If instruction is branch, change PC and instruction queue
        if(execute->type == I
           && execute->inst.I.op == Icode["beq"])
            loadInstructions(exResult);

        return true;
    }
    return false;
}

bool stageMEM()
{
    // MEM get EX
    if(!memory) {
        memory = execute;
        execute = NULL;
    }

    // Access memory operand
    if(memory) {
        // Print MEM stage
        printStage(getInsturctionName(memory), MEM);
        // Access memory
        // lw, sw
        if(memory->type == I) {
            if(memory->inst.I.op == Icode["lw"])
                memResult = mem[exResult];
            // Store register to memory
            if(memory->inst.I.op == Icode["sw"])
                mem[exResult] = reg[memory->inst.I.rt];
        }
        if(memory->type == R)
            memResult = exResult;

        return true;
    }
    return false;
}

bool stageWB()
{
    writeback = NULL;
    // WB get MEM
    if(memory) {
        writeback = memory;
        memory = NULL;
    }

    // Write result back to register
    if(writeback) {
        // Print WB stage
        printStage(getInsturctionName(writeback), WB);
        // Write back
        // R, lw
        if(writeback->type == R)
            reg[writeback->inst.R.rd] = memResult;
        if(writeback->type == I
           && writeback->inst.I.op == Icode["lw"])
            reg[writeback->inst.I.rt] = memResult;
    }
    return false;
}

bool trackBranch()
{
    // If either ID or EX are branch instruction
    if(decode && decode->type == I
       && decode->inst.I.op == Icode["beq"])
        return true;
    if(execute && execute->type == I
       && execute->inst.I.op == Icode["beq"])
        return true;

    return false;
}

bool trackRegister(const int treg)
{
    // Track MEM destination register
    if(memory) {
        if(memory->type == R && memory->inst.R.rd == treg)
            return true;
        if(memory->type == I && memory->inst.I.rt == treg)
            return true;
    }
    // If not frowarding, track WB destination register
    if(!FORWARDING && writeback) {
        if(writeback->type == R && writeback->inst.R.rd == treg)
            return true;
        if(writeback->type == I && writeback->inst.I.rt == treg)
            return true;
    }
    return false;
}

int executeRtype(const Instruction &in)
{
    int a = reg[in.inst.R.rs], b = reg[in.inst.R.rt];
    int sht = in.inst.R.shamt;
    if(in.inst.R.func == Rcode["add"])
        return (a + b) << sht;
    if(in.inst.R.func == Rcode["sub"])
        return (a - b) << sht;
    return 0;
}

int executeItype(const Instruction &in)
{
    int rs = reg[in.inst.I.rs], rt = reg[in.inst.I.rt];
    int addr = in.inst.I.addr;
    if(in.inst.I.op == Icode["beq"]) {
        if(rs == rt)
            return addr;
        else
            return 0;
    }
    if(in.inst.I.op == Icode["lw"]
       || in.inst.I.op == Icode["sw"])
        return rs + addr;
    return 0;
}

void printStage(int cycle)
{
    pipelineState.push_back(make_pair("", cycle));
}

void printStage(const string name, int stage)
{
    pipelineState.push_back(make_pair(name, stage));
}

void printPipelined()
{
    for(size_t i = 0; i < pipelineState.size(); i++) {
        if(pipelineState[i].first != "") {
            cout << pipelineState[i].first << ":";
            if(pipelineState[i].second == IF)
                cout << "|I F|" << endl;
            if(pipelineState[i].second == ID)
                cout << "|I D|" << endl;
            if(pipelineState[i].second == EX)
                cout << "|E X|" << endl;
            if(pipelineState[i].second == MEM)
                cout << "|MEM|" << endl;
            if(pipelineState[i].second == WB)
                cout << "|W B|" << endl;
        }
        else
            cout << "Cycle " << pipelineState[i].second << endl;
    }

    for(int i = 0; i < 32; i++)
        cout << "$" << i << (i < 31 ? " " : "\n");
    for(int i = 0; i < 32; i++)
        cout << reg[i] << (i < 31 ? " " : "\n");

    for(int i = 0; i < 32; i++)
        cout << "W" << i << (i < 31 ? " " : "\n");
    for(int i = 0; i < 32; i++)
        cout << mem[i] << (i < 31 ? " " : "\n");
}
