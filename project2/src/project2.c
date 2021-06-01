/*
 * David Bardwell
 * Computer Architecture Project 2 - Cache Memory Subsystem
 * 
 * for Computer Organization and Design
 * Tyngsboro campus - Wednesday evening 6-9pm class
 *
 * Due: October 29, 2008
 *
 */
 /* Project comments:
  *  This project simulates how a 2-way set associative write-back cache 
  *  works in terms of handling read and write requests from the processor.
  *
  *  The program below does the following:
  *  (1) Takes two inputs from the command line for the input and output
  *      files for processing read, write and display cache requests.
  *  (2) Top-level processing includes reading a command from the file and
  *      processing the read, write or display command which may include
  *      updating the cache and main memory.
  *
  *  To execute program:
  *    project2 <inputfilename> <outputfilename>
  * 
  *  Note: input2.txt and output2.txt will be defaulted if program arguments
  *        are not provided on command line
  *
  *  The program is written in such a way that one could easily alter the
  *  set associativeness of the cache, i.e. make it 4 instead of 2
  *  Just change the define for CACHE_ASSOCIATIVENESS, recompile and re-run.
  */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

/* Define top-level logical sizes for 2-way set associative cache */
#define CACHE_ASSOCIATIVENESS  2
#define CACHE_SET_COUNT        8
#define SYSTEM_BLOCK_SIZE      8
#define TOTAL_MEMORY_SIZE      0x1000

/* Defines for bitwise operations */
#define OFFSET_MASK  0x00000007  /* 8 byte block requires 3 bits for offset */
#define SET_MASK     0x00000038  /* Mask to get the cache set */  
#define SET_SHIFT    3           /* positions to right shift to get the cache set */
#define TAG_MASK     0xFFFFFFC0  /* Mask to get the tag */
#define TAG_SHIFT    6           /* positions to right shift to get the tag */

/* enumeration information for the 3 commands or memory operations, 
   cache operation results
   and postion in cache set found */
enum mem_operation  {MEM_READ =   1, MEM_WRITE,  CACHE_DISPLAY};
enum cache_result   {CACHE_HIT1 = 0, CACHE_HIT2, CACHE_HIT3, CACHE_HIT4, CACHE_MISS};
enum cache_position {POSITION1 =  0, POSITION2,  POSITION3,  POSITION4};

/* Definition of one cache set element for the overall cache */
typedef struct AssociativeCacheSetElement {
    short dirty;
    short valid;
    int usage_count;
    int tag;
    unsigned char cache_mem[SYSTEM_BLOCK_SIZE];

} AssociativeCacheSetElement;

/* The cache will be a two dimensional array of AssociativeCacheSetElements */
AssociativeCacheSetElement Cache[CACHE_SET_COUNT][CACHE_ASSOCIATIVENESS];

/* Main Memory will simply be a flat character array for byte addessability */
unsigned char Main_mem[TOTAL_MEMORY_SIZE];  /* Main memory is exactly 4K bytes */

/* Forward declarations */
void initialize();
int  getNextCommand(FILE *inputfile, int *mem_addr, unsigned char *mem_contents);
int  read_memory(int mem_address, char *actionTaken);
int  write_memory(int mem_address, unsigned char mem_contents, char *actionTaken);
void display_cache(FILE *fp_output);
void readMainMemoryIntoCache(int tagNumber, int cacheSet, int cacheSetPosition);
void writeCacheToMainMemory(int cacheSet, int cacheSetPosition);

/* forward declaration - helper functions */
int  findSet(int memory_address);
int  findTag(int memory_address);
int  findOffset(int memory_address);
int  inCache(int cacheSet, int tagNumber);
int  findCacheSetPosition(int cacheSet, int *conflictFlag);

/*
 * main() - main driver to open input file input2.txt and process
 *          the commands in the input file which includes simulating
 *          cache reads, cache writes and display cache contents.
 *
 *  returns: 0; on normal exit
 *
 */
