/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"



int main(int argc, char *argv[])
{
    
    ifstream fin;
    FILE * pFile;

    if(argv[1] == NULL){
         printf("input format: ");
         printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
         exit(0);
        }

    ulong cache_size     = atoi(argv[1]);
    ulong cache_assoc    = atoi(argv[2]);
    ulong blk_size       = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    ulong protocol       = atoi(argv[5]); /* 0:MSI 1:MSI BusUpgr 2:MESI 3:MESI Snoop FIlter */
    char *fname        = (char *) malloc(20);
    fname              = argv[6];

//************************print out Simulator Configurations***************************//

	cout << "===== 506 Coherence Simulator configuration =====" << '\n'
	<< "L1_SIZE: " <<  cache_size << '\n'
	<< "L1_ASSOC: "<< cache_assoc << '\n'
	<< "L1_BLOCKSIZE: " << blk_size << '\n'
	<< "NUMBER OF PROCESSORS: " << num_processors << endl;
	if (protocol == 0 )
	   cout << "COHERENCE PROTOCOL: MSI" << endl;
	else if (protocol == 1)
	   cout << "COHERENCE PROTOCOL: MSI Busupgr" << endl;
	else if (protocol == 2)
	   cout << "COHERENCE PROTOCOL: MESI" << endl;
    else if(protocol == 3)
       cout<<"COHERENCE PROTOCOL: MESI Filter BusNOP"<< endl; 
	cout << "TRACE FILE: " << fname << endl;    

    // print out simulator configuration here
    
    // Using pointers so that we can use inheritance */
	Cache **cachesArray = new Cache *[num_processors];
    //Cache **hist_fil= new Cache *[num_processors];

	for(ulong i=0;i<num_processors;i++)
	{
		cachesArray[i] = new Cache(cache_size, cache_assoc, blk_size, num_processors, cachesArray);
        //hist_fil[i] = new Cache(blk_size,num_processors, hist_fil);
	}

    pFile = fopen (fname,"r");
    if(pFile == 0)
    {   
        printf("Trace file problem\n");
        exit(0);
    }
    
    int proc;
    char  op;
    ulong addr;
    const char* op1;


    int line = 1;
    while(fscanf(pFile, "%d %c %lx", &proc, &op, &addr) != EOF)
    {
/*#ifdef _DEBUG
        printf("%d\n", line);
#endif*/
        // propagate request down through memory hierarchy
        // by calling cachesArray[processor#]->Access(...)
        op1= &op;

		//cout << "Access no " << line << endl;
		//cout << "Proc no. is " << proc << " Op is " << op1 << " Address is " << addr << endl;
        //printf("proc no. is %d, op is %c and addr is %lx \n",proc,op,addr);

		if( protocol == 0 )
		    cachesArray[proc]->Access_MSI(proc, (ulong) addr, op1);
        else if( protocol ==1){
            cachesArray[proc]->Access_MSI_Busupg(proc, (ulong) addr, op1);
        } 
        else if( protocol == 2){
            cachesArray[proc]->Access_MESI(proc, (ulong) addr, op1); 
        }
        else if(protocol == 3){
            cachesArray[proc]->Access_MESI_Upg(proc, (ulong)addr, op1);
        }
        line++;
    }

    fclose(pFile);

    //********************************//
    //print out all caches' statistics //
    //********************************//

	for(ulong i=0;i<num_processors;i++)
	{	
		cout << "============ Simulation results (Cache " << i << ") ============" <<  endl;
        ulong m;

		if ( protocol == 0 )
		   m = cachesArray[i]->memtrans + cachesArray[i]->readMisses + cachesArray[i]->writeMisses + cachesArray[i]->writeBacks;
        else if( protocol == 1)
           m= cachesArray[i]->memtrans+ cachesArray[i]->readMisses + cachesArray[i]->writeMisses + cachesArray[i]->writeBacks;
        else if (protocol == 2 || protocol == 3)
           m=  cachesArray[i]->readMisses + cachesArray[i]->writeMisses + cachesArray[i]->writeBacks - cachesArray[i]->cache2cache;  
  

	    cachesArray[i]->printStats(m, protocol);
	}
    
}
