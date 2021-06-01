/*
 * David Bardwell
 * Computer Architecture Project 3 - Pipeline simulator
 * 
 * for Computer Organization and Design
 * Tyngsboro Campus - Wednesday evening 6-9pm class
 *
 * Due: December 11, 2008
 * 
 */
/* Project comments:
 *   Simulate the MIPS 5-stage pipeline
 *
 *   Liberal use of global variables to keep the logic a little simpler than
 *   passing a lot of context around. 
 */

/* includes */
#include <stdio.h>
#include <string.h>
#include <malloc.h>

/* MIPS instruction masks to determine components of instructions */
#define MASK_OPCODE     0xfc000000
#define MASK_REG1       0x03e00000
#define MASK_REG2       0x001f0000
#define MASK_REG3       0x0000f800
#define MASK_FUNC       0x0000003f
#define MASK_PC_OFFSET  0x0000ffff

/* Right bitwise shift positions to get value of constituent components
   of the instruction */
#define OPCODE_SHIFT  26
#define REG1_SHIFT    21
#define REG2_SHIFT    16
#define REG3_SHIFT    11

/* Function codes for R-TYPE instructions */
#define ADD_FUNCT  0x20
#define SUB_FUNCT  0x22
#define NOP_FUNCT  0x00  /* NOP will look like an R-TYPE instruction, i.e. opcode is 0 */

/* Opcodes for I-TYPE instructions */
#define LB_OPCODE   0x20
#define SB_OPCODE   0x28

/* Data Structures for the Control and 4 pipeline registers */
typedef struct PIPELINE_CONTROL {
    int RegDst;
    int ALUOpt1;
    int ALUOpt0;
    int ALUSrc;
    int MemRead;
    int MemWrite;
    int RegWrite;
    int MemToReg;
} PIPELINE_CONTROL;


typedef struct IF_ID_PIPELINE_REG {
    unsigned int Fetched_Instruction;

} IF_ID_PIPELINE_REG;


typedef struct ID_EX_PIPELINE_REG {
    PIPELINE_CONTROL control;
    unsigned int Fetched_Instruction;
    char InstructionName[32];
    int  Opcode;
    int  FuncCode;
    int  SrcReg1;       /* Needed for data hazard detection */
    int  SrcReg2;       /* Needed for data hazard detection */
    int  ReadReg1Value;
    int  ReadReg2Value;
    int  WriteRegNum1;
    int  WriteRegNum2;
    int  SEOffset;      /* At this point SEOffset is a 32-bit extended value */
} ID_EX_PIPELINE_REG;


typedef struct EX_MEM_PIPELINE_REG {
    PIPELINE_CONTROL control;
    char InstructionName[32]; /* for clarity in output */
    int  ALUResult;
    int  SWValue;
    int  WriteRegNum;
} EX_MEM_PIPELINE_REG;


typedef struct MEM_WB_PIPELINE_REG {
    PIPELINE_CONTROL control;
    char InstructionName[32]; /* for clarity in output */
    int  LWDataValue;
    int  ALUResult;
    int  WriteRegNum;
} MEM_WB_PIPELINE_REG;


typedef struct INSTRUCTION_DECODE {
   int  Opcode;
   int  FuncCode;
   char InstructionName[32];
   int  SrcReg1;
   int  SrcReg2;
   int  CandidateWriteReg1;
   int  CandidateWriteReg2;
   short SEOffset;
} INSTRUCTION_DECODE;


#define TOTAL_MEMORY_SIZE 0x400  /* Exactly 1K */

/* Global Variables for pipeline registers, system registers and main memory */
static unsigned char Main_Memory[TOTAL_MEMORY_SIZE];

static int Sys_Registers[32];

IF_ID_PIPELINE_REG Write_IF_ID_Reg;
IF_ID_PIPELINE_REG Read_IF_ID_Reg;

ID_EX_PIPELINE_REG Write_ID_EX_Reg;
ID_EX_PIPELINE_REG Read_ID_EX_Reg;

EX_MEM_PIPELINE_REG Write_EX_MEM_Reg;
EX_MEM_PIPELINE_REG Read_EX_MEM_Reg;

MEM_WB_PIPELINE_REG Write_MEM_WB_Reg;
MEM_WB_PIPELINE_REG Read_MEM_WB_Reg;

INSTRUCTION_DECODE Instruction_Decode;

/* pointer that holds MIPS instructions to disassemble */
static unsigned int *mips_instructions;

/* Forward declarations including the 5 pipeline stage functions */
void Initialize();
void IF_stage(int instruction_num);
void ID_stage();
void EX_stage();
void MEM_stage();
void WB_stage();
void print_out_everything(FILE *fp_output, int ClockCycle);
void Copy_write_to_read();
int  LoadMIPSInstructions();
void DecodeInstruction(unsigned int mips_instruction);

/*
 * main
 *   Loop over instructions and simulate the 5-stage pipeline
 *
 * returns - 0 on normal exit
 * 
 */
int main(int argc, char *argv[]) {

    int i, j, instruction_count;
    FILE *fp_output;
   
    fp_output = fopen("output3.txt", "w");   

    /* Initialize everything */
    Initialize();

    /* Get the instructions to simulate the pipeline */
    instruction_count = LoadMIPSInstructions();
   
    /* Main Loop over instructions and simulate the pipeline */
    for (i = 0; i < instruction_count; i++) {

        /* Call the pipeline stage functions to simulate the pipeline */
        /* Start with the IF_Stage and work thru until finish the WB_Stage */
        IF_stage(i);

        ID_stage();

        EX_stage(); /* Where we handle data hazards, at start of EX_Stage */

        MEM_stage();

        WB_stage();

        print_out_everything(fp_output, i + 1);

        Copy_write_to_read();
    }

    /* Dump Memory to verify as well */
    fprintf(fp_output, "Verify main memory also\n");
    j = 0;
    for (i = 0; i < 0x400 ; i++) {
        if (Main_Memory[i] % 256 != j) {
            fprintf(fp_output, "Memory addess %x has modified value %x.\n",
                    i, Main_Memory[i]);
        }
        j++;
        if (j == 256)
            j = 0;
    }
   
    fclose(fp_output);
    free(mips_instructions);
    return 0;
}


