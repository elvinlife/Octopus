#ifndef __UDT_SCB_H__
#define __UDT_SCB_H__

#include "udt.h"
#include "common.h"

struct seginfo {
    seginfo* next_;
    seginfo* prev_;
    int32_t startseq_;
    int32_t endseq_;

    seginfo()
        :next_(NULL), prev_(NULL),
        startseq_(0), endseq_(0) {}
};

class ReassemblyQueue
{
    public:
        static int n_scb_;
        static seginfo* freelist_;
        static seginfo* newseginfo();
        static void deleteseginfo(seginfo*);

        ReassemblyQueue(int32_t rcv_next)
            : total_(0), rcv_next_(rcv_next), head_(NULL), tail_(NULL) {
                n_scb_ += 1;
            };
        
        ~ReassemblyQueue();

        int empty() const {return head_ == NULL; }
        void add( int32_t start, int32_t end );
        void clearto( int32_t seq );
        void clear();
        int32_t nextHole( int32_t seq );
        void dumpList() const;

        int total_;         // # of pkts sacked in rq
        int32_t& rcv_next_; // current cumulative ack, updated by the insertion of new sack areas

    private:
        void fremove(seginfo*);
        void coalesce(seginfo*, seginfo*, seginfo*);
        seginfo* head_;
        seginfo* tail_;
};

class ScoreBoard
{
public:
    ScoreBoard(int ISN)
        : ack_cumu_(ISN), sack_high_(ISN), 
        seq_next_(ISN), rq_(ISN)
    { }

    ~ScoreBoard() {}

    int32_t getNextRetran();
    int update(int32_t ack, int32_t *sack_array);
    void markRetran(int32_t retran_seq);
    void dumpBoard() const;
    void clear();
    bool empty() const { return rq_.empty(); }

private:
    int32_t ack_cumu_;  // current cumulative ack
    int32_t sack_high_; // highest seq seen plus one
    int32_t seq_next_;  // the seq should be retransmitted next
    ReassemblyQueue rq_;
};

#endif
