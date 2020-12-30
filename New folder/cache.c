//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include<stdio.h>
#include<stdlib.h>
#include "cache.h"
#include<stdint.h>
#include<math.h>


//
// TODO:Student Information
//
const char *studentName = "Wenran Tian";
const char *studentID   = "A59002429";
const char *email       = "w2tian@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$

uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//
typedef struct node
{
  struct node *last, *next;
  uint32_t val;
}node;

typedef struct set
{
  uint32_t length;
  uint32_t maxlength;
  node *first, *last;
}set;

set *Isets, *Dsets, *L2sets;

// insert and pop if the length is longer the threshold
int insert(set *theset, int val){
	node *nnode=(node*)malloc(sizeof(node));;
	nnode->val = val;
//	nnode.val = val;
	int res = -1;
	if((theset->length) == 0){
		theset->first = nnode;
		theset->last = nnode;
		nnode->last = nnode;
		nnode->next = nnode;
	}else{
		theset->first->last = nnode;
		nnode->next = theset->first;
		theset->first = nnode;
	}
	theset->length += 1;
	if(theset->length > theset->maxlength){
		node *thenode= theset->last;
		theset->last = thenode->last;
//		thenode->last->next = theset->first;
		res = thenode->val;
		free(thenode);
		theset->length -= 1;
	}
	return res;
}
void initSet(set *theset, int maxlength){
	theset->maxlength = maxlength;
	theset->length = 0;
	theset->first = (node*)malloc(sizeof(node));
	theset->last = (node*)malloc(sizeof(node));
}


// search and put the searched node into front
int search(set *theset, int val){  
	int i;
	if(theset->length == 0) return -1;
	node *thenode = theset->first;
	if(thenode->val == val) return 0;
	for(i=1;i<theset->length; i++){
		if(thenode->val == val){
			thenode->last->next = thenode->next;
			thenode->next->last = thenode->last;
			
			theset->first->last = thenode;
			thenode->next = theset->first;
			theset->first = thenode;
			return 0;
		}
		thenode = thenode->next;
	}
	return -1;
}

// pop the front node
void pop(set *theset){
	switch(theset->length){
		case 0: return;
		case 1:{
			free(theset->first);
			initSet(theset, theset->maxlength);
			break;
		}
		default:{
			node *pointer;
			theset->length -= 1;
			pointer = theset->first;
			theset->first = theset->first->next;
			theset->first->last = NULL;
			free(pointer);
			break;
		}
	}
}

int log2t(uint32_t operand){
	int val = 2, log = 0;
	while(val <= operand){
		val *= 2; 
		log++; 
	}
	return log;
}

uint32_t iTagMask, iIndexMask, iIndexBits; 
uint32_t dTagMask, dIndexMask, dIndexBits; 
uint32_t l2TagMask, l2IndexMask, l2IndexBits; 

uint32_t offsetMask=0, offsetBits;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  int i;	
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  //
  //TODO: Initialize Cache Simulator Data Structures
  //
  Isets = (set*)malloc(sizeof(set) * icacheSets);
  Dsets = (set*)malloc(sizeof(set) * dcacheSets);
  L2sets = (set*)malloc(sizeof(set) * l2cacheSets);
  
  for(i=0; i<icacheSets; i++) initSet(&Isets[i], icacheAssoc);
  for(i=0; i<dcacheSets; i++) initSet(&Dsets[i], dcacheAssoc);
  for(i=0; i<l2cacheSets; i++) initSet(&L2sets[i], l2cacheAssoc);
  

  offsetBits = (uint32_t) log2t(blocksize);
  offsetMask = (uint32_t) (1<< offsetBits)-1;
  
  iIndexBits =  (uint32_t) log2t(icacheSets);
  iIndexMask = (uint32_t) (1<< iIndexBits)-1;

  dIndexBits =  (uint32_t) log2t(dcacheSets);
  dIndexMask = (uint32_t) (1<< dIndexBits)-1;  
  
  l2IndexBits =  (uint32_t) log2t(l2cacheSets);
  l2IndexMask = (uint32_t) (1<< l2IndexBits)-1;
  
  printf("//////////////////////////////////");
  printf("the inclusive %d\n", inclusive);
  
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  //
  //TODO: Implement I$
  //
  uint32_t index, tag, speed = icacheHitTime, l2time;
  
  if(icacheSets==0){
    return l2cache_access(addr);
  }
  
  ++icacheRefs;
  index = (addr >> offsetBits) & iIndexMask;
  tag = (addr >> (offsetBits + iIndexBits));
  
  if(search(&Isets[index], tag) == -1){
  		++icacheMisses;
		l2time = l2cache_access(addr);
		icachePenalties += l2time;
		speed += l2time;
		insert(&Isets[index],tag);
  }
  
  return speed;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  //
  //TODO: Implement D$
  //
  uint32_t index, tag, speed = dcacheHitTime, l2time;
  
  if(dcacheSets==0){
    return l2cache_access(addr);
  }
  
  ++dcacheRefs;
  index = (addr >> offsetBits) & dIndexMask;
  tag = (addr >> (offsetBits + dIndexBits));
  
  if(search(&Dsets[index], tag) == -1){
  		++dcacheMisses;
		l2time = l2cache_access(addr);
		dcachePenalties += l2time;
		speed += l2time;
		insert(&Dsets[index],tag);
  }
  
  return speed;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  //
  //TODO: Implement L2$
  //
    if(l2cacheSets==0) return memspeed;

  uint32_t index, tag, popAdd, speed = l2cacheHitTime;
  uint32_t i_index, i_tag, d_index, d_tag;
  
  ++l2cacheRefs;
  index = (addr >> offsetBits) & l2IndexMask;
  tag = (addr >> (offsetBits + l2IndexBits));
  
  if(search(&L2sets[index], tag) == -1){
  		++l2cacheMisses;
		speed += memspeed;
		l2cachePenalties += memspeed;
		popAdd = insert(&L2sets[index],  tag);
		
		if(inclusive && popAdd != -1){
			
			popAdd = (popAdd << l2IndexBits) + index;
			
			i_index = popAdd&iIndexMask;
			i_tag = (popAdd >> iIndexBits);
			if(search(&Isets[i_index], i_tag) == 0) pop(&Isets[i_index]);
	
			d_index = popAdd&dIndexMask;
			d_tag = (popAdd >> dIndexBits);
			if(search(&Dsets[d_index], d_tag) == 0) pop(&Dsets[d_index]);
			
		}
  }
  
  return speed;
}