/* Initialize system registers, main memory and pipeline registers */
void Initialize() {

    int mem_address, i, j;
    
    /* Initialize System Registers */
    Sys_Registers[0] = 0x0;
    for (i = 1; i < 32; i++) {
        Sys_Registers[i] = 0x100 + i;
    }

    /* Initialize Main Memory */
    for (j = 0; j < 0x4 ; j++) {
        for (i = 0; i < 0x100; i++) {
            mem_address = j * 0x100 + i;
            Main_Memory[mem_address] = i;
        }
    }

    /* Initialize the write and read pipeline registers */
    /* IF_ID_PIPELINE */
    Write_IF_ID_Reg.Fetched_Instruction = 0x0;
    Read_IF_ID_Reg.Fetched_Instruction = 0x0;
    
    /* ID_EX_PIPELINE */
    Write_ID_EX_Reg.control.RegDst = 0;
    Write_ID_EX_Reg.control.ALUOpt1 = 0;
    Write_ID_EX_Reg.control.ALUOpt0 = 0;
    Write_ID_EX_Reg.control.ALUSrc = 0;
    Write_ID_EX_Reg.control.MemRead = 0;
    Write_ID_EX_Reg.control.MemWrite = 0;
    Write_ID_EX_Reg.control.RegWrite = 0;
    Write_ID_EX_Reg.control.MemToReg = 0;
    strcpy(Write_ID_EX_Reg.InstructionName, "NOP");
    Write_ID_EX_Reg.Opcode = 0;
    Write_ID_EX_Reg.FuncCode = 0;
    Write_ID_EX_Reg.SrcReg1 = 0;
    Write_ID_EX_Reg.SrcReg2 = 0;
    Write_ID_EX_Reg.ReadReg1Value = 0x0;
    Write_ID_EX_Reg.ReadReg2Value = 0x0;
    Write_ID_EX_Reg.WriteRegNum1 = 0;
    Write_ID_EX_Reg.WriteRegNum2 = 0;
    Write_ID_EX_Reg.SEOffset = 0;

    Read_ID_EX_Reg.control.RegDst = 0;
    Read_ID_EX_Reg.control.ALUOpt1 = 0;
    Read_ID_EX_Reg.control.ALUOpt0 = 0;
    Read_ID_EX_Reg.control.ALUSrc = 0;
    Read_ID_EX_Reg.control.MemRead = 0;
    Read_ID_EX_Reg.control.MemWrite = 0;
    Read_ID_EX_Reg.control.RegWrite = 0;
    Read_ID_EX_Reg.control.MemToReg = 0;
    strcpy(Read_ID_EX_Reg.InstructionName, "NOP");
    Read_ID_EX_Reg.Opcode = 0;
    Read_ID_EX_Reg.FuncCode = 0;
    Read_ID_EX_Reg.SrcReg1 = 0;
    Read_ID_EX_Reg.SrcReg2 = 0;
    Read_ID_EX_Reg.ReadReg1Value = 0x0;
    Read_ID_EX_Reg.ReadReg2Value = 0x0;
    Read_ID_EX_Reg.WriteRegNum1 = 0;
    Read_ID_EX_Reg.WriteRegNum2 = 0;
    Read_ID_EX_Reg.SEOffset = 0;

    /* EX_MEM_PIPELINE */
    Write_EX_MEM_Reg.control.MemRead = 0;
    Write_EX_MEM_Reg.control.MemWrite = 0;
    Write_EX_MEM_Reg.control.RegWrite = 0;
    Write_EX_MEM_Reg.control.MemToReg = 0;
    strcpy(Write_EX_MEM_Reg.InstructionName, "NOP");
    Write_EX_MEM_Reg.ALUResult = 0;
    Write_EX_MEM_Reg.SWValue = 0;
    Write_EX_MEM_Reg.WriteRegNum = 0;

    Read_EX_MEM_Reg.control.MemRead = 0;
    Read_EX_MEM_Reg.control.MemWrite = 0;
    Read_EX_MEM_Reg.control.RegWrite = 0;
    Read_EX_MEM_Reg.control.MemToReg = 0;
    strcpy(Read_EX_MEM_Reg.InstructionName, "NOP");
    Read_EX_MEM_Reg.ALUResult = 0;
    Read_EX_MEM_Reg.SWValue = 0;
    Read_EX_MEM_Reg.WriteRegNum = 0;

    /* MEM_WB_PIPELINE */
    Write_MEM_WB_Reg.control.MemRead = 0;
    Write_MEM_WB_Reg.control.MemWrite = 0;    
    Write_MEM_WB_Reg.control.RegWrite = 0;
    Write_MEM_WB_Reg.control.MemToReg = 0;
    strcpy(Write_MEM_WB_Reg.InstructionName, "NOP");
    Write_MEM_WB_Reg.LWDataValue = 0;
    Write_MEM_WB_Reg.ALUResult = 0;
    Write_MEM_WB_Reg.WriteRegNum = 0;

    Read_MEM_WB_Reg.control.MemRead = 0;
    Read_MEM_WB_Reg.control.MemWrite = 0;
    Read_MEM_WB_Reg.control.RegWrite = 0;
    Read_MEM_WB_Reg.control.MemToReg = 0;
    strcpy(Read_MEM_WB_Reg.InstructionName, "NOP");
    Read_MEM_WB_Reg.LWDataValue = 0;
    Read_MEM_WB_Reg.ALUResult = 0;
    Read_MEM_WB_Reg.WriteRegNum = 0;
}

