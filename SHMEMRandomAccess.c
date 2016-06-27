/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; -*- */

/*
 * This code has been contributed by the DARPA HPCS program.  Contact
 * David Koester <dkoester@mitre.org> or Bob Lucas <rflucas@isi.edu>
 * if you have questions.
 *
 *
 * GUPS (Giga UPdates per Second) is a measurement that profiles the memory
 * architecture of a system and is a measure of performance similar to MFLOPS.
 * The HPCS HPCchallenge RandomAccess benchmark is intended to exercise the
 * GUPS capability of a system, much like the LINPACK benchmark is intended to
 * exercise the MFLOPS capability of a computer.  In each case, we would
 * expect these benchmarks to achieve close to the "peak" capability of the
 * memory system. The extent of the similarities between RandomAccess and
 * LINPACK are limited to both benchmarks attempting to calculate a peak system
 * capability.
 *
 * GUPS is calculated by identifying the number of memory locations that can be
 * randomly updated in one second, divided by 1 billion (1e9). The term "randomly"
 * means that there is little relationship between one address to be updated and
 * the next, except that they occur in the space of one half the total system
 * memory.  An update is a read-modify-write operation on a table of 64-bit words.
 * An address is generated, the value at that address read from memory, modified
 * by an integer operation (add, and, or, xor) with a literal value, and that
 * new value is written back to memory.
 *
 * We are interested in knowing the GUPS performance of both entire systems and
 * system subcomponents --- e.g., the GUPS rating of a distributed memory
 * multiprocessor the GUPS rating of an SMP node, and the GUPS rating of a
 * single processor.  While there is typically a scaling of FLOPS with processor
 * count, a similar phenomenon may not always occur for GUPS.
 *
 * Select the memory size to be the power of two such that 2^n <= 1/2 of the
 * total memory.  Each CPU operates on its own address stream, and the single
 * table may be distributed among nodes. The distribution of memory to nodes
 * is left to the implementer.  A uniform data distribution may help balance
 * the workload, while non-uniform data distributions may simplify the
 * calculations that identify processor location by eliminating the requirement
 * for integer divides. A small (less than 1%) percentage of missed updates
 * are permitted.
 *
 * When implementing a benchmark that measures GUPS on a distributed memory
 * multiprocessor system, it may be required to define constraints as to how
 * far in the random address stream each node is permitted to "look ahead".
 * Likewise, it may be required to define a constraint as to the number of
 * update messages that can be stored before processing to permit multi-level
 * parallelism for those systems that support such a paradigm.  The limits on
 * "look ahead" and "stored updates" are being implemented to assure that the
 * benchmark meets the intent to profile memory architecture and not induce
 * significant artificial data locality. For the purpose of measuring GUPS,
 * we will stipulate that each thread is permitted to look ahead no more than
 * 1024 random address stream samples with the same number of update messages
 * stored before processing.
 *
 * The supplied MPI-1 code generates the input stream {A} on all processors
 * and the global table has been distributed as uniformly as possible to
 * balance the workload and minimize any Amdahl fraction.  This code does not
 * exploit "look-ahead".  Addresses are sent to the appropriate processor
 * where the table entry resides as soon as each address is calculated.
 * Updates are performed as addresses are received.  Each message is limited
 * to a single 64 bit long integer containing element ai from {A}.
 * Local offsets for T[ ] are extracted by the destination processor.
 *
 * If the number of processors is equal to a power of two, then the global
 * table can be distributed equally over the processors.  In addition, the
 * processor number can be determined from that portion of the input stream
 * that identifies the address into the global table by masking off log2(p)
 * bits in the address.
 *
 * If the number of processors is not equal to a power of two, then the global
 * table cannot be equally distributed between processors.  In the MPI-1
 * implementation provided, there has been an attempt to minimize the differences
 * in workloads and the largest difference in elements of T[ ] is one.  The
 * number of values in the input stream generated by each processor will be
 * related to the number of global table entries on each processor.
 *
 * The MPI-1 version of RandomAccess treats the potential instance where the
 * number of processors is a power of two as a special case, because of the
 * significant simplifications possible because processor location and local
 * offset can be determined by applying masks to the input stream values.
 * The non power of two case uses an integer division to determine the processor
 * location.  The integer division will be more costly in terms of machine
 * cycles to perform than the bit masking operations
 *
 * For additional information on the GUPS metric, the HPCchallenge RandomAccess
 * Benchmark,and the rules to run RandomAccess or modify it to optimize
 * performance -- see http://icl.cs.utk.edu/hpcc/
 *
 */

