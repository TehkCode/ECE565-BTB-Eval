/*
 * Copyright (c) 2013-2014 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *
 *  Fetch2 receives lines of data from Fetch1, separates them into
 *  instructions and passes them to Decode
 */

#ifndef __CPU_MINOR_FETCH2_HH__
#define __CPU_MINOR_FETCH2_HH__

#include <vector>
// #include <unordered_map>
// #include <unordered_set>


#include "base/named.hh"
#include "cpu/minor/buffers.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/minor/pipe_data.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/BaseMinorCPU.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Minor, minor);
namespace minor
{

/** This stage receives lines of data from Fetch1, separates them into
 *  instructions and passes them to Decode */
class Fetch2 : public Named
{
  protected:
    /** Pointer back to the containing CPU */
    MinorCPU &cpu;

    //std::unordered_map<PCStateBase, std::unordered_set<uint64_t>> btb_map;

    /** Input port carrying lines from Fetch1 */
    Latch<ForwardLineData>::Output inp;

    /** Input port carrying branches from Execute.  This is a snoop of the
     *  data provided to F1. */
    Latch<BranchData>::Output branchInp;

    /** Output port carrying predictions back to Fetch1 */
    Latch<BranchData>::Input predictionOut;

    /** Output port carrying instructions into Decode */
    Latch<ForwardInstData>::Input out;

    /** Interface to reserve space in the next stage */
    std::vector<InputBuffer<ForwardInstData>> &nextStageReserve;

    /** Width of output of this stage/input of next in instructions */
    unsigned int outputWidth;

    /** If true, more than one input word can be processed each cycle if
     *  there is room in the output to contain its processed data */
    bool processMoreThanOneInput;

    /** Branch predictor passed from Python configuration */
    branch_prediction::BPredUnit &branchPredictor;

  public:
    /* Public so that Pipeline can pass it to Fetch1 */
    std::vector<InputBuffer<ForwardLineData>> inputBuffer;

  protected:
    /** Data members after this line are cycle-to-cycle state */

    struct Fetch2ThreadInfo
    {
        Fetch2ThreadInfo() {}

        Fetch2ThreadInfo(const Fetch2ThreadInfo& other) :
            inputIndex(other.inputIndex),
            havePC(other.havePC),
            lastStreamSeqNum(other.lastStreamSeqNum),
            expectedStreamSeqNum(other.expectedStreamSeqNum),
            predictionSeqNum(other.predictionSeqNum),
            blocked(other.blocked)
        {
            set(pc, other.pc);
        }

        /** Index into an incompletely processed input line that instructions
         *  are to be extracted from */
        unsigned int inputIndex = 0;


        /** Remembered program counter value.  Between contiguous lines, this
         *  is just updated with advancePC.  For lines following changes of
         *  stream, a new PC must be loaded and havePC be set.
         *  havePC is needed to accomodate instructions which span across
         *  lines meaning that Fetch2 and the decoder need to remember a PC
         *  value and a partially-offered instruction from the previous line */
        std::unique_ptr<PCStateBase> pc;

        /** PC is currently valid.  Initially false, gets set to true when a
         *  change-of-stream line is received and false again when lines are
         *  discarded for any reason */
        bool havePC = false;

        /** Stream sequence number of the last seen line used to identify
         *  changes of instruction stream */
        InstSeqNum lastStreamSeqNum = InstId::firstStreamSeqNum;

        /** Fetch2 is the source of fetch sequence numbers.  These represent the
         *  sequence that instructions were extracted from fetched lines. */
        InstSeqNum fetchSeqNum = InstId::firstFetchSeqNum;

        /** Stream sequence number remembered from last time the
         *  predictionSeqNum changed.  Lines should only be discarded when their
         *  predictionSeqNums disagree with Fetch2::predictionSeqNum *and* they
         *  are from the same stream that bore that prediction number */
        InstSeqNum expectedStreamSeqNum = InstId::firstStreamSeqNum;

        /** Fetch2 is the source of prediction sequence numbers.  These
         *  represent predicted changes of control flow sources from branch
         *  prediction in Fetch2. */
        InstSeqNum predictionSeqNum = InstId::firstPredictionSeqNum;

        /** Blocked indication for report */
        bool blocked = false;
    };

    std::vector<Fetch2ThreadInfo> fetchInfo;
    ThreadID threadPriority;

    struct Fetch2Stats : public statistics::Group
    {
        Fetch2Stats(MinorCPU *cpu);
        /** Stats */
        statistics::Scalar intInstructions;
        statistics::Scalar fpInstructions;
        statistics::Scalar vecInstructions;
        statistics::Scalar loadInstructions;
        statistics::Scalar storeInstructions;
        statistics::Scalar amoInstructions;
    } stats;

  protected:
    /** Get a piece of data to work on from the inputBuffer, or 0 if there
     *  is no data. */
    const ForwardLineData *getInput(ThreadID tid);

    /** Pop an element off the input buffer, if there are any */
    void popInput(ThreadID tid);

    /** Dump the whole contents of the input buffer.  Useful after a
     *  prediction changes control flow */
    void dumpAllInput(ThreadID tid);

    /** Update local branch prediction structures from feedback from
     *  Execute. */
    void updateBranchPrediction(const BranchData &branch);

    /** Predicts branches for the given instruction.  Updates the
     *  instruction's predicted... fields and also the branch which
     *  carries the prediction to Fetch1 */
    void predictBranch(MinorDynInstPtr inst, BranchData &branch);

    /** Use the current threading policy to determine the next thread to
     *  fetch from. */
    ThreadID getScheduledThread();

  public:
    Fetch2(const std::string &name,
        MinorCPU &cpu_,
        const BaseMinorCPUParams &params,
        Latch<ForwardLineData>::Output inp_,
        Latch<BranchData>::Output branchInp_,
        Latch<BranchData>::Input predictionOut_,
        Latch<ForwardInstData>::Input out_,
        std::vector<InputBuffer<ForwardInstData>> &next_stage_input_buffer);

  public:
    /** Pass on input/buffer data to the output if you can */
    void evaluate();

    void minorTrace() const;


    /** Is this stage drained?  For Fetch2, draining is initiated by
     *  Execute halting Fetch1 causing Fetch2 to naturally drain.
     *  Branch predictions are ignored by Fetch1 during halt */
    bool isDrained();
};

} // namespace minor
} // namespace gem5

#endif /* __CPU_MINOR_FETCH2_HH__ */
