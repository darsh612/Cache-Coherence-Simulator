/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

/*Cache:: Cache(int b, int num_procs, Cache **hist_fil1){
   
   currentCycle=0;
   procs=num_procs;
   lineSize   = (ulong)(b);
   assoc      = 1;   
   sets       = 16;
   size       = (ulong) lineSize*assoc*sets;
   numLines   = (ulong)(size/lineSize);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
   hist_fil=hist_fil1;
   tagMask=0;
   for(ulong i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }

   cache = new cacheLine*[sets];
   for(ulong i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(ulong j=0; j<assoc; j++) 
      {
         //printf("aaa\n");
         cache[i][j].invalidate();
      }
   } 

}*/

Cache::Cache(int s,int a,int b, int num_procs, Cache **cachearr1)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   cache2cache=invalidations=intervention=0;

   procs=num_procs;
   memtrans=0;
   flushes=BusRdX=BusUpg=0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));
   hist_assoc = 1;
   hist_sets  = 16;
   hist_size  = hist_assoc*hist_sets*lineSize;

   //printf("L - %lu\n",log2Blk);
  
   //*******************//
   //initialize your counters here//
   //*******************//
   cachearr=cachearr1;
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
         cache[i][j].invalidate();
      }
   }   
   hist_filter = new cacheLine*[hist_sets];
   for(ulong i=0; i<hist_sets; i++)
   {
      hist_filter[i]= new cacheLine[hist_assoc];
      for(ulong j=0;j<hist_assoc;j++){
         hist_filter[i][j].invalidate();
      }
   }  
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/


//*************************MSI*****************************//

void Cache::Access_MSI(int p,ulong addr,const char* op)
{
   self=p;
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
         
   if(*op == 'w') writes++;
   else if(*op == 'r') reads++;
   if((*op == 'r')||(*op == 'w'))
   {
      cacheLine * line = findLine(addr);
      if(line == NULL)/*miss*/  //Invalid State
      {
         if(*op == 'w') writeMisses++;
         else readMisses++;
   
         cacheLine *newline = fillLine(addr);
   
         if(*op == 'w'){  
            newline->setFlags(MODIFIED); 
            BusRdX++;          //BusRdX increases
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op ="x";
               cachearr[i]->Access_MSI(i,addr,op.c_str());
            }
         }   
         else if(*op == 'r'){
            newline->setFlags(SHARED);
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op = "d";
               cachearr[i]->Access_MSI(i,addr,op.c_str());
            }
         }
      }
      
      else
      {
         /**since it's a hit, update LRU and update dirty flag**/
         updateLRU(line);
         if(line->getFlags()==MODIFIED){
            line->setFlags(MODIFIED);   //If in modified stay in modified
         }
   
         else if(line->getFlags()== SHARED){
            if(*op == 'r'){
               line->setFlags(SHARED);
            }
            else if(*op == 'w'){
               line->setFlags(MODIFIED); 
               BusRdX++;
               memtrans++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="x";
                  cachearr[i]->Access_MSI(i,addr,op.c_str());
               }
            }
         }
      }
   }
   else if((*op == 'd')|| (*op =='x')){
      cacheLine* line = findLine(addr);
      if(line == NULL){
         if( *op == 'd'|| *op =='x')
         {

         }
      }
      else{
         if(line->getFlags()==MODIFIED)
         { 
            writeBack(addr);
            if(*op == 'd')
            {
               line->setFlags(SHARED);
               intervention++;
               flushes++;
            }
            else if(*op == 'x'){
               line->invalidate();
               invalidations++;
               flushes++;
            }
         }
         if( line->getFlags()==SHARED){
            if( *op == 'd'){
               line->setFlags(SHARED);
            }
            else if( *op == 'x'){
               line->invalidate();
               invalidations++;
            }
         }
      }
   }
}

//***************************END MSI*******************************//

//**************************MSI BUSUPG*****************************//

