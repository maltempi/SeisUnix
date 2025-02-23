/* Copyright (c) Colorado School of Mines, 2022.*/
/* All rights reserved.			*/

/* SUBRAID: $Revision: 1.1 $ ; $Date: 2022/03/26 02:48:10 $	*/

#include "su.h"
#include "segy.h"
#include "headcase.h"

/*********************** self documentation **********************/
char *sdoc[] = {
"                                                                       ",
" SUBRAID - Braid traces from files together and output.                ",
"           (combine multiple ordered inputs into 1 ordered output)     ",
"                                                                       ",
" subraid in1=filea.su in2=fileb.su in3=...   key=cdp >stdout           ",
"                                                                       ",
"   inMM= An input file name. Where MM is 1 or 2 digits (0 to 99).      ",
"         Therefore 100 files can be input in one setup.                ",
"         Examples: in0= in1= in33= in99=                               ",
"         There must be at least 2 input files.                         ",
"                                                                       ",
"   key=  Keys that indicate the order of the traces in every input.    ",
"         Traces must be in this order. Order is checked on-the-fly     ",
"         and an error-halt will occur if input trace order is wrong.   ",
"         This parameter can be a list. For instance:  key=cdp,offset   ",
"         means the traces are in offset order within cdps.             ",
"         A negative sign in front of the name means decreasing order   ",
"         for the values of that key.                                   ",
"         Note: If the key= values are equal, the traces from the       ",
"               input file with lower MM number are output first.       ",
" 									",
"  trac=1 Reset tracl and tracr keys to the output trace count          ",
"         (really only done because the SUSORT program resets them).    ",
"      =0 Do not reset. Copy as-is to output.                           ",
" 									",
" This program can combine multiple cdp ordered input files into one    ",
" cdp ordered output file. It can also combine multiple input files     ",
" of any other order into one output file in same order. This is done   ",
" by a process conceptually similar to braiding hair or braiding a rope.",
"                                                                       ",
" Basically, you tell this program the names of the su keys which       ",
" indicate the input trace order. This program opens all input files    ",
" and outputs whichever trace is lowest. It then reads another trace    ",
" from that input file and again finds lowest trace for all input files.",
" It repeats the process until no traces remain on any input file.      ",
" (For traces with the same key values, the trace from lowest inMM= is  ",
" is output first. All traces are always output - except on error-halt).",
"                                                                       ",
" So, for cdp order, if file1 has 3 traces in cdp 17 and file2 has      ",
" 5 traces in cdp 17, then the output will have 8 traces in cdp 17.     ",
" It does not matter if a cdp only exists in one file, and it does      ",
" not matter if there are gaps in cdp numbers in the files.             ",
"                                                                       ",
" Notes:                                                                ",
"   1. See tests (examples) in src/demos/Utilities/Subraid              ",
"   2. This program only stores 1 trace at a time from each input file. ",
"   3. Trace order is checked on-the-fly to make sure it matches        ",
"      your key= list. If it does not match, error-halt occurs after    ",
"      output of the last trace which IS in the correct order.          ",
"   4. Only trace order is checked. No other checks at all. You can     ",
"      easily cause bad results by inputting files where the traces have", 
"      been procssed differently. Or by processing data in pieces when  ",
"      it should be kept together (surface consistant analysis, etc.).  ",
"   5. For clarify, remember that being in order does not necessarily   ",
"      mean that traces have been through a sort program. This is fine, ",
"      subraid.c does not care how the su key values got into order.    ",
"                                                                       ",
" EXAMPLE USES:                                                         ",
"   1. Imagine that your survey is so large you have trouble sorting it ",
"      to cdp order. Just sort howevermany shot gathers you want into   ",
"      cdp order. Then sort another set of shot gathers into cdp order. ",
"      Keep doing that until you have sorted all your traces.           ",
"      Then use this program to combine the multiple cdp ordered files  ",
"      into one output file. Note for both 2d and 3d, these cdp sorted  ",
"      input files have over-lapping cdps. Some cdps only exist in      ",
"      one of the input files and some cdps in multiple input files.    ",
"   2. You may want to deliberately partition your traces for different ",
"      processing. This can be done by sucleave (and other programs).   ",
"      You can cleave your traces into offset ranges, while retaining   ",
"      cdp order in those offset-ranged files. Then use subraid to      ",
"      bring them back together.                                        ",
"   3. In streamer-cable-marine you may want to partition your shot     ",
"      gathers by cables (or port-starboard shot, cable combinations).  ",
"      This is sometimes called nominal subsurface line processing.     ",
"      This program can help put these kinds of data partitions back    ",
"      together while reducing resource utilization.                    ",
"                                                                       ",
NULL};

/* Author:
 *	Andre Latour.        
 *	Looked at suop2.c code to get started (efopen,fgettr). 
 */
/**************** end self doc ***********************************/

segy *atrace[100];

int compdouble (double *p1, double *p2, int nkeys) ;

