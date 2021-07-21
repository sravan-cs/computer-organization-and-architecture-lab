#include <bits/stdc++.h>
using namespace std;

int inst_Cnt = 0,inst_arthCnt = 0,inst_logicCnt = 0,inst_dtCnt = 0,inst_ctCnt = 0,hltCnt = 0,dataStalls = 0,controlStalls=0;
int PC=0;
int IR;
int ALUoutput;
int LMD;											//Necessary Variables
int stalls=0;
int branch = 0;
int dataHz = 0;
vector<int> registers(16,0);
vector<int> valid(16,1);
vector<int> instructionArray(256,0);				//Necessary Vectors
vector<int> dataArray(256,0);
bool halt = false;
queue<int> IFs;
queue<vector<int>> IDs;
queue<vector<int>> EXs;
queue<vector<int>> memAcc;							//Necessary queues
queue<vector<int>> WBs;

void Instruction_Fetch(){							//Fetch instruction and push it to queue IF and update IR
	if(!halt){
		IR = 256*instructionArray[PC] + instructionArray[PC+1];
		IFs.push(IR);
		PC = PC + 2;
		if(IR >> 12 == 15)
        {
            halt = true;
        }
	}
}


void Instruction_Decode(){						//Decode
	if(!IFs.empty()){
		int inst = IFs.front();					//First split instruction into operands and opcode
		IFs.pop();
		vector<int> op(4);
		for(int i=3;i>=0;i--){
			op[i] = inst%16;
			inst = inst >> 4;
		}
		if(op[0]==15) halt = true;
        inst_Cnt++;
		IDs.push(op);
	}
	if(!IDs.empty())
    {
        auto tempOp = IDs.front();
        if(tempOp[0]<3 || tempOp[0]==4 || tempOp[0]==5 || tempOp[0]==7){	//If there are three operands
            if(valid[tempOp[3]] && valid[tempOp[2]]){
                valid[tempOp[1]] = 0;
                vector<int> temp = IDs.front();
                temp.push_back(0);
                EXs.push(temp);
                if(tempOp[0] < 3) inst_arthCnt++;
                else inst_logicCnt++;
                IDs.pop();
            }
            else{
                dataStalls++;
            	stalls++;
                dataHz = 1;
            }
        }
        else if(tempOp[0]==6 || tempOp[0]==8){								//If there are two operands
            if(valid[tempOp[2]]){
                valid[tempOp[1]] = 0;
                vector<int> temp = IDs.front();
                temp.push_back(0);
                EXs.push(temp);
                if(tempOp[0]==6) inst_logicCnt++;
                else inst_dtCnt++;
                IDs.pop();
            }
            else{
                dataStalls++;
            	stalls++;
                dataHz = 1;
            }
        }
        else if(tempOp[0]==3 || tempOp[0]==11){								//If there is one dependent register
            if(valid[tempOp[1]]){
                if(tempOp[0]==3) valid[tempOp[1]] = 0;
                vector<int> temp = IDs.front();
                if(tempOp[0]==11){
                    inst_ctCnt++;
                    controlStalls += 2;
                    stalls += 2;
                    branch = 2;
                    if(registers[tempOp[1]]==0){
                        temp.push_back(1);
                        while(!IFs.empty()) IFs.pop();
                        while(!IDs.empty()) IDs.pop();
                        halt = false;
                    }
                    else temp.push_back(0);
                }
                else
                {
                    inst_arthCnt++;
                    temp.push_back(0);
                }
                EXs.push(temp);
                if(!IDs.empty()) IDs.pop();
            }
            else{
                dataStalls++;
            	stalls++;
                dataHz = 1;
            }
        }
        else if(tempOp[0]==9){											//If it is store operation
            if(valid[tempOp[2]] && valid[tempOp[1]]){
                inst_dtCnt++;
                vector<int> temp = IDs.front();
                temp.push_back(0);
                EXs.push(temp);
                IDs.pop();
            }
            else{
                dataStalls++;
            	stalls++;
                dataHz = 1;
            }
        }
        else{	

            vector<int> temp = IDs.front();
            if(tempOp[0]==10){											//If it is jump operation
                inst_ctCnt++;
                controlStalls+=2;										//Always stall 2 cycles
                stalls += 2;
                branch = 2;
                temp.push_back(1);
                while(!IFs.empty()) IFs.pop();							//Flush(actually not necessary)
                while(!IDs.empty()) IDs.pop();
                halt = false;
            }
            else
            {
                hltCnt++;
                temp.push_back(0);
            }
            EXs.push(temp);
            if(!IDs.empty())IDs.pop();
        }
    }
}