/*
 * IF_stage()
 * Instruction fetch stage
 */
void IF_stage(int instruction_num) {
    /* Fetch next MIPS instruction and keep it in the Write_IF_ID_Reg */
    Write_IF_ID_Reg.Fetched_Instruction = mips_instructions[instruction_num];
}

/*
 * ID_stage()
 * Instruction decode stage
 *  (1) Decode the instruction
 *  (2) Determine the full control for the rest of the processing of the instruction
 *      thru the remainder of the pipeline execution.
 *  (3) Write to the Write_ID_EX_Reg pipeline register all the details from the 
 *      instruction decoding.
 */
void ID_stage() {

    /* Read the instruction from the Read_IF_ID_Reg */
    Write_ID_EX_Reg.Fetched_Instruction = Read_IF_ID_Reg.Fetched_Instruction;

    /* Decode the instruction */
    DecodeInstruction(Read_IF_ID_Reg.Fetched_Instruction);

    /* Populate the pipeline control structure now that we have decoded the instruction 
       Keep in mind that at this point we do not know for sure what our write register is 
       going to be.
     */
    /* R-Type but not NOP */
    if ( (Instruction_Decode.Opcode == 0) &&
         (Instruction_Decode.FuncCode != 0) )
    {
        Write_ID_EX_Reg.control.RegDst =   1;
        Write_ID_EX_Reg.control.ALUOpt1 =  1;
        Write_ID_EX_Reg.control.ALUOpt0 =  0;
        Write_ID_EX_Reg.control.ALUSrc =   0;
        Write_ID_EX_Reg.control.MemRead =  0;
        Write_ID_EX_Reg.control.MemWrite = 0;
        Write_ID_EX_Reg.control.RegWrite = 1;
        Write_ID_EX_Reg.control.MemToReg = 0;

    }
    else if ((Instruction_Decode.Opcode   == 0) &&
             (Instruction_Decode.FuncCode == 0) )
    {
        /* NOP */
        Write_ID_EX_Reg.control.RegDst =   0;
        Write_ID_EX_Reg.control.ALUOpt1 =  0;
        Write_ID_EX_Reg.control.ALUOpt0 =  0;
        Write_ID_EX_Reg.control.ALUSrc =   0;
        Write_ID_EX_Reg.control.MemRead =  0;
        Write_ID_EX_Reg.control.MemWrite = 0;
        Write_ID_EX_Reg.control.RegWrite = 0;
        Write_ID_EX_Reg.control.MemToReg = 0;
    }
    else {
        /* I-Type instructions: LB and SB */
        if (Instruction_Decode.Opcode == LB_OPCODE) {
            Write_ID_EX_Reg.control.RegDst =   0;
            Write_ID_EX_Reg.control.ALUOpt1 =  0;
            Write_ID_EX_Reg.control.ALUOpt0 =  0;
            Write_ID_EX_Reg.control.ALUSrc =   1;
            Write_ID_EX_Reg.control.MemRead =  1;
            Write_ID_EX_Reg.control.MemWrite = 0;
            Write_ID_EX_Reg.control.RegWrite = 1;
            Write_ID_EX_Reg.control.MemToReg = 1;
        }
        else if (Instruction_Decode.Opcode == SB_OPCODE) {
            Write_ID_EX_Reg.control.RegDst =   0;
            Write_ID_EX_Reg.control.ALUOpt1 =  0;
            Write_ID_EX_Reg.control.ALUOpt0 =  0;
            Write_ID_EX_Reg.control.ALUSrc =   1;
            Write_ID_EX_Reg.control.MemRead =  0;
            Write_ID_EX_Reg.control.MemWrite = 1;
            Write_ID_EX_Reg.control.RegWrite = 0;
            Write_ID_EX_Reg.control.MemToReg = 0;
        }
    }

    /* Write the decoded instruction information into the pipeline register */
    strcpy(Write_ID_EX_Reg.InstructionName, Instruction_Decode.InstructionName);
    Write_ID_EX_Reg.Opcode = Instruction_Decode.Opcode;
    Write_ID_EX_Reg.FuncCode = Instruction_Decode.FuncCode;
    Write_ID_EX_Reg.SrcReg1 = Instruction_Decode.SrcReg1;  /* data hazard support */
    Write_ID_EX_Reg.SrcReg2 = Instruction_Decode.SrcReg2;  /* data hazard support */
    Write_ID_EX_Reg.ReadReg1Value = Sys_Registers[Instruction_Decode.SrcReg1];
    Write_ID_EX_Reg.ReadReg2Value = Sys_Registers[Instruction_Decode.SrcReg2];
    Write_ID_EX_Reg.WriteRegNum1 = Instruction_Decode.CandidateWriteReg1;
    Write_ID_EX_Reg.WriteRegNum2 = Instruction_Decode.CandidateWriteReg2;
    Write_ID_EX_Reg.SEOffset = Instruction_Decode.SEOffset;
}

/*
 * EX_stage()
 *  ALU Execute stage in the pipeline
 *  
 *  Data hazards are handled by checking the next two stages of the
 *  Pipeline to see if either source register in this stage are written
 *  to in the next two stages of the pipeline. If so, we grab the ALUResult
 *  that will eventually go into the referenced source register.
 *
 */