int main(int argc, char *argv[])
{
    char  *inputfile;
    char  *outputfile;
    FILE  *fp_input;
    FILE  *fp_output;
    int   mem_addr;
    unsigned char mem_contents;
    int   command;
    int   cacheStatus;
    char  actionTaken[256];
    int   doneFlag, i, j;

    /* get input arguments: 1st is input file i.e. input2.txt and
     * second argument is output file i.e. output2.txt
     */
    if (argc == 1) {
        inputfile = (char *) malloc(30);
        outputfile = (char *) malloc(30);
        strcpy(inputfile, "input2.txt");
        strcpy(outputfile, "output2.txt");
    }
    else if (argc == 2) {
        inputfile = argv[1];
        outputfile = (char *) malloc(30);
        strcpy(outputfile, "output2.txt");
    }
    else if (argc >= 3) {
        inputfile = argv[1];
        outputfile = argv[2];
    }

    /* Initialize Main Memory and Cache */
    initialize();

    /* open input and output files */
    fp_input = fopen(inputfile, "r");
    fp_output = fopen(outputfile, "w");

    /*     Main Loop    */
    /* Read the input file for memory operations and write results
     * to the output file until the input file is exhausted
     */
    doneFlag = 0;
    while (!doneFlag) {
        command = getNextCommand(fp_input, &mem_addr, &mem_contents);
        if (command != EOF) {
            if (command == MEM_READ) {
                cacheStatus = read_memory(mem_addr, actionTaken);
                fprintf(fp_output, "%s %x\n", "Command: Read Memory address", mem_addr);
                fprintf(fp_output, "%s\n", actionTaken);

            }
            else if (command == MEM_WRITE) {
                cacheStatus = write_memory(mem_addr, mem_contents, actionTaken);
                fprintf(fp_output, "Command: Write Memory address %x with value %x\n", 
                        mem_addr, mem_contents);
                fprintf(fp_output, "%s\n", actionTaken);
            }
            else {
                fprintf(fp_output, "Command: Display Cache\n");
                display_cache(fp_output);
            }
        }
        else
            doneFlag = 1;
    }

    /* Dump changes to main memory and verify two writes flushed from cache are present */
    fprintf(fp_output, "\n\nVerify main memory also\n");
    j = 0;
    for (i = 0; i < TOTAL_MEMORY_SIZE; i++) {
        if ( (Main_mem[i] % 256) != j) {
            fprintf(fp_output, "*** Main Memory Address %x has modified value %x\n",
                    i, Main_mem[i]);
        }
        j++;
        if (j == 256)
            j = 0;
    }
    
    /* close input and output files and free memory as needed */
    fclose(fp_input);
    fclose(fp_output);
    if (argc == 1) {
        free(inputfile);
        free(outputfile);
    }
    else if (argc == 2) {
        free(outputfile);
    }
    return 0;
}

/* Initialize main memory and the cache */
void initialize()
{
    int mem_address, i, j, k;
    
    /* first, initialize main memory */
    /* 0x10 is 16 and 0x100 is 256   */
    for (j = 0; j < 0x10; j++) {
        for (i = 0; i < 0x100; i++) {
            mem_address = j * 0x100 + i;
            Main_mem[mem_address] = i;
        }
    }

    /* second, initialize the cache */
    for (i = 0; i < CACHE_SET_COUNT; i++) {
        for (j = 0; j < CACHE_ASSOCIATIVENESS; j++) {
            Cache[i][j].dirty = 0;
            Cache[i][j].valid = 0;
            Cache[i][j].usage_count = 0;
            Cache[i][j].tag = 0;
            for (k = 0; k < SYSTEM_BLOCK_SIZE; k++) {
                Cache[i][j].cache_mem[k] = '\0';
            }
        } 
    }
}

/*
 * getNextCommand() - gets next command from input file
 *    command type as return and input variables for
 *    memory address and memory contents for write
 *
 *  Returns:
 *    enum mem_operation; MEM_READ, MEM_WRITE, CACHE_DISPLAY or EOF
 *
 */