void Cache::Access_MSI_Busupg(int p,ulong addr,const char* op)
{
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
   self=p;
         
   if(*op == 'w') writes++;
   else if(*op == 'r') reads++;
   if((*op == 'r')||(*op == 'w'))
   {
      cacheLine * line = findLine(addr);
      if(line == NULL)/*miss*/  //Invalid State
      {
         if(*op == 'w') writeMisses++;
         else readMisses++;
   
         cacheLine *newline = fillLine(addr);
   
         if(*op == 'w'){  
            newline->setFlags(MODIFIED); 
            BusRdX++;          //BusRdX increases
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op ="x";
               cachearr[i]->Access_MSI_Busupg(i,addr,op.c_str());
            }
         }   
         else if(*op == 'r'){
            newline->setFlags(SHARED);
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op = "d";
               cachearr[i]->Access_MSI_Busupg(i,addr,op.c_str());
            }
         }
      }
      
      else
      {
         /**since it's a hit, update LRU and update dirty flag**/
         updateLRU(line);
         if(line->getFlags()==MODIFIED){
            line->setFlags(MODIFIED);   //If in modified stay in modified
         }
   
         else if(line->getFlags()== SHARED){
            if(*op == 'r'){
               line->setFlags(SHARED);
            }
            else if(*op == 'w'){
               line->setFlags(MODIFIED);
               BusUpg++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="u";
                  cachearr[i]->Access_MSI_Busupg(i,addr,op.c_str());
               }
            }
         }
      }
   }
   else if((*op == 'd')|| (*op =='x')||(*op =='u')){
      cacheLine* line = findLine(addr);
      if(line == NULL){
         if( *op == 'd'|| *op =='x' || *op =='u')
         {

         }
      }
      else{
         if(line->getFlags()==MODIFIED)
         {
            if(*op == 'd')
            {
               line->setFlags(SHARED);
               intervention++;
               writeBack(addr);
               flushes++;
            }
            else if(*op == 'x'){
               line->invalidate();
               invalidations++;
               writeBack(addr);
               flushes++;
            }
         }
         if( line->getFlags()==SHARED){
            if( *op == 'd'){
               line->setFlags(SHARED);
            }
            else if( *op == 'x' || *op =='u'){
               line->invalidate();
               invalidations++;
            }
         }
      }
   }
}

//**************************END MSI BUSUPG*************************//

//***************************MESI**********************************//

void Cache::Access_MESI(int p, ulong addr, const char* op)
{
   self=p;
   currentCycle++;
   if(*op == 'w') writes++;
   else if(*op == 'r') reads++;

   //checking if block exists in other caches or not
   int check_line=0;

   for(int i=0;i<procs;i++){
      if(i==self){
         continue;
      }
      check_line= cachearr[i]->searchblock(addr);
      if(check_line==1){
         break;
      }
   }
   if ((*op =='r')||(*op =='w')){
      cacheLine* line = findLine(addr);
      if(line == NULL){
         if(*op == 'w') writeMisses++;
         else readMisses++;

         cacheLine *newline= fillLine(addr);
         if(*op =='w'){
            newline->setFlags(MODIFIED);
            if(check_line == 1){
               cache2cache++;
            }
            BusRdX++;
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op="x";
               cachearr[i]->Access_MESI(i,addr,op.c_str());
            }
         }
         else if(*op =='r'){
            if(check_line==1){
               newline->setFlags(SHARED);
               cache2cache++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="d";
                  cachearr[i]->Access_MESI(i,addr,op.c_str());
               }
            }
            else if(check_line==0){
               newline->setFlags(EXCLUSIVE);
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="d";
                  cachearr[i]->Access_MESI(i,addr,op.c_str());
               }
            }
         } //PrRd Ends
      }  //Miss ends
      else{
         updateLRU(line);
         if(line->getFlags()==MODIFIED){
            line->setFlags(MODIFIED);
         }
         else if(line->getFlags()== SHARED){
            if(*op =='r'){
               line->setFlags(SHARED);
            }
            else if(*op == 'w'){
               line->setFlags(MODIFIED);
               BusUpg++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="u";
                  cachearr[i]->Access_MESI(i,addr,op.c_str());
               }
            }
         }
         else if(line->getFlags()==EXCLUSIVE){
            if(*op =='r'){
               line->setFlags(EXCLUSIVE);
            }
            else if(*op =='w'){
               line->setFlags(MODIFIED);
            }
         }
      }
   }
   else if((*op =='d')||(*op =='x')||(*op =='u')){
      cacheLine* line = findLine(addr);
      if(line == NULL){
         if(*op =='d'|| *op =='x'|| *op =='u'){

         }
      }
      else{
         if(line->getFlags()== MODIFIED){
            if(*op=='d'){
               line->setFlags(SHARED);
               intervention++;
               writeBack(addr);
               flushes++;
            }
            else if(*op =='x'){
               line->invalidate();
               invalidations++;
               writeBack(addr);
               flushes++;
            }
            else if(*op =='u'){

            }
         }
         else if(line->getFlags() == SHARED){
            if(*op =='d'){
               line->setFlags(SHARED);
            }
            else if(*op =='x'){
               line->invalidate();
               invalidations++;
            }
            else if(*op =='u'){
               line->invalidate();
               invalidations++;
            }
         }
         else if(line->getFlags()==EXCLUSIVE){
            if(*op =='d'){
               line->setFlags(SHARED);
               intervention++;
            }
            else if(*op =='x'){
               line->invalidate();
               invalidations++;
            }
            else if(*op =='u'){

            }
         }
      } //HIT ends
   } //Bus transactions end
}