void EX_stage() {
    int SrcRegValue1;
    int SrcRegValue2;
    int ALUInput1;
    int ALUInput2;
    int ALUResult;
    int SWValue;
    
    /* Read from the Read_ID_EX_Reg */
    SrcRegValue1 = Read_ID_EX_Reg.ReadReg1Value; /* May override in the data hazard case */ 
    SrcRegValue2 = Read_ID_EX_Reg.ReadReg2Value; /* May override in the data hazard case */
    
    /* Forwarding processing for possible data hazards */
    /* Update the SrcRegValue1 and/or SrcRegValue2 if data hazards detected */
    /*         --- Data Hazard Detection Logic ---                          */
    /* Check pipeline least recent to most recent for correct ALUResult value. */
    /* See if the current source registers are referenced in the next two stages of */
    /* the pipeline as write registers and that the RegWrite control is 1. */
    
    /* Check first source register */
    if ((Read_ID_EX_Reg.SrcReg1 == Read_MEM_WB_Reg.WriteRegNum) &&
        (Read_MEM_WB_Reg.control.RegWrite == 1)) {
        SrcRegValue1 = Read_MEM_WB_Reg.ALUResult;
    }
    if ((Read_ID_EX_Reg.SrcReg1 == Read_EX_MEM_Reg.WriteRegNum) &&
        (Read_EX_MEM_Reg.control.RegWrite == 1)) {
        SrcRegValue1 = Read_EX_MEM_Reg.ALUResult;
    }
    
    /* Check second source register */
    if ((Read_ID_EX_Reg.SrcReg2 == Read_MEM_WB_Reg.WriteRegNum) &&
        (Read_MEM_WB_Reg.control.RegWrite == 1)) {
        SrcRegValue2 = Read_MEM_WB_Reg.ALUResult;
    }
    if ((Read_ID_EX_Reg.SrcReg2 == Read_EX_MEM_Reg.WriteRegNum) &&
        (Read_EX_MEM_Reg.control.RegWrite == 1)) {
        SrcRegValue2 = Read_EX_MEM_Reg.ALUResult;
    }
    
    /* Important: Determine the correct ALU Inputs based on ALUSrc */
    ALUInput1 = SrcRegValue1;
    if (Read_ID_EX_Reg.control.ALUSrc == 0) {
        /* R-Type */
        ALUInput2 = SrcRegValue2;
    }
    else {
        /* I-Type */
        ALUInput2 = Read_ID_EX_Reg.SEOffset;
    }
    
    /* Perform the ALU Operation based on ALU Control */
    if (Read_ID_EX_Reg.Opcode == 0) {
        /* R-Type Add */
        if (Read_ID_EX_Reg.FuncCode == ADD_FUNCT) {
            ALUResult = ALUInput1 + ALUInput2;
        }
        /* R-Type Subtract */
        else if (Read_ID_EX_Reg.FuncCode == SUB_FUNCT) {
            ALUResult = ALUInput1 - ALUInput2;
        }
        else if (Read_ID_EX_Reg.FuncCode == NOP_FUNCT) {
        /* NOP */
        ALUResult = 0;
        }
    }
    else {
        /* I-Type LB or SB */
        ALUResult = ALUInput1 + ALUInput2;
    }
    
    /* The store word value is forwarded in the pipeline for MEM_stage() */
    SWValue = Sys_Registers[Read_ID_EX_Reg.SrcReg2];

    /* Write to the EX_MEM Pipeline register */
    strcpy(Write_EX_MEM_Reg.InstructionName, Read_ID_EX_Reg.InstructionName);
    Write_EX_MEM_Reg.control.MemRead =  Read_ID_EX_Reg.control.MemRead;
    Write_EX_MEM_Reg.control.MemWrite = Read_ID_EX_Reg.control.MemWrite;
    Write_EX_MEM_Reg.control.RegWrite = Read_ID_EX_Reg.control.RegWrite;
    Write_EX_MEM_Reg.control.MemToReg = Read_ID_EX_Reg.control.MemToReg;
    Write_EX_MEM_Reg.ALUResult = ALUResult;
    Write_EX_MEM_Reg.SWValue = SWValue;
    
    /* The Register destination for the write register is determined
     * by the RegDst control and is written to the EX/MEM write register at
     * this point in the pipeline.
     */
    if (Read_ID_EX_Reg.control.RegDst == 0) {
        Write_EX_MEM_Reg.WriteRegNum = Read_ID_EX_Reg.WriteRegNum1; /* rt */
    }
    else {
        Write_EX_MEM_Reg.WriteRegNum = Read_ID_EX_Reg.WriteRegNum2; /* rd */
    }
}

/*
 * MEM_stage()
 *  Load Byte and Store Byte will have work in this stage
 *  All R-Type instructions do nothing in this stage
 *
 */
void MEM_stage() {
    /* Perform the appropriate memory operation based on memory read/write controls */
    if (Read_EX_MEM_Reg.control.MemRead == 1) {
        Write_MEM_WB_Reg.LWDataValue = Main_Memory[Read_EX_MEM_Reg.ALUResult];
    }
    else if (Read_EX_MEM_Reg.control.MemWrite == 1) {
        Main_Memory[Read_EX_MEM_Reg.ALUResult] = Read_EX_MEM_Reg.SWValue & 0x000000FF;
    }

    /* write to the MEM_WB Pipeline register */
    strcpy(Write_MEM_WB_Reg.InstructionName, Read_EX_MEM_Reg.InstructionName);
    Write_MEM_WB_Reg.control.MemRead =  Read_EX_MEM_Reg.control.MemRead;
    Write_MEM_WB_Reg.control.MemWrite = Read_EX_MEM_Reg.control.MemWrite;    
    Write_MEM_WB_Reg.control.RegWrite = Read_EX_MEM_Reg.control.RegWrite;
    Write_MEM_WB_Reg.control.MemToReg = Read_EX_MEM_Reg.control.MemToReg;    
    Write_MEM_WB_Reg.ALUResult = Read_EX_MEM_Reg.ALUResult;
    Write_MEM_WB_Reg.WriteRegNum = Read_EX_MEM_Reg.WriteRegNum;
}

/**
 *  WB_Stage()
 *  Write back to write register either the results of an ALU
 *  operation for an R-Type instruction or the value loaded from
 *  memory for the lb instruction.
 *
 */