int getNextCommand(FILE *fp, int *mem_address, unsigned char *mem_contents)
{
    char nextCommand[3];
    int returnCommand;
    int readbyte;

    if (fscanf(fp, "%s\n", nextCommand) > 0) {
        if (strcmp(nextCommand, "R") == 0) {
            fscanf(fp, "%x\n", mem_address);
            returnCommand = MEM_READ;
        }
        else if (strcmp(nextCommand, "W") == 0) {
            fscanf(fp, "%x\n", mem_address);
            fscanf(fp, "%x\n", &readbyte);
            *mem_contents = (char) readbyte;
            returnCommand = MEM_WRITE;
        }
        else {
            returnCommand = CACHE_DISPLAY;
        }
    }
    else
        returnCommand = EOF;
    
    return returnCommand;
}

/*
 * read_memory - simulate a memory read by looking in the cache for
 *             the requested memory address. If found, then we have a 
 *             cache hit; otherwise we have a cache miss. In the case of
 *             a cache miss we must get the address from main memory.
 *  return: enum cache_result; CACHE_HIT1, CACHE_HIT2 or CACHE_MISS
 *
 */
int read_memory(int mem_address, char *actionTaken)
{
    int cacheSet;
    int tagNumber;
    int offset;
    int cacheStatus;
    unsigned char byteValue;
    int cacheSetPosition;
    int conflictFlag;
    
    /* Decompose memory address into cache set, tag and offset */
    cacheSet = findSet(mem_address);
    tagNumber = findTag(mem_address);
    offset = findOffset(mem_address);

    /* Find out if memory address is in cache, and process accordingly */
    /* Note: cacheStatus will be either CACHE_HIT1 or CACHE_HIT2 if found in cache */
    cacheStatus = inCache(cacheSet, tagNumber);
    
    if (cacheStatus < CACHE_MISS) {
        /* Cache Hit in cacheSet position 0 or 1 for 2-way set associative cache */
        Cache[cacheSet][cacheStatus].usage_count++;
        byteValue = Cache[cacheSet][cacheStatus].cache_mem[offset];
        sprintf(actionTaken, 
           "Read Cache Hit: Memory Address %x found in Cache Set %x in position %x with memory value %x\n", 
               mem_address, cacheSet, cacheStatus, byteValue);
    }
    else {
        /* Cache Miss - read block from memory */
        
        /* First, find which cacheSet position to place memory into cache */
        conflictFlag = 0;
        cacheSetPosition = findCacheSetPosition(cacheSet, &conflictFlag);
        
        /* If there is a conflict, need to write cache to memory (if dirty) - write back cache */
        if (conflictFlag == 1) {
            if (Cache[cacheSet][cacheSetPosition].dirty == 1) {
                writeCacheToMainMemory(cacheSet, cacheSetPosition);
            }
        }
        
        /* Read from main memory into the cache and update cache information */
        readMainMemoryIntoCache(tagNumber, cacheSet, cacheSetPosition);
        byteValue = Cache[cacheSet][cacheSetPosition].cache_mem[offset];
            
        /* Record the cache miss information */
        sprintf(actionTaken, 
           "Read Cache Miss: Memory Address %x read into cache set %x in position %x from memory with value %x\n", 
              mem_address, cacheSet, cacheSetPosition, byteValue);

    }
    return cacheStatus;
}

/*
 * write_memory - simulate a memory write by looking in the cache for
 *             the requested memory address. If found, then we have a 
 *             cache hit; otherwise we have a cache miss. In the case of
 *             a cache miss we must read the address block from main memory.
 *             We then update the memory address in the cache and not 
 *             in main memory.
 *             The code is very similar to that of read_memory().
 *  return: enum cache_result; CACHE_HIT1, CACHE_HIT2 or CACHE_MISS
 *
 */