/* Jan 2005
 *
 * This code has been modified to allow local bucket sorting of updates.
 * The total maximum number of updates in the local buckets of a process
 * is currently defined in "RandomAccess.h" as MAX_TOTAL_PENDING_UPDATES.
 * When the total maximum number of updates is reached, the process selects
 * the bucket (or destination process) with the largest number of
 * updates and sends out all the updates in that bucket. See buckets.c
 * for details about the buckets' implementation.
 *
 * This code also supports posting multiple MPI receive descriptors (based
 * on a contribution by David Addison).
 *
 * In addition, this implementation provides an option for limiting
 * the execution time of the benchmark to a specified time bound
 * (see time_bound.c). The time bound is currently defined in
 * time_bound.h, but it should be a benchmark parameter. By default
 * the benchmark will execute the recommended number of updates,
 * that is, four times the global table size.
 */

/* June 2013
 * Converted the MPI+SHMEM version to SGI/OpenSHMEM version.
 * The call to the MPI functions have been commented but not
 * removed for ease of future experiments with support for
 * hybrid models.
 * Author: Siddhartha Jana
 */
/*
 * OpenSHMEM version:
 *
 * Copyright (c) 2011 - 2015
 *   University of Houston System and UT-Battelle, LLC.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * o Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * o Neither the name of the University of Houston System,
 *   UT-Battelle, LLC. nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <hpcc.h>
#include <stdio.h>
#include "RandomAccess.h"
#if defined(USE_MPI3_RMA)
#include <mpi.h>
#else
#include <shmem.h>
#endif
#define MAXTHREADS 256
#define CHUNK    1
#define CHUNKBIG (32*CHUNK)

void
do_abort(char* f)
{
  fprintf(stderr, "%s\n", f);
}

u64Int srcBuf[] = {
  0xb1ffd1da
};
u64Int targetBuf[sizeof(srcBuf) / sizeof(u64Int)];

static s64Int count;
s64Int updates[MAXTHREADS];
static s64Int ran;
#if defined(USE_MPI3_RMA)
MPI_Win count_win;
MPI_Win updates_win;
#endif

// Note: make this function accept comm?
void
Power2NodesRandomAccessUpdate(u64Int logTableSize,
                                 u64Int TableSize,
                                 u64Int LocalTableSize,
                                 u64Int MinLocalTableSize,
                                 u64Int GlobalStartMyProc,
                                 u64Int Top,
                                 int logNumProcs,
                                 int NumProcs,
                                 int Remainder,
                                 int MyProc,
                                 s64Int ProcNumUpdates)
{
  int i,j,k;
  int logTableLocal,ipartner,iterate,niterate;
  int ndata,nkeep,nsend,nrecv,index,nlocalm1;
  int numthrds;
  u64Int datum,procmask;
  u64Int *data,*send;
  void * tstatus;
  int remote_proc, offset;
  u64Int *tb;
  s64Int remotecount;
  int thisPeId;
  int numNodes;
  int count2;

#if defined(USE_MPI3_RMA)
  MPI_Comm_rank(MPI_COMM_WORLD, &thisPeId);
  MPI_Comm_size(MPI_COMM_WORLD, &numNodes);
  MPI_Barrier(MPI_COMM_WORLD);
#else
  thisPeId = shmem_my_pe();
  numNodes = shmem_n_pes();
  shmem_barrier_all();
#endif

  /* setup: should not really be part of this timed routine */
  ran = starts(4*GlobalStartMyProc);