void WB_stage() {
    /* Only have work to do if RegWrite is 1, if so, determine which
     * value to write to the register using the MemToReg control
     */
    if (Read_MEM_WB_Reg.control.RegWrite == 1) {
        if (Read_MEM_WB_Reg.control.MemToReg == 0) {
            Sys_Registers[Read_MEM_WB_Reg.WriteRegNum] = Read_MEM_WB_Reg.ALUResult;
        }
        else {
            Sys_Registers[Read_MEM_WB_Reg.WriteRegNum] = Read_MEM_WB_Reg.LWDataValue;
        }
    }
}

/**
 * print_out_everything()
 *
 * For each clock cycle, print out the contents of the 32 system registers
 * and the full details of what is currently in the write and read
 * pipeline registers.
 *
 */
void print_out_everything(FILE *fp_output, int ClockCycle) {
    int i, j, k;
    
    /* print out 32 system registers */
    fprintf(fp_output, "--------------------------------------------------------------------\n");
    fprintf(fp_output, "  ------------------------ Clock Cycle %d ------------------------  \n", 
            ClockCycle);
    fprintf(fp_output, "--------------------------------------------------------------------\n");

    fprintf(fp_output, "System Registers 0 to 31\n");
    for (i = 0 ; i < 8; i++) {
        for (j = 0; j < 4; j++) {
            k = i*4 + j;
            fprintf(fp_output, "Register $%2d = %3x   ", k, Sys_Registers[k]);
        }
        fprintf(fp_output, "\n");
    }
    fprintf(fp_output, "\n");

    /* IF/ID Write Pipeline */
    fprintf(fp_output, "IF/ID Write (written to by the IF stage)\n");
    fprintf(fp_output, "Instruction = %8.8x\n\n", Write_IF_ID_Reg.Fetched_Instruction);

    /* IF/ID  Read Pipeline */
    fprintf(fp_output, "IF/ID Read (read by the ID stage)\n");
    fprintf(fp_output, "Instruction = %8.8x\n", Read_IF_ID_Reg.Fetched_Instruction);
    fprintf(fp_output, "--------------------------------------------------\n\n");

    
    /* ID/EX Write Pipeline */
    fprintf(fp_output, "ID/EX Write (written to by the ID stage)\n");
    fprintf(fp_output, "Instruction = %8.8x, Instruction Name [%s]\n", 
            Write_ID_EX_Reg.Fetched_Instruction, Write_ID_EX_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Execution/address calculation stage\n");
    
    if (Write_ID_EX_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegDst = %d   ALUOpt1 = %d   ALUOpt0 = %d   ALUSrc = %d\n\n",
                 Write_ID_EX_Reg.control.RegDst,
                 Write_ID_EX_Reg.control.ALUOpt1,
                 Write_ID_EX_Reg.control.ALUOpt0,
                 Write_ID_EX_Reg.control.ALUSrc);
    }
    else {
        fprintf(fp_output, "  RegDst = X   ALUOpt1 = %d   ALUOpt0 = %d   ALUSrc = %d\n\n",
                 Write_ID_EX_Reg.control.ALUOpt1,
                 Write_ID_EX_Reg.control.ALUOpt0,
                 Write_ID_EX_Reg.control.ALUSrc);
    }
    fprintf(fp_output, " Memory Access Stage\n");
    fprintf(fp_output, "  MemRead = %d   MemWrite = %d\n\n",
               Write_ID_EX_Reg.control.MemRead,
               Write_ID_EX_Reg.control.MemWrite);
    fprintf(fp_output, " Write-back Stage\n");
    
    if (Write_ID_EX_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Write_ID_EX_Reg.control.RegWrite,
                 Write_ID_EX_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Write_ID_EX_Reg.control.RegWrite);
    }

    if ((Write_ID_EX_Reg.control.RegDst == 0) &&
        (Write_ID_EX_Reg.control.ALUSrc == 1)) {
        fprintf(fp_output, "ReadReg1Value = %x, ReadReg2Value = %x, SEOffset = %x\n", 
                Write_ID_EX_Reg.ReadReg1Value,
                Write_ID_EX_Reg.ReadReg2Value,
                Write_ID_EX_Reg.SEOffset);
    }
    else {
        fprintf(fp_output, "ReadReg1Value = %x, ReadReg2Value = %x, SEOffset = X\n", 
                Write_ID_EX_Reg.ReadReg1Value,
                Write_ID_EX_Reg.ReadReg2Value);
    }
    
    /* Do not know for sure our Write Register at this point */
    if (Write_ID_EX_Reg.control.RegWrite == 1) {
        fprintf(fp_output, "WriteRegNum = %d(rt), %d(rd)\n\n",
            Write_ID_EX_Reg.WriteRegNum1, Write_ID_EX_Reg.WriteRegNum2);
    }
    else {
        fprintf(fp_output, "WriteRegNum = X, X\n\n");
    }

    /* ID/EX Read Pipeline */    
    fprintf(fp_output, "ID/EX Read (read by the EX stage)\n");
    fprintf(fp_output, "Instruction = %8.8x, Instruction Name [%s]\n",
             Read_ID_EX_Reg.Fetched_Instruction, Read_ID_EX_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Execution/address calculation stage\n");
    
    
    if (Read_ID_EX_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegDst = %d   ALUOpt1 = %d   ALUOpt0 = %d   ALUSrc = %d\n\n",
                 Read_ID_EX_Reg.control.RegDst,
                 Read_ID_EX_Reg.control.ALUOpt1,
                 Read_ID_EX_Reg.control.ALUOpt0,
                 Read_ID_EX_Reg.control.ALUSrc);
    }
    else {
        fprintf(fp_output, "  RegDst = X   ALUOpt1 = %d   ALUOpt0 = %d   ALUSrc = %d\n\n",
                 Read_ID_EX_Reg.control.ALUOpt1,
                 Read_ID_EX_Reg.control.ALUOpt0,
                 Read_ID_EX_Reg.control.ALUSrc);
    }
    fprintf(fp_output, " Memory Access Stage\n");
    fprintf(fp_output, "  MemRead = %d   MemWrite = %d\n\n",
               Read_ID_EX_Reg.control.MemRead,
               Read_ID_EX_Reg.control.MemWrite);
    fprintf(fp_output, " Write-back Stage\n");
    
    if (Read_ID_EX_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Read_ID_EX_Reg.control.RegWrite,
                 Read_ID_EX_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Read_ID_EX_Reg.control.RegWrite);
    }
    
    if ((Read_ID_EX_Reg.control.RegDst == 0) &&
        (Read_ID_EX_Reg.control.ALUSrc == 1)) {
        fprintf(fp_output, "ReadReg1Value = %x, ReadReg2Value = %x, SEOffset = %x\n", 
                Read_ID_EX_Reg.ReadReg1Value,
                Read_ID_EX_Reg.ReadReg2Value,
                Read_ID_EX_Reg.SEOffset);
    }
    else {
        fprintf(fp_output, "ReadReg1Value = %x, ReadReg2Value = %x, SEOffset = X\n", 
                Read_ID_EX_Reg.ReadReg1Value,
                Read_ID_EX_Reg.ReadReg2Value);
    }

    if (Read_ID_EX_Reg.control.RegWrite == 1) {
        fprintf(fp_output, "WriteRegNum = %d(rt), %d(rd)\n",
            Read_ID_EX_Reg.WriteRegNum1, Read_ID_EX_Reg.WriteRegNum2);
    }
    else {
        fprintf(fp_output, "WriteRegNum = X, X\n");
    }
    fprintf(fp_output, "--------------------------------------------------\n\n");

    
    /* EX/MEM Write Pipeline */
    fprintf(fp_output, "EX/MEM Write (written to by the EX stage)\n");
    fprintf(fp_output, "Instruction Name [%s]\n", Write_EX_MEM_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Memory Access Stage\n");
    fprintf(fp_output, "  MemRead = %d   MemWrite = %d\n\n",
               Write_EX_MEM_Reg.control.MemRead,
               Write_EX_MEM_Reg.control.MemWrite);
    fprintf(fp_output, " Write-back Stage\n");

    if (Write_EX_MEM_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Write_EX_MEM_Reg.control.RegWrite,
                 Write_EX_MEM_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Write_EX_MEM_Reg.control.RegWrite);
    }
    
    if (Write_EX_MEM_Reg.control.MemWrite == 1) {
        fprintf(fp_output, "ALUResult = %x   SWValue = %x   WriteRegNum = X\n\n", 
            Write_EX_MEM_Reg.ALUResult, Write_EX_MEM_Reg.SWValue);
    }
    else if (Write_EX_MEM_Reg.control.RegWrite == 1) {
        fprintf(fp_output, "ALUResult = %x   SWValue = X   WriteRegNum = %d\n\n", 
            Write_EX_MEM_Reg.ALUResult, Write_EX_MEM_Reg.WriteRegNum);    
    }
    else {
        fprintf(fp_output, "ALUResult = %x   SWValue = X   WriteRegNum = X\n\n", 
            Write_EX_MEM_Reg.ALUResult);
    }
    
    /* EX/MEM Read Pipeline */
    fprintf(fp_output, "EX/MEM Read (Read by the MEM stage)\n");
    fprintf(fp_output, "Instruction Name [%s]\n", Read_EX_MEM_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Memory Access Stage\n");
    fprintf(fp_output, "  MemRead = %d   MemWrite = %d\n\n",
               Read_EX_MEM_Reg.control.MemRead,
               Read_EX_MEM_Reg.control.MemWrite);
    fprintf(fp_output, " Write-back Stage\n");
    
    if (Read_EX_MEM_Reg.control.MemWrite == 0) {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Read_EX_MEM_Reg.control.RegWrite,
                 Read_EX_MEM_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Read_EX_MEM_Reg.control.RegWrite);
    }

    if (Read_EX_MEM_Reg.control.MemWrite == 1) {
        fprintf(fp_output, "ALUResult = %x   SWValue = %x   WriteRegNum = X\n\n", 
            Read_EX_MEM_Reg.ALUResult, Read_EX_MEM_Reg.SWValue);
    }
    else if (Read_EX_MEM_Reg.control.RegWrite == 1) {
        fprintf(fp_output, "ALUResult = %x   SWValue = X   WriteRegNum = %d\n\n", 
            Read_EX_MEM_Reg.ALUResult, Read_EX_MEM_Reg.WriteRegNum);    
    }
    else {
        fprintf(fp_output, "ALUResult = %x   SWValue = X   WriteRegNum = X\n\n", 
            Read_EX_MEM_Reg.ALUResult);
    }
    fprintf(fp_output, "--------------------------------------------------\n\n");

    
    /* MEM/WB Write Pipeline */
    fprintf(fp_output, "MEM/WB Write (written to by the MEM stage)\n");
    fprintf(fp_output, "Instruction Name [%s]\n", Write_MEM_WB_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Write-back Stage\n");
    
    if (Write_MEM_WB_Reg.control.MemWrite == 0) {    
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Write_MEM_WB_Reg.control.RegWrite,
                 Write_MEM_WB_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Write_MEM_WB_Reg.control.RegWrite);
    }

    if (Write_MEM_WB_Reg.control.MemRead == 1) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = %x   WriteRegNum = %d\n\n", 
            Write_MEM_WB_Reg.ALUResult, Write_MEM_WB_Reg.LWDataValue, Write_MEM_WB_Reg.WriteRegNum);
    }
    else if ((Write_MEM_WB_Reg.control.MemRead == 0) &&
             (Write_MEM_WB_Reg.control.RegWrite == 1)) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = X   WriteRegNum = %d\n\n", 
            Write_MEM_WB_Reg.ALUResult, Write_MEM_WB_Reg.WriteRegNum);
    }
    else if ((Write_MEM_WB_Reg.control.MemRead == 0) &&
             (Write_MEM_WB_Reg.control.RegWrite == 0)) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = X   WriteRegNum = X\n\n", 
            Write_MEM_WB_Reg.ALUResult);
    }    

    /* MEM/WB Read Pipeline */
    fprintf(fp_output, "MEM/WB Read (Read by the WB stage)\n");
    fprintf(fp_output, "Instruction Name [%s]\n", Read_MEM_WB_Reg.InstructionName);
    fprintf(fp_output, "Pipeline Controls:\n");
    fprintf(fp_output, " Write-back Stage\n");
    
    if (Read_MEM_WB_Reg.control.MemWrite == 0) {    
        fprintf(fp_output, "  RegWrite = %d   MemToReg = %d\n\n",
                 Read_MEM_WB_Reg.control.RegWrite,
                 Read_MEM_WB_Reg.control.MemToReg);
    }
    else {
        fprintf(fp_output, "  RegWrite = %d   MemToReg = X\n\n",
                 Read_MEM_WB_Reg.control.RegWrite);
    }
    
    if (Read_MEM_WB_Reg.control.MemRead == 1) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = %x   WriteRegNum = %d\n\n", 
            Read_MEM_WB_Reg.ALUResult, Read_MEM_WB_Reg.LWDataValue,
            Read_MEM_WB_Reg.WriteRegNum);
    }
    else if ((Read_MEM_WB_Reg.control.MemRead == 0) &&
             (Read_MEM_WB_Reg.control.RegWrite == 1)) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = X   WriteRegNum = %d\n\n", 
            Read_MEM_WB_Reg.ALUResult, Read_MEM_WB_Reg.WriteRegNum);
    }
    else if ((Read_MEM_WB_Reg.control.MemRead == 0) &&
             (Read_MEM_WB_Reg.control.RegWrite == 0)) {
        fprintf(fp_output, "ALUResult = %x   LWDataValue = X   WriteRegNum = X\n\n", 
            Read_MEM_WB_Reg.ALUResult);
    }    
    fprintf(fp_output, "\n\n");
}