int write_memory(int mem_address, unsigned char write_contents, char *actionTaken)
{
    int cacheSet;
    int tagNumber;
    int offset;
    int cacheStatus;
    int cacheSetPosition;
    int conflictFlag;
    
    /* Decompose memory address into cache set, tag and offset */
    cacheSet = findSet(mem_address);
    tagNumber = findTag(mem_address);
    offset = findOffset(mem_address);

    /* Find out if memory address is in cache, and process accordingly */
    cacheStatus = inCache(cacheSet, tagNumber);
    
    if (cacheStatus < CACHE_MISS) {
        /* Cache Hit in cacheSet position 0 or 1*/
        Cache[cacheSet][cacheStatus].usage_count++;
        Cache[cacheSet][cacheStatus].cache_mem[offset] = write_contents;
        Cache[cacheSet][cacheStatus].dirty = 1;
        sprintf(actionTaken, 
           "Write Cache Hit: Memory Address %x found in cache set %x in position %x and updated with value %x\n", 
               mem_address, cacheSet, cacheStatus, write_contents);
    }
    else {
        /* Cache Miss - read block from memory */
        
        /* First, find which cacheSet Position to place memory into cache */
        conflictFlag = 0;
        cacheSetPosition = findCacheSetPosition(cacheSet, &conflictFlag);
        
        /* If there is a conflict, need to write cache to memory (if dirty) - write back cache */
        if (conflictFlag == 1) {
            if (Cache[cacheSet][cacheSetPosition].dirty == 1) {
                writeCacheToMainMemory(cacheSet, cacheSetPosition);
            }
        }
        
        /* Read from main memory into the cache and update cache information */
        readMainMemoryIntoCache(tagNumber, cacheSet, cacheSetPosition);
        
        /* Record the cache miss information for write */
        /* Note: usage_count was already updated from read from main memory */
        Cache[cacheSet][cacheSetPosition].cache_mem[offset] = write_contents;
        Cache[cacheSet][cacheSetPosition].dirty = 1; /* set dirty flag as we have updated the cache */
        sprintf(actionTaken, 
            "Write Cache Miss: Memory Address %x read into cache set %x in position %x from memory and then updated with value %x\n", 
               mem_address, cacheSet, cacheSetPosition, write_contents);
    }
    return cacheStatus;
}

void display_cache(FILE *fp_output)
{
    /* Display the cache as a snapshot of where things stand in the cache */
    int i, j, k;
    
    fprintf(fp_output, "Display Cache - here is the snapshot of what is in the cache\n");
    fprintf(fp_output, "Set  Usage  Dirty  Valid  Tag    Data \n");
    for (i = 0; i < CACHE_SET_COUNT; i++) {
        for (j = 0; j < CACHE_ASSOCIATIVENESS; j++) {
            fprintf(fp_output, " %x    %2d      %d      %d    %3x   ", 
                i, Cache[i][j].usage_count, Cache[i][j].dirty, Cache[i][j].valid, Cache[i][j].tag);

            for (k = 0; k < SYSTEM_BLOCK_SIZE; k++) {
                fprintf(fp_output, "%2x ", Cache[i][j].cache_mem[k]);
            }
            fprintf(fp_output, "\n");
        }
        fprintf(fp_output, "\n");    
    }
}

/*
 * readMainMemoryIntoCache() 
 *   reads main memory into the cache and updates the cache control
 *   information. 
 *
 * returns void
 */
void readMainMemoryIntoCache(int tagNumber, int cacheSet, int cacheSetPosition)
{
    int i, startingBlockAddress;

    /* Calculate starting address in memory from tag and cache set number 
     * block offset always starts at 0.
     */
    startingBlockAddress = (tagNumber << TAG_SHIFT) + (cacheSet << SET_SHIFT);

    /* update the memory in the cache from main memory */
    for (i = startingBlockAddress ;
          i < startingBlockAddress + SYSTEM_BLOCK_SIZE;
          i++) {
        Cache[cacheSet][cacheSetPosition].cache_mem[i - startingBlockAddress] = Main_mem[i];
    }
    /* update cache control information from latest read */
    Cache[cacheSet][cacheSetPosition].usage_count++;
    Cache[cacheSet][cacheSetPosition].tag = tagNumber;
    Cache[cacheSet][cacheSetPosition].valid = 1;
    Cache[cacheSet][cacheSetPosition].dirty = 0; /* reset dirty with new read */
}

