//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

//#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "cache.h"

//
// TODO:Student Information
//
const char *studentName = "Yue Zhao/Fanjin Zeng";
const char *studentID   = "A53250901/A53238021";
const char *email       = "yuz105@eng.ucsd.edu/f1zeng@ucsd.edu";

int log2t(uint32_t operand){
	int val = 2, log = 0;
	while(val <= operand){
		val *= 2; 
		log++; 
	}
	return log;
}

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

typedef struct Block
{
  struct Block *prev, *next;
  uint32_t val;
}Block;

typedef struct Set
{
  uint32_t size;
  Block *front, *back;
}Set;

Block* createBlock(uint32_t val)
{
  Block *b = (Block*)malloc(sizeof(Block));
  b->val = val;
  b->prev = NULL;
  b->next = NULL;

  return b;
}

// Didn't check the set size, assume checked by the caller
void setPush(Set* s,  Block *b)
{
  // If set not empty, append to the back
  if(s->size)
  {
    s->back->next = b;
    b->prev = s->back;
    s->back = b;
  }
  // If empty, initialize set
  else
  {
    s->front = b;
    s->back = b;
  }
  (s->size)++;
}

// Pop front
void setPop(Set* s){
  // If empty, do nothing
  if(!s->size)
    return;

  Block *p = s->front;
  s->front = p->next;

  if(s->front)
    s->front->prev = NULL;

  (s->size)--;
  free(p);
}

Block* setPopIndex(Set* s, int index){
  // Invalid Pop index
  if(index > s->size || index<0)
    return NULL;

  Block *p = s->front;

  if(s->size == 1){
    s->front = NULL;
    s->back = NULL;
  }
  else if (index == 0)
  {
    s->front = p->next;
    s->front->prev = NULL;
  }
  else if (index == s->size - 1)
  {
    p = s->back;
    s->back = s->back->prev;
    s->back->next = NULL;
  }
  else{
    for(int i=0; i<index; i++)
      p = p->next;
    p->prev->next = p->next;
    p->next->prev = p->prev;
  }

  p->next = NULL;
  p->prev = NULL;

  (s->size)--;
  //Block ownership transfer to caller
  return p;
}

Set *icache;
Set *dcache;
Set *l2cache;

uint32_t offset_size;
uint32_t offset_mask;

uint32_t icache_index_mask;
uint32_t dcache_index_mask;
uint32_t l2cache_index_mask;

uint32_t icache_index_size;
uint32_t dcache_index_size;
uint32_t l2cache_index_size;

int counter = 0;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//


// Initialize the Cache Hierarchy
//
void
init_cache()
{
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

  icache = (Set*)malloc(sizeof(Set) * icacheSets);
  dcache = (Set*)malloc(sizeof(Set) * dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set) * l2cacheSets);

  for(int i=0; i<icacheSets; i++)
  {
    icache[i].size = 0;
    icache[i].front = NULL;
    icache[i].back = NULL;
  }

  for(int i=0; i<dcacheSets; i++)
  {
    dcache[i].size = 0;
    dcache[i].front = NULL;
    dcache[i].back = NULL;
  }

  for(int i=0; i<l2cacheSets; i++)
  {
    l2cache[i].size = 0;
    l2cache[i].front = NULL;
    l2cache[i].back = NULL;
  }
    offset_size = (uint32_t)log2t(blocksize);
    offset_size += ((1<<offset_size)==blocksize)? 0 : 1;
    offset_mask = (1 << offset_size) - 1;


    icache_index_size = (uint32_t)log2t(icacheSets);
    icache_index_size += ((1<<icache_index_size)==icacheSets)? 0 : 1;
    dcache_index_size = (uint32_t)log2t(dcacheSets);
    dcache_index_size += ((1<<dcache_index_size)==dcacheSets)? 0 : 1;
    l2cache_index_size = (uint32_t)log2t(l2cacheSets);
    l2cache_index_size += ((1<<l2cache_index_size)==l2cacheSets)? 0 : 1;

    icache_index_mask = ((1 << icache_index_size) - 1) << offset_size;
    dcache_index_mask = ((1 << dcache_index_size) - 1) << offset_size;
    l2cache_index_mask = ((1 << l2cache_index_size) - 1) << offset_size;
}

