/*
 * csim.c
 * Name: Bin Feng
 * Andrew ID: bfeng
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "cachelab.h"


typedef struct Block{
	int valid;		/* Valid bit*/
	int tag;		/* Tag */
	int LRUCounter;	/* LRU counter */
}Block;

typedef struct Set{
	Block* Blocks;
}Set;

typedef struct Cache{
	Set* sets;
}Cache;

/* E.g. ./csim -v -s 4 -E 1 -b 4 -t traces/yi.trace
• -h: Optional help ﬂag that prints usage info
• -v: Optional verbose ﬂag that displays trace info
• -s <s>: Number of set index bits (S = 2^s is the number of sets)
• -E <E>: Associativity (number of Blocks per set)
• -b <b>: Number of block bits (B = 2^b is the block size)
• -t <tracefile>: Name of the valgrind trace to replay
*
*/
typedef struct Options{
	int help;			/* -h */
	int verbose;		/* -v */
	int sets;			/* -s */
	int associativity;	/* -E */
	int blocks;			/* -b */
	char* traceFile;	/* -t */
}Options;


int globalHits;
int globalMiss;
int globalEvictions;

int main(int argc, char** argv);
void resetGlobalVariables();
void resetOptions(Options* options);
int parseOptions(int argc, char** argv, Options* options);
int parseTraceFile(Options* options);

int main(int argc, char** argv)
{
	Options options;
	resetGlobalVariables();

	if(parseOptions(argc, argv, &options) < 0){
		printf("Parse options error!\n");
		exit(-1);
	}

	if(parseTraceFile(&options) < 0){
		printf("Parse trace file error!\n");
		exit(-1);
	}

    printSummary(globalHits, globalMiss, globalEvictions);
    return 0;
}


void resetGlobalVariables(){
	globalHits = 0;
	globalMiss = 0;
	globalEvictions = 0;
}

void resetOptions(Options* options){
	options->help = 0;
	options->verbose = 0;
	options->sets = -1;
	options->associativity = -1;
	options->blocks = -1;
	options->traceFile = NULL;
}

int parseOptions(int argc, char** argv, Options* options){

	int opt;
	resetOptions(options);

	while((opt=getopt(argc, argv, "hvs:E:b:t:")) != -1){
		switch(opt){
			case 'h':
				options->help = 1;
				break;
			case 'v':
				options->verbose = 1;
				break;
			case 's':
				options->sets = atoi(optarg);
				if(options->sets < 0){
					printf("-s argument error.");
					return -1;
				}
				break;
			case 'E':
				options->associativity = atoi(optarg);
				if(options->associativity < 0){
					printf("-E argument error.");
					return -1;
				}
				break;
			case 'b':
				options->blocks = atoi(optarg);
				if(options->blocks < 0){
					printf("-b argument error.");
					return -1;
				}
				break;
			case 't':
				options->traceFile = (char*)malloc(strlen(optarg)+1);
				if(!options->traceFile){
					printf("-t argument error.\n");
					return -1;
				}
				strncpy(options->traceFile, optarg, strlen(optarg));
				break;
			default:
				// printf("opt:%c\n", opt);
				printf("Wrong argument!\n");
				return -1;
		}
	}

	return 0;
}



int parseTraceFile(Options* options){

	Cache* cache = NULL;
	FILE* file = NULL;
	int i, j;


	/* Initialize a simulated cache */
	cache = (Cache*)malloc(sizeof(Cache));

	if(!cache){
		printf("Malloc cache failed!\n");
		return -1;
	}

	cache->sets = (Set*)malloc(sizeof(Set) * (1<<(options->sets)));
	if(!cache->sets){
		printf("Malloc set failed!\n");
		return -1;
	}

	for(i=0; i<(1<<(options->sets)); i++){
		cache->sets[i].Blocks = (Block*)malloc(sizeof(Block) * (1<<(options->associativity)));
		
		if(!cache->sets[i].Blocks){
			printf("Malloc Block failed!\n");
			return -1;
		}
		
		for(j=0; j<(options->associativity); j++){
			cache->sets[i].Blocks[j].valid = 0;
			cache->sets[i].Blocks[j].tag = -1;
			cache->sets[i].Blocks[j].LRUCounter = 0;
		}
		
	}

	printf("traceFile:%s\n", options->traceFile);
	file = fopen(options->traceFile, "r+");
	if(!file){
		printf("Error opening file.\n");
		return -1;
	}

	char operation;
	char address[100];
	int size;
	long addr;

	int setIndex, tag;
	int setIndexMask = 0x7fffffffffffffff >> (63 - options->sets);
	int tagMask = 0x7fffffffffffffff >> (63 - options->sets - options->blocks);

	int traceLineHits = 0;
	int traceLineMiss = 0;
	int traceLineEvictions = 0;
	int counts = 0;

	while(1){
		fscanf(file, " %c %s,%d", &operation, address, &size);
		if(operation == 'I'){
			continue;
		}
		if(feof(file)){
			break;
		}

		addr = strtol(address, NULL, 16);
		// printf("%ld\n", addr);

		/* Parse address */
		setIndex = (addr>>options->blocks) & setIndexMask;
		tag = (addr>>(options->sets+options->blocks)) & tagMask;

		printf("setIndex:%d, tag:%d\n", setIndex, tag);

		traceLineHits = 0;
		traceLineMiss = 0;
		traceLineEvictions = 0;

		for(i=0; i<options->associativity; i++){
			// If original block is not empty and hit
			if(cache->sets[setIndex].Blocks[i].valid == 1 && cache->sets[setIndex].Blocks[i].tag == tag){
				traceLineHits++;
				globalHits++;
				cache->sets[setIndex].Blocks[i].LRUCounter = counts++;

				if(operation == 'M'){
					traceLineHits++;
					globalHits++;
				}
				break;
			}
			// If original block is empty
			else if(cache->sets[setIndex].Blocks[i].valid == 0){
				traceLineMiss++;
				globalMiss++;

				if(operation == 'M'){
					traceLineHits++;
					globalHits++;
				}

				cache->sets[setIndex].Blocks[i].valid = 1;
				cache->sets[setIndex].Blocks[i].tag = tag;
				cache->sets[setIndex].Blocks[i].LRUCounter = counts++;
				break;
			}
			// If all blocks are full and no hit, evit the least recently used block
			else if(i == options->associativity-1){
				int minBlockIndex = 0;
				for(j=0; j<options->associativity; j++){
					if(cache->sets[setIndex].Blocks[j].LRUCounter < cache->sets[setIndex].Blocks[minBlockIndex].LRUCounter){
						minBlockIndex = j;
					}
				}

				traceLineMiss++;
				globalMiss++;
				traceLineEvictions++;
				globalEvictions++;

				if(operation == 'M'){
					traceLineHits++;
					globalHits++;
				}

				cache->sets[setIndex].Blocks[minBlockIndex].tag = tag;
				cache->sets[setIndex].Blocks[minBlockIndex].LRUCounter = counts++;
				break;
			}
		}
	}


	return 0;
}