void EX(){
    if(!EXs.empty()){
        vector<int> tempEX = EXs.front();
        EXs.pop();
        if(tempEX[0] == 0){
            ALUoutput = registers[tempEX[2]] + registers[tempEX[3]];	//Add
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 1){
            ALUoutput = registers[tempEX[2]] - registers[tempEX[3]];	//Sub
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 2){
            ALUoutput = registers[tempEX[2]] * registers[tempEX[3]];	//Mul
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 3){
            ALUoutput = registers[tempEX[1]] + 1;						//Inc
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 4){
            ALUoutput = registers[tempEX[2]] & registers[tempEX[3]];	//And
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 5){
            ALUoutput = registers[tempEX[2]] | registers[tempEX[3]]; 	//Or
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 6){
            ALUoutput = ~registers[tempEX[2]];							//Not
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 7){
            ALUoutput = registers[tempEX[2]] ^ registers[tempEX[3]];	//Xor
            vector<int> temp;
            temp.push_back(ALUoutput);
            temp.push_back(tempEX[1]);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 8 || tempEX[0]==9){
            int x = ((tempEX[3] & 1<<3) == 0 ? tempEX[3] : (tempEX[3]-16));
            ALUoutput = registers[tempEX[2]] + x;
            vector<int> temp;
            temp.push_back(ALUoutput);									//Load store effective address calculation
            temp.push_back(tempEX[1]);
            temp.push_back(tempEX[0]-8);
            memAcc.push(temp);
        }
        else if(tempEX[0] == 10 || tempEX[0]==11){
            vector<int> temp;
            if(tempEX[4]==1){
                int unsignInt;
                if(tempEX[0]==10)
                {
                    unsignInt = 16*tempEX[1] + tempEX[2];				//Jmp and branch, Target address calculation
                }
                else
                {
                    unsignInt = 16*tempEX[2] + tempEX[3];
                }
                int signInt = ((unsignInt & 1<<7) == 0 ? unsignInt : (unsignInt - 256));
                PC = PC + 2*signInt;
                memAcc.push(temp);
            }
            else memAcc.push(temp);
        }
        else if(tempEX[0] == 15){										//Halt,push dummy vector
            vector<int> temp;
            memAcc.push(temp);
        }
    }
}

void Memory_Access(){
	if(!memAcc.empty()){
		vector<int> mem = memAcc.front();
		memAcc.pop();
		vector<int> temp;
		if(mem.size()==3){												//If its a store  or register load operation(mem[2] == 1 is store otherwise loading in register)
			if(mem[2]==1){
				dataArray[mem[0]] = registers[mem[1]];
			}
			else{
				LMD = dataArray[mem[0]];
				temp.push_back(LMD);
				temp.push_back(mem[1]);
				temp.push_back(0);
			}
		}

		else if(mem.size()==2){											//Forwarding register register alu operations to write back							
			temp = mem;
		}
		WBs.push(temp);													//Forwarding dummy vector otherwise
	}
}

void Write_Back(){
	if(!WBs.empty()){
		vector<int> temp = WBs.front();	
		WBs.pop();

		if(temp.size()==3){												//If its a register loading operation,write LMD to appropriate registers
			registers[temp[1]] = temp[0];
			valid[temp[1]] = 1;
		}
		else if(temp.size()==2){										//Register - Register ALU operation
			registers[temp[1]] = temp[0];
			valid[temp[1]] = 1;
		}
	}
}

int main()
{
	string s;
	ifstream myFileD("DCache.txt");										//Read DCache into array
	for(int i=0;i<256;i++){
		getline(myFileD,s);
		int data = stoi(s,0,16);
		int signInt = ((data & 1<<7) == 0 ? data : (data - 256));
		dataArray[i] = data;
	}
	myFileD.close();

	ifstream myFileI("ICache.txt");										//Read ICache into array
	for(int i=0;i<256;i++){
		getline(myFileI,s);
		int inst = stoi(s,0,16);
		instructionArray[i] = inst;
	}
	myFileI.close();
    ifstream myFileR("RF.txt");											//Read registers into array
	for(int i=0;i<16;i++){
		getline(myFileR,s);
		int r = stoi(s,0,16);
		int signInt = ((r & 1<<7) == 0 ? r : (r - 256));
		registers[i] = signInt;
	}
	myFileR.close();
	int time = 0;
	branch = 0;
	while(1){															//loop,each iteration is one cycle
        Write_Back();
        Memory_Access();
        EX();
        Instruction_Decode();
        if(branch > 0)													//stalling because of control hazard
        {
            branch--;
            time++;
            continue;
        }
        if(dataHz > 0)													//stalling because of data hazard
        {
            dataHz--;
            time++;
            continue;
        }
        Instruction_Fetch();
		time++;
		if(halt && IFs.empty() && IDs.empty() && EXs.empty() && memAcc.empty() && WBs.empty()) break;
	}
	ofstream fileD;
	fileD.open("DCache0.txt");											//printing output
	for(int i = 0;i < 256;i++)
    {
        int x = dataArray[i];
        x = x & 255;
        char a,b;
        b = (x%16 < 10 ?('0'+x%16) : ('a' + (x%16)-10));
        x = x/16;
        a = (x%16 < 10 ?('0'+x%16) : ('a' + (x%16)-10));
        fileD << a << b << "\n";
    }
    fileD.close();
    ofstream fileRF;
    fileRF.open("RF0.txt");
    for(int i = 0;i < 16;i++)
    {
        int x = registers[i];
        x = x & 255;
        char a,b;
        b = (x%16 < 10 ?('0'+x%16) : ('a' + (x%16)-10));
        x = x/16;
        a = (x%16 < 10 ?('0'+x%16) : ('a' + (x%16)-10));
        fileRF << a << b << "\n";
    }
    fileRF.close();
    ofstream file;
    file.open("Output.txt");
    file << "Total number of instructions executed\t: "<< inst_Cnt << "\n";
    file << "Number of instructions in each class\n";
    file << "Arithmetic instructions\t\t\t: "<< inst_arthCnt << "\n";
    file << "Logical instructions\t\t\t: "<<inst_logicCnt << "\n";
    file << "Data instructions\t\t\t: "<<inst_dtCnt << "\n";
    file << "Control instructions\t\t\t: "<< inst_ctCnt << "\n";
    file << "Halt instructions\t\t\t: "<< hltCnt << "\n";
	file << "Cycles Per Instruction\t\t\t: "<< (double)time/inst_Cnt << "\n";
    file << "Total number of stalls\t\t\t: "<<stalls << "\n";
    file << "Data stalls (RAW)\t\t\t: "<< dataStalls << "\n";
    file << "Control stalls\t\t\t\t: " << controlStalls << "\n";
    file.close();
}