//************************END MESI*****************************//

//************************MESI with filter*********************//

void Cache::Access_MESI_Upg(int p, ulong addr, const char* op)
{
   self=p;
   currentCycle++;
   if(*op == 'w') writes++;
   else if(*op == 'r') reads++;
   
   //checking if block exists in other caches or not
   int check_line=0;
   for(int i=0;i<procs;i++){
      if(i==self){
         continue;
      }
      check_line= cachearr[i]->searchblock(addr);
      if(check_line==1){
         break;
      }
   }
   
   if ((*op =='r')||(*op =='w')){
      
      cacheLine * check_self=hist_findLine(addr);
      if(check_self!=NULL){
         check_self->invalidate();
      }      
      cacheLine * line = findLine(addr);
      if(line == NULL){
         if(*op == 'w') writeMisses++;
         else readMisses++;
         cacheLine * newline= fillLine(addr);
         if(*op =='w'){
            newline->setFlags(MODIFIED);
            if(check_line == 1){
               cache2cache++;
            }
            BusRdX++;
            for(int i=0;i<procs;i++){
               if(i==self){
                  continue;
               }
               string op="x";
               cachearr[i]->Access_MESI_Upg(i,addr,op.c_str());
            }
         }
         else if(*op =='r'){
            if(check_line==1){
               newline->setFlags(SHARED);
               cache2cache++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="d";
                  cachearr[i]->Access_MESI_Upg(i,addr,op.c_str());
               }
            }
            else if(check_line==0){
               newline->setFlags(EXCLUSIVE);
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="d";
                  cachearr[i]->Access_MESI_Upg(i,addr,op.c_str());
               }
            }
         } //PrRd Ends
      }  //Miss ends
      else{
         updateLRU(line);
         if(line->getFlags()==MODIFIED){
            line->setFlags(MODIFIED);
         }
         else if(line->getFlags()== SHARED){
            if(*op =='r'){
               line->setFlags(SHARED);
            }
            else if(*op == 'w'){
               line->setFlags(MODIFIED);
               BusUpg++;
               for(int i=0;i<procs;i++){
                  if(i==self){
                     continue;
                  }
                  string op ="u";
                  cachearr[i]->Access_MESI_Upg(i,addr,op.c_str());
               }
            }
         }
         else if(line->getFlags()==EXCLUSIVE){
            if(*op =='r'){
               line->setFlags(EXCLUSIVE);
            }
            else if(*op =='w'){
               line->setFlags(MODIFIED);
            }
         }
      }
   }
   ///////////////////////////////////////////////////////////

   else if((*op =='d')||(*op =='x')||(*op =='u')){
      cacheLine * check_hist;
      check_hist=hist_findLine(addr);
      if(check_hist!=NULL){
         hist_updateLRU(check_hist);
         filtered++;
      }
      else{
         cacheLine* line = findLine(addr);
         if(line == NULL){
            if(*op =='d'|| *op =='x'|| *op =='u'){
               hist_fillLine(addr);
               wasted++;
            }
         }
         else{
            useful++;
            if(line->getFlags()== MODIFIED){
               if(*op=='d'){
                  line->setFlags(SHARED);
                  intervention++;
                  writeBack(addr);
                  flushes++;
               }
               else if(*op =='x'){
                  //hist_updateLRU(check_hist);
                  line->invalidate();
                  hist_fillLine(addr);
                  //hist_updateLRU(other_line);
                  invalidations++; 
                  writeBack(addr);
                  flushes++;
               }
               else if(*op =='u'){
   
               }
            }
            else if(line->getFlags() == SHARED){
               if(*op =='d'){
                  line->setFlags(SHARED);
               }
               else if(*op =='x'){

                  //hist_updateLRU(other_line);
                  //hist_updateLRU(other_lne);
                  line->invalidate();
                  hist_fillLine(addr);                  
                  invalidations++;
               }
               else if(*op =='u'){
                  line->invalidate();
                  hist_fillLine(addr);
                  //hist_updateLRU(other_line);
                  //hist_updateLRU(check_hist);
                  invalidations++;
               }
            }
            else if(line->getFlags()==EXCLUSIVE){
               if(*op =='d'){
                  line->setFlags(SHARED);
                  intervention++;
               }
               else if(*op =='x'){
                  line->invalidate();
                  hist_fillLine(addr);
                  //hist_updateLRU(other_line);               
                  //hist_updateLRU(check_hist);
                  invalidations++;
               }
               else if(*op =='u'){
   
               }
            }
         } //HIT ends
      } //Bus transactions end
   }
}