void icacheInvalidate(uint32_t addr){
  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & icache_index_mask) >> offset_size;
  uint32_t tag = addr >> (icache_index_size + offset_size);

  Block *p = icache[index].front;

  for(int i=0; i<icache[index].size; i++){
    if(p->val == tag){ // Find it
      Block *b = setPopIndex(&icache[index], i); //Invalidate it
      free(b);
      return;
    }
    p = p->next;
  }
}
void dcacheInvalidate(uint32_t addr){
  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & dcache_index_mask) >> offset_size;
  uint32_t tag = addr >> (dcache_index_size + offset_size);

  Block *p = dcache[index].front;

  for(int i=0; i<dcache[index].size; i++){
    if(p->val == tag){ // Find it
      Block *b = setPopIndex(&dcache[index], i); // Invalidate it
      free(b);
      return;
    }
    p = p->next;
  }
}
// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t icache_access(uint32_t addr)
{
  // If not intialized, bypass the L1
  if(icacheSets==0){
    return l2cache_access(addr);
  }
  // If intialized, go below
  icacheRefs += 1;

  // Memory address should be ||addr|| == ||tag||index||offset||
  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & icache_index_mask) >> offset_size;
  uint32_t tag = addr >> (icache_index_size + offset_size);

  Block *p = icache[index].front;

  for(int i=0; i<icache[index].size; i++){
    if(p->val == tag){ // Hit
      Block *b = setPopIndex(&icache[index], i); // Get the hit block
      setPush(&icache[index],  b); // move to end of set queue
      return icacheHitTime;
    }
    p = p->next;
  }
  printf("%d\t", index);
  icacheMisses += 1;

  uint32_t penalty = l2cache_access(addr);
  icachePenalties += penalty;

  // icache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(icache[index].size == icacheAssoc) // set filled, replace LRU (front of set queue)
    setPop(&icache[index]);
  setPush(&icache[index],  b);

  return penalty + icacheHitTime;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t dcache_access(uint32_t addr)
{
  // If not intialized, bypass the L1
  if(dcacheSets==0){
    return l2cache_access(addr);
  }
  // If intialized, go below
  dcacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & dcache_index_mask) >> offset_size;
  uint32_t tag = addr >> (dcache_index_size + offset_size);

  Block *p = dcache[index].front;

  for(int i=0; i<dcache[index].size; i++){
    if(p->val == tag){ // Hit
      Block *b = setPopIndex(&dcache[index], i); // Get the hit block
      setPush(&dcache[index],  b); // move to end of set queue
      return dcacheHitTime;
    }
    p = p->next;
  }

  dcacheMisses += 1;


  uint32_t penalty = l2cache_access(addr);
  dcachePenalties += penalty;

  // dcache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(dcache[index].size == dcacheAssoc) // set filled, replace LRU (front of set queue)
    setPop(&dcache[index]);
  setPush(&dcache[index],  b);

  return penalty + dcacheHitTime;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  // If not intialized, bypass the L2
  if(l2cacheSets==0){
    return memspeed;
  }
  // If intialized, go below
  l2cacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & l2cache_index_mask) >> offset_size;
  uint32_t tag = addr >> (l2cache_index_size + offset_size);

  Block *p = l2cache[index].front;
  
//    if(counter <100) {
//		printf("%d   ", addr);
//		counter++; 
//	}


  for(int i=0; i<l2cache[index].size; i++){
    if(p->val == tag){ // Hit
      Block *b = setPopIndex(&l2cache[index], i); // Get the hit block
      setPush(&l2cache[index],  b); // move to end of set queue
      return l2cacheHitTime;
    }
    p = p->next;
  }
  

  l2cacheMisses += 1;

  // l2cache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);
  
  if(l2cache[index].size == l2cacheAssoc){ // set filled, replace LRU (front of set queue)
    if(inclusive){
      //Invalidate L1
      uint32_t swapoutBlockAddr = (((l2cache[index].front->val)<<l2cache_index_size) + index)<<offset_size;
      icacheInvalidate(swapoutBlockAddr);
      dcacheInvalidate(swapoutBlockAddr);
    }
    setPop(&l2cache[index]);
    // else suppose it's a non-inclusive cache
  }
  setPush(&l2cache[index],  b);

  l2cachePenalties += memspeed;
  return memspeed + l2cacheHitTime;
}