/*
 * At the end of the clock cycle, we will copy what was written to the
 * Write Register to the corresponding Read Register to avoid any kind of
 * contention in our simulated pipeline.
 *
 */
void Copy_write_to_read() {

    /* Copy from Write_IF_ID_Reg -> Read_IF_ID_Reg */
    Read_IF_ID_Reg.Fetched_Instruction = Write_IF_ID_Reg.Fetched_Instruction;

    /* Copy from Write_ID_EX_Reg -> Read_IF_ID_Reg */
    Read_ID_EX_Reg.control.RegDst = Write_ID_EX_Reg.control.RegDst;
    Read_ID_EX_Reg.control.ALUOpt1 = Write_ID_EX_Reg.control.ALUOpt1;
    Read_ID_EX_Reg.control.ALUOpt0 = Write_ID_EX_Reg.control.ALUOpt0;
    Read_ID_EX_Reg.control.ALUSrc = Write_ID_EX_Reg.control.ALUSrc;
    Read_ID_EX_Reg.control.MemRead = Write_ID_EX_Reg.control.MemRead;
    Read_ID_EX_Reg.control.MemWrite = Write_ID_EX_Reg.control.MemWrite;
    Read_ID_EX_Reg.control.RegWrite = Write_ID_EX_Reg.control.RegWrite;
    Read_ID_EX_Reg.control.MemToReg = Write_ID_EX_Reg.control.MemToReg;
    Read_ID_EX_Reg.Fetched_Instruction = Write_ID_EX_Reg.Fetched_Instruction;
    strcpy(Read_ID_EX_Reg.InstructionName, Write_ID_EX_Reg.InstructionName);
    Read_ID_EX_Reg.Opcode = Write_ID_EX_Reg.Opcode;
    Read_ID_EX_Reg.FuncCode = Write_ID_EX_Reg.FuncCode;
    Read_ID_EX_Reg.SrcReg1 = Write_ID_EX_Reg.SrcReg1;
    Read_ID_EX_Reg.SrcReg2 = Write_ID_EX_Reg.SrcReg2;
    Read_ID_EX_Reg.ReadReg1Value = Write_ID_EX_Reg.ReadReg1Value;
    Read_ID_EX_Reg.ReadReg2Value = Write_ID_EX_Reg.ReadReg2Value;
    Read_ID_EX_Reg.WriteRegNum1 = Write_ID_EX_Reg.WriteRegNum1;
    Read_ID_EX_Reg.WriteRegNum2 = Write_ID_EX_Reg.WriteRegNum2;
    Read_ID_EX_Reg.SEOffset = Write_ID_EX_Reg.SEOffset;

    /* Copy from Write_EX_MEM_Reg -> Read_EX_MEM_Reg */
    Read_EX_MEM_Reg.control.MemRead = Write_EX_MEM_Reg.control.MemRead;
    Read_EX_MEM_Reg.control.MemWrite = Write_EX_MEM_Reg.control.MemWrite;
    Read_EX_MEM_Reg.control.RegWrite = Write_EX_MEM_Reg.control.RegWrite;
    Read_EX_MEM_Reg.control.MemToReg = Write_EX_MEM_Reg.control.MemToReg;    
    strcpy(Read_EX_MEM_Reg.InstructionName, Write_EX_MEM_Reg.InstructionName);
    Read_EX_MEM_Reg.ALUResult = Write_EX_MEM_Reg.ALUResult;
    Read_EX_MEM_Reg.SWValue = Write_EX_MEM_Reg.SWValue;
    Read_EX_MEM_Reg.WriteRegNum = Write_EX_MEM_Reg.WriteRegNum;

    /* Copy from Write_MEM_WB_Reg -> Read_MEM_WB_Reg */
    Read_MEM_WB_Reg.control.MemRead = Write_MEM_WB_Reg.control.MemRead;
    Read_MEM_WB_Reg.control.MemWrite = Write_MEM_WB_Reg.control.MemWrite;    
    Read_MEM_WB_Reg.control.RegWrite = Write_MEM_WB_Reg.control.RegWrite;
    Read_MEM_WB_Reg.control.MemToReg = Write_MEM_WB_Reg.control.MemToReg;    
    strcpy(Read_MEM_WB_Reg.InstructionName, Write_MEM_WB_Reg.InstructionName);
    Read_MEM_WB_Reg.LWDataValue = Write_MEM_WB_Reg.LWDataValue;
    Read_MEM_WB_Reg.ALUResult = Write_MEM_WB_Reg.ALUResult;
    Read_MEM_WB_Reg.WriteRegNum = Write_MEM_WB_Reg.WriteRegNum;
}