//***********************END MESI with filter******************//


//************************Cache Operations*********************//

int Cache::searchblock(ulong addr){
   int found=0;
   ulong tag;
   ulong i;
   tag = calcTag(addr);
   i= calcIndex(addr);
   for(ulong j=0;j<assoc;j++){
      if(cache[i][j].isValid()){
         if(cache[i][j].getTag()==tag){
            found=1;
            break;
         }
      }
   }
   return found;
}


/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
   if(cache[i][j].isValid()) {
      if(cache[i][j].getTag() == tag)
      {
         pos = j; 
         break; 
      }
   }
   if(pos == assoc) {
      return NULL;
   }
   else {
      return &(cache[i][pos]); 
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*///
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getFlags() == MODIFIED) {
      writeBack(addr);
   }
   
   //printf("2\n");
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

///////////////////////

int Cache::hist_searchblock(ulong addr){
   int found=0;
   ulong tag;
   ulong i;
   tag = calcTag(addr);
   i= calcIndex(addr);
   for(ulong j=0;j<hist_assoc;j++){
      if(hist_filter[i][j].isValid()){
         if(hist_filter[i][j].getTag()==tag){
            found=1;
            break;
         }
      }
   }
   return found;
}

cacheLine * Cache::hist_findLine(ulong addr)
{
   ulong i, tag, pos;
   
   pos = hist_assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(ulong j=0;j<hist_assoc;j++){
      if(hist_filter[i][j].isValid()) {
         if(hist_filter[i][j].getTag() == tag)
         {
            pos = j;
            break; 
         }
      }
   }
   if(pos == 1) {
      return NULL;
   }
   else {
      return &(hist_filter[i][pos]); 
   }
}

void Cache::hist_updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

cacheLine * Cache::hist_getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = hist_assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<hist_assoc;j++)
   {
      if(hist_filter[i][j].isValid() == 0) { 
         return &(hist_filter[i][j]); 
      }   
   }

   for(j=0;j<hist_assoc;j++)
   {
      if(hist_filter[i][j].getSeq() <= min) { 
         victim = j; 
         min = hist_filter[i][j].getSeq();}
   } 

   assert(victim != hist_assoc);
   
   return &(hist_filter[i][victim]);
}

cacheLine *Cache::hist_findLineToReplace(ulong addr)
{
   cacheLine * victim = hist_getLRU(addr);
   hist_updateLRU(victim);
  
   return (victim);
}

cacheLine *Cache::hist_fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = hist_findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getFlags() == MODIFIED) {
      writeBack(addr);
   }
   
   //printf("2\n");
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats(ulong m,int proc)
{ 
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
   
   cout<<"01. number of reads                   :\t"<<reads<<'\n';
   cout<<"02. number of read misses             :\t"<<readMisses<<'\n';
   cout<<"03. number of writes                  :\t"<<writes<<"\n";
   cout<<"04. number of write misses            :\t"<<writeMisses<<"\n";  
   float miss_rate = (float) (100 * ((float)(readMisses + writeMisses)/(float)(reads+writes)));
   if (miss_rate >= 10){
      cout<<"05. total miss rate                 :  \t"<<setprecision(4) <<miss_rate<<"%\n";
   }
   else if(miss_rate < 10){
      cout<<"05. total miss rate                  : \t"<<setprecision(3) <<miss_rate<<"%\n";
   }
   cout<<"06. number of write backs             :\t"<<writeBacks<<"\n";
   cout<<"07. number of cache-to-cache transfers:\t"<<cache2cache<<"\n";
   cout<<"08. number of memory transactions     :\t"<<m<<"\n";
   cout<<"09. number of interventions           :\t"<<intervention<<"\n";
   cout<<"10. number of invalidations          :\t"<<invalidations<<"\n";
   cout<<"11. number of flushes                :\t"<<flushes<<"\n";
   cout<<"12. number of BusRdX                 :\t"<<BusRdX<<"\n";
   cout<<"13. number of BusUpgr                :\t"<<BusUpg<<"\n";
   if(proc == 3){
      cout<<"14. number of useful snoops          :\t"<<useful<<"\n";
      cout<<"15. number of wasted snoops          :\t"<<wasted<<"\n";
      cout<<"16. number of filtered snoops        :\t"<<filtered<<"\n";
   } 
}