int main(int argc, char **argv) {
  cwp_String key[81];       /* for key= names                                */
  int itrac=1;              /* renumber tracl and tracr key values.          */
  int kase[81];             /* for case code of key names (from GetCase)     */
  double ddir[81];          /* direction from key name                       */
  double dprev[81];         /* key= values for previous output trace         */
  int nkeys=0;              /* number of keys                                */
  FILE *fps[100];           /* file pointers	                             */
  int  idf[100];            /* keeps track of file parameter names           */
  cwp_String fname=NULL;    /* a file name                                   */
  double *dvals[100];       /* key= values for each input file               */
  int nfiles=0;             /* number of input files                         */
  int i=0;                  /* general                                       */
  int j=0;                  /* general                                       */
  int ismall=0;             /* which file currently has lowest trace         */
  int ntrac = 0;            /* output trace counter                          */
  cwp_String pname[100] =
        {"in0", "in1", "in2", "in3", "in4", "in5", "in6", "in7", "in8", "in9",
        "in10","in11","in12","in13","in14","in15","in16","in17","in18","in19",
        "in20","in21","in22","in23","in24","in25","in26","in27","in28","in29",
        "in30","in31","in32","in33","in34","in35","in36","in37","in38","in39",
        "in40","in41","in42","in43","in44","in45","in46","in47","in48","in49",
        "in50","in51","in52","in53","in54","in55","in56","in57","in58","in59",
        "in60","in61","in62","in63","in64","in65","in66","in67","in68","in69",
        "in70","in71","in72","in73","in74","in75","in76","in77","in78","in79",
        "in80","in81","in82","in83","in84","in85","in86","in87","in88","in89",
        "in90","in91","in92","in93","in94","in95","in96","in97","in98","in99"};

/* hook up getpar */

  initargs(argc, argv);
  requestdoc(1);                    

/* Get key case numbers from GetCase. Also set ddir when reverse order.      */

  nkeys = countparval("key"); 
  if(nkeys<1) err("key= list must have at least 1 name");

  getparstringarray("key",key);
  for (j=0; j<nkeys; j++) { 
    ddir[j] = 1;
    if(key[j][0]=='+') kase[j] = GetCase(key[j]+1);
    else if(key[j][0]=='-') {
      kase[j] = GetCase(key[j]+1);
      ddir[j] = -1;
    }
    else kase[j] = GetCase(key[j]);
    if(kase[j] < 1) err("key= list contains unknown name (%s)",key[j]);
  
    dprev[j] = -1.e99;  /* It may seem ddir should be multiplied here. No. */

  }
  
  if (!getparint("trac", &itrac)) itrac = 1;
  if(itrac>1 || itrac<0) err("error: trac= is out of range.");

/* Open files, allocate, read first trace of each file, get key values.      */

  for (i=0; i<100; i++) {
    if (countparname(pname[i])>0) {
      getparstring(pname[i], &fname);
      fps[nfiles] = efopen(fname, "r");
      atrace[nfiles] = (segy *)ealloc1(sizeof(segy),sizeof(char));
      if(!fgettr(fps[nfiles], atrace[nfiles])) {
        err("could not read first trace in file input by %s=",pname[idf[nfiles]]);
      }
      dvals[nfiles] = ealloc1double(nkeys);
      for (j=0; j<nkeys; j++) 
        dvals[nfiles][j] = ddir[j] * fromhead(*atrace[nfiles],kase[j]);
      idf[nfiles] = i;
      nfiles++;
    }
  }

  checkpars();

  if(nfiles<2) err("At least 2 input files are needed.");

/* Perform the main loop.                                                    */

  while (nfiles) {

/* Which input file has the lowest trace?                                    */

    ismall = 0;
    for (i=1; i<nfiles; i++) {
      if(compdouble (dvals[i], dvals[ismall], nkeys) < 0) ismall = i;
    }

/* Was previous output trace <=  (easier than checking the set of inputs).   */

    if(compdouble (dprev, dvals[ismall], nkeys) > 0) {
      err("traces out-of-order in file input by %s=",pname[idf[ismall]]); 
    }
    for (j=0; j<nkeys; j++) dprev[j] = dvals[ismall][j];

/* Put trace and try to get another trace from same input file.              */

    if(itrac>0) {
      ntrac++;
      atrace[ismall]->tracl = ntrac;
      atrace[ismall]->tracr = ntrac;
    }

    puttr(atrace[ismall]);

    if(fgettr(fps[ismall], atrace[ismall])) {
      for (j=0; j<nkeys; j++) 
        dvals[ismall][j] = ddir[j] * fromhead(*atrace[ismall],kase[j]);
    }
    else { /* no more traces on that input file, eliminate it from loops.    */
      efclose(fps[ismall]);
      for (i=ismall+1; i<nfiles; i++) {
        for (j=0; j<nkeys; j++) dvals[i-1][j] = dvals[i][j];
        fps[i-1] = fps[i];
        idf[i-1] = idf[i];
        memcpy(atrace[i-1],atrace[i],sizeof(segy));
      }
      nfiles--;
    }

  } /* end of  while (nfiles) { */

  return(CWP_Exit());
}

/*---------------------------------------------------------------------------*/
/* Specify compare function. Note that nkeys=0 returns 0.                    */

int compdouble (double *p1, double *p2, int nkeys) {

  int n = 0;

  for(n=0; n<nkeys; n++) {  
    if(p1[n] < p2[n]) return (-1);
    if(p1[n] > p2[n]) return (1); 
  }

  return (0); 

}
