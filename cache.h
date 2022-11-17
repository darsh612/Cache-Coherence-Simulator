/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>
#include <iomanip>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum {
   INVALID = 0,
   VALID,
   MODIFIED,
   SHARED ,
   EXCLUSIVE 
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:modified, 3:shared, 4:exclusive
   ulong seq; 
 
public:
   cacheLine()                { tag = 0; Flags = 0; }
   ulong getTag()             { return tag; }
   ulong getFlags()           { return Flags;}
   ulong getSeq()             { return seq; }
   void setSeq(ulong Seq)     { seq = Seq;}
   void setFlags(ulong flags) {  Flags = flags;}
   void setTag(ulong a)       { tag = a; }
   void invalidate()          { tag = 0; Flags = INVALID; } //useful function
   bool isValid()             { return ((Flags) != INVALID); }
};

class Cache
{
public:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines, hist_size, hist_assoc, hist_sets;
   ulong reads,readMisses,writes,writeMisses,writeBacks;
   ulong flushes,BusRdX,BusUpg,intervention,invalidations, cache2cache;
   ulong memtrans;
   ulong wasted,useful,filtered;
   int procs;

   //******///
   //add coherence counters here///
   //******///

   cacheLine **cache;
   cacheLine **hist_filter;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
   
    ulong currentCycle;  
    int self;
    Cache **cachearr;
     
   Cache(int,int,int,int, Cache *a[]);
   ~Cache() { delete cache;}

   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   int searchblock(ulong);

   cacheLine *hist_findLineToReplace(ulong addr);
   cacheLine *hist_fillLine(ulong addr);
   cacheLine * hist_findLine(ulong addr);
   cacheLine * hist_getLRU(ulong);
   int hist_searchblock(ulong);
   
   ulong getRM()     {return readMisses;} 
   ulong getWM()     {return writeMisses;} 
   ulong getReads()  {return reads;}       
   ulong getWrites() {return writes;}
   ulong getWB()     {return writeBacks;}
   
   void writeBack(ulong) {writeBacks++;}
   void Access_MSI(int,ulong, const char*);
   void Access_MSI_Busupg(int,ulong, const char*);
   void Access_MESI(int, ulong, const char*);
   void Access_MESI_Upg(int, ulong, const char*);
   void Snoop_MESI_Upg(int, ulong, const char*);
   void printStats(ulong, int);
   void updateLRU(cacheLine *);
   void hist_updateLRU(cacheLine *);
   //******///
   //add other functions to handle bus transactions///
   //******///

};


#endif