#if defined(USE_MPI3_RMA)
  // create *updates* & *count* window
  MPI_Win_create(&updates, sizeof(s64Int)*MAXTHREADS, sizeof(s64Int), MPI_INFO_NULL, MPI_COMM_WORLD, &updates_win);
  MPI_Win_lock_all(MPI_MODE_NOCHECK, updates_win);

  MPI_Win_create(&count, sizeof(s64Int), sizeof(s64Int), MPI_INFO_NULL, MPI_COMM_WORLD, &count_win);
  MPI_Win_lock_all(MPI_MODE_NOCHECK, count_win); 
  
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  niterate = ProcNumUpdates;
  logTableLocal = logTableSize - logNumProcs;
  nlocalm1 = LocalTableSize - 1;

  for (j = 0; j < MAXTHREADS; j++)
    updates[j] = 0;
#if defined(USE_MPI3_RMA)
  MPI_Win_sync(updates_win);
#endif

  for (iterate = 0; iterate < niterate; iterate++) {
      count = 0;
#if defined(USE_MPI3_RMA)
  MPI_Win_sync(count_win);
#endif
#if defined(USE_MPI3_RMA)
      MPI_Barrier(MPI_COMM_WORLD);
#else
      shmem_barrier_all();
#endif
      ran = (ran << 1) ^ ((s64Int) ran < ZERO64B ? POLY : ZERO64B);
      remote_proc = (ran >> logTableLocal) & (numNodes - 1);

#if defined(USE_MPI3_RMA)
      s64Int one = 1;
      MPI_Fetch_and_op(&one, &remotecount, MPI_S64INT_T, remote_proc, 0, MPI_SUM, count_win);
      MPI_Win_flush_local(remote_proc, count_win);
      MPI_Put(&ran, 1, MPI_S64INT_T, remote_proc, remotecount, 1, MPI_S64INT_T, updates_win);
      MPI_Win_flush(remote_proc, updates_win);
      MPI_Barrier(MPI_COMM_WORLD);
#else
      remotecount = shmem_longlong_fadd(&count, 1, remote_proc);
      shmem_longlong_p(&updates[remotecount], ran, remote_proc);
      shmem_barrier_all();
#endif
      for(i=0;i<count;i++) {
          datum = updates[i];
          index = datum & nlocalm1;
          HPCC_Table[index] ^= datum;
          updates[i] = 0;
      }
#if defined(USE_MPI3_RMA)
  MPI_Win_sync(updates_win);
#endif
  }

#if defined(USE_MPI3_RMA)
  MPI_Win_unlock_all(count_win);
  MPI_Win_free(&count_win);
  MPI_Win_unlock_all(updates_win);
  MPI_Win_free(&updates_win);
  MPI_Barrier(MPI_COMM_WORLD);
#else
  shmem_barrier_all();
#endif
}

HPCC_Params params;

int main(int argc, char **argv)
{
  int myRank, commSize;
  time_t currentTime;
  int provided;


#if defined(USE_MPI3_RMA)
  MPI_Init(&argc, &argv);
#else
  shmem_init();
#endif
  HPCC_SHMEMRandomAccess( &params );
#if defined(USE_MPI3_RMA)
  MPI_Finalize();
#else
  shmem_finalize();
#endif

  return 0;
}

/* Utility routine to start random number generator at Nth step */
s64Int
starts(u64Int n)
{
  /* s64Int i, j; */
  int i, j;
  u64Int m2[64];
  u64Int temp, ran;

  while (n < 0)
    n += PERIOD;
  while (n > PERIOD)
    n -= PERIOD;
  if (n == 0)
    return 0x1;

  temp = 0x1;
  for (i=0; i<64; i++)
    {
      m2[i] = temp;
      temp = (temp << 1) ^ ((s64Int) temp < 0 ? POLY : 0);
      temp = (temp << 1) ^ ((s64Int) temp < 0 ? POLY : 0);
    }

  for (i=62; i>=0; i--)
    if ((n >> i) & 1)
      break;

  ran = 0x2;

  while (i > 0)
    {
      temp = 0;
      for (j=0; j<64; j++)
        if ((ran >> j) & 1)
          temp ^= m2[j];
      ran = temp;
      i -= 1;
      if ((n >> i) & 1)
        ran = (ran << 1) ^ ((s64Int) ran < 0 ? POLY : 0);
    }

  return ran;
}