/*
 * writeCacheToMainMemory()
 *  writes the cache memory contents back to main memory
 *  This will only need to be done in the event of a cache conflict
 *  where there is no where in the cache to place the requested memory.
 *
 *  Returns: void
 */
void writeCacheToMainMemory(int cacheSet, int cacheSetPosition)
{
    int i, startingBlockAddress;

    startingBlockAddress = (Cache[cacheSet][cacheSetPosition].tag << TAG_SHIFT) +
                            (cacheSet << SET_SHIFT);

    for (i = startingBlockAddress ; 
          i < startingBlockAddress + SYSTEM_BLOCK_SIZE;
          i++) {
        Main_mem[i] = Cache[cacheSet][cacheSetPosition].cache_mem[i - startingBlockAddress];
    }
}

/* Helper functions to extract cacheSet, tag and offset from a memory address */
int findSet(int memory_address)
{
    int cacheSet;

    cacheSet = (memory_address & SET_MASK) >> SET_SHIFT;
    return cacheSet;
}

int findTag(int memory_address)
{
    int tagNumber;

    tagNumber = (memory_address & TAG_MASK) >> TAG_SHIFT;
    return tagNumber;
}

int findOffset(int memory_address)
{
    int offset;

    offset = (memory_address & OFFSET_MASK);
    return offset;
}

/*
 * inCache()
 *  Determines if the memory address is currently in the cache by
 *  examining the valid field and if the tags match
 * 
 *  returns: enum cache_result; CACHE_HIT1, CACHE_HIT2 or CACHE_MISS
 */
int inCache(int cacheSet, int tagNumber)
{
    int cacheStatus, i;
    cacheStatus = CACHE_MISS;  /* a cache miss until found, if found */
    
    
    for (i = 0; i < CACHE_ASSOCIATIVENESS ; i++) {
        /* Cache hit requires a valid bit and matching tags */
        /* Check first position in cache set */
        if (Cache[cacheSet][i].valid == 1) {
            if (Cache[cacheSet][i].tag == tagNumber) {
                // cache hit
                cacheStatus = i;
                break;
            }
        }
    }
    return cacheStatus;
}

/*
 * findCacheSetPosition()
 *  Given that a cache miss occurred, find where to place the new
 *  block to be read in from memory. Resolve conflicts with
 *  the usage count. Cache position 0 in cache set will be reloaded 
 *  from memory if there is a tie resolving the conflict. 
 *
 * returns: enum cache_position; POSITION1 or POSITION2
 *
 */
int findCacheSetPosition(int cacheSet, int *conflictFlag)
{
    int cachePosition, i;
    int lowestUsageCount;

    cachePosition = -1;

    /* Find first position in cache set that is available */
    for (i = 0; i < CACHE_ASSOCIATIVENESS ; i++) {
        if (Cache[cacheSet][i].valid == 0) {
            cachePosition = i;
            break;
        }
    }
    
    /* If we did not find an available position, 
     * use the usage count to find where to place block
     */
    if (cachePosition == -1) {
        /* handle cache conflict with usage_count */
        *conflictFlag = 1;

        lowestUsageCount = Cache[cacheSet][0].usage_count;
        cachePosition = 0;
        for (i = 1; i < CACHE_ASSOCIATIVENESS ; i++) {
            if (Cache[cacheSet][i].usage_count < lowestUsageCount) {
                lowestUsageCount = Cache[cacheSet][i].usage_count;
                cachePosition = i;
            }
        }
    }
    return cachePosition;
}