/**
 * Name:        LoadMIPSInstructions
 * Description: Loads MIPS instructions for disassembling 
 * 
 * Returns:     int; number of instructions loaded for disassembling
 *         
 **/
int LoadMIPSInstructions()
{
    mips_instructions = (unsigned int *) malloc(15 * sizeof(unsigned int));
    
    /* Initialize the MIPS instruction array for disassembling */
    mips_instructions[0]  = 0xa1020000;
    mips_instructions[1]  = 0x00831820;
    mips_instructions[2]  = 0x810afffc;
    mips_instructions[3]  = 0x00663820;
    mips_instructions[4]  = 0x00e24820;
    mips_instructions[5]  = 0x01279820;
    mips_instructions[6]  = 0x03297020;
    mips_instructions[7]  = 0x01635822;
    mips_instructions[8]  = 0x81180000;
    mips_instructions[9]  = 0x81510010;
    mips_instructions[10] = 0x00000000;
    mips_instructions[11] = 0x00000000;
    mips_instructions[12] = 0x00000000;
    mips_instructions[13] = 0x00000000;

    return 14;
}


/**
 * Name: DecodeInstruction
 *
 * Description: Decodes the instruction for the ID_stage and
 *              populates an Instruction Decode structure that will
 *              be used by the ID_stage to determine the controls for
 *              executing the instruction.
 *
 * Returns : Void
 */
void DecodeInstruction(unsigned int mips_instruction)
{
    unsigned int opcode;         /* opcode         6-bits, bit pos 31-26 in word */
    unsigned int src_register1;  /* src_register1  5-bits, bit pos 25-21 in word */
    unsigned int src_register2;  /* src_register2  5-bits, bit pos 20-16 in word */
    unsigned int dest_register3; /* dest_register3 5-bits, bit pos 15-11 in word */
    unsigned int function_code;  /* func_code      6-bits, bit pos 5-0   in word */
    char function_code_str[10];
    short pc_offset;             /* pc_offset 16-bits bit pos 15-0  in word */
  
    /* Get Opcode */
    opcode = (mips_instruction & MASK_OPCODE) >> OPCODE_SHIFT ;
	
    /* Get Register 1 */
    src_register1 = (mips_instruction & MASK_REG1) >> REG1_SHIFT ;
	
    /* Get Register 2 */
    src_register2 = (mips_instruction & MASK_REG2) >> REG2_SHIFT;

    /* Get Register 3 as write register - may not be used but get it anyway */
    dest_register3 = (mips_instruction & MASK_REG3) >> REG3_SHIFT;    

    /* Get pc_offset for instruction - may not be used but get it anyway */
    pc_offset = mips_instruction & MASK_PC_OFFSET;
    
    /* Get Function code - may not be used but get it anyway */
    function_code = mips_instruction & MASK_FUNC;

    /* Generically set opcode, function code, source registers and offset */
    Instruction_Decode.Opcode = opcode;
    Instruction_Decode.FuncCode = function_code;
    Instruction_Decode.SrcReg1 = src_register1;
    Instruction_Decode.SrcReg2 = src_register2;
    Instruction_Decode.SEOffset = pc_offset;

    /* For realistic pipeline simulation, we will track 2 possible write registers
     *  and then resolve at the EX/MEM write using RegDst Control.
     */
    Instruction_Decode.CandidateWriteReg1 = src_register2;  /* rt - bits 20-16 */
    Instruction_Decode.CandidateWriteReg2 = dest_register3; /* rd - bits 15-11 */
    
    /* Decode Instruction: Handle the R-Type instructions here i.e. opcode = 0 */
    if (opcode == 0) {
        if (function_code == ADD_FUNCT)
            strcpy(function_code_str, "add");
        else if (function_code == SUB_FUNCT)
            strcpy(function_code_str, "sub");

        /* Populate the Instruction Decode structure */
        if (function_code != NOP_FUNCT) {
            sprintf(Instruction_Decode.InstructionName, 
                "%s $%d, $%d, $%d",
                function_code_str, dest_register3, src_register1, src_register2);
        }
        else {
            strcpy(Instruction_Decode.InstructionName, "NOP");
        }
    }
    else {
        /* Handle I-Type instructions: load byte, store byte */
        switch(opcode) {
        case LB_OPCODE:
            strcpy(function_code_str, "lb");
            sprintf(Instruction_Decode.InstructionName,
                "lb $%d, %d($%d)",
                src_register2, pc_offset, src_register1);
            break;

        case SB_OPCODE:
            strcpy(function_code_str, "sb");
            sprintf(Instruction_Decode.InstructionName,
                "sb $%d, %d($%d)",
                src_register2, pc_offset, src_register1);
            break;

           default:
               break;
        }
    }
}
