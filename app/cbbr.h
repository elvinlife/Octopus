#include <udt.h>#include <ccc.h>
#include <cstdio>
#include "common.h"

enum BBRState {
    Startup,
    Drain,
    ProbeBW,
    ProbeRTT
};

struct Filter
{
    const int window_len_;
    int first_round_;
    int last_round_;    // inclusive
    double *value_array_ = NULL;
    
    Filter( int window_len, float init_value )
        : window_len_( window_len ),
        first_round_( 0 ),
        last_round_( 0 ),
        value_array_( new double[window_len+1] ) {
            value_array_[0] = init_value;
        }
    
    ~Filter() { delete value_array_; }
    
    double update( int round, double value )
    {
        double max = -1;
        // reset those skipped rounds
        for (int i = last_round_ + 1; i < round; ++i)
            value_array_[ i % window_len_ ] = -1;

        if ( value > value_array_[ round % window_len_ ] ) {
            value_array_[ round % window_len_ ] = value;
        }

        last_round_ = round;
        if (first_round_ + window_len_ <= last_round_)
            first_round_ = last_round_ - window_len_ + 1;
        for (int i = first_round_; i <= last_round_; ++i)
            if (max < value_array_[i % window_len_])
                max = value_array_[i % window_len_];
        return max;
    }
};

class CBBR: public CCC
{
    public:
        using CCC::CCC;

        CBBR() 
            :btl_bw_ ( getThroughput(BBRMinPipeCwnd, BBRInitRTT) ),
            rt_prop_( RTpropFilterLen ),
            pacing_gain_( 1 ),
            cwnd_gain_( 1 ),
            btl_bw_filter_( BtlBWFilterLen, btl_bw_ ), 
            rtprop_stamp_( CTimer::getTime() ),
            probe_rtt_done_stamp_( 0 ),
            probe_rtt_round_done_( false ),
            prior_cwnd_( 0 ),
            packet_conservation_( false ),
            idle_restart_( false ) {
                initRoundCounting();
                initFullPipe();
                initPacingRate();
                enterStartup();
                fprintf(stderr, "bbr_init: rate: %f, cwnd: %f, btl_bw: %f, rt_prop: %ld\n"\
                        , pacing_rate_, m_dCWndSize, btl_bw_, rt_prop_);
            }

        inline void initRoundCounting()
        {
            cumu_delivered_ = 0;
            next_round_delivered_ = 0;
            round_start_ = false;
            round_count_ = 0;
        }

        inline void initFullPipe()
        {
            filled_pipe_ = false;
            full_bw_ = 0;
            full_bw_count_ = 0;
        }

        inline void initPacingRate()
        {
            pacing_rate_ = pacing_gain_ * getThroughput(BBRMinPipeCwnd, BBRInitRTT);
        }

        virtual void onAck ( Block* block, const RateSample* rs ) override
        {
            updateModelAndState( block, rs );
            updateControlParameters();
        }

        virtual void onTimeout () override
        {
            
        }

    protected:
        void updateModelAndState( Block* block, const RateSample* rs)
        {
            updateBtlBw( block, rs );
            checkCyclePhase( rs );
            checkFullPipe( rs );
            checkDrain( rs );
            updateRTprop( block );
            checkProbeRTT( rs );
        }

        void updateBtlBw( Block* block, const RateSample* rs)
        {
            updateRound( block, rs );
            // ratesample hasn't started yet
            if ( rs->deliveryRate() == 0 )
                return;
            if ( rs->deliveryRate() >= btl_bw_
                    || !rs->isAppLimited() ) {
                btl_bw_ = btl_bw_filter_.update(
                        round_count_,
                        rs->deliveryRate() );
            }
        }

        void updateRound( Block* block, const RateSample* rs)
        {
            cumu_delivered_ = rs->cumuDelivered();
            if (block->delivered_ >= next_round_delivered_) {
                next_round_delivered_ = cumu_delivered_;
                round_count_ += 1;
                round_start_ = true;
            }
            else {
                round_start_ = false;
            }
        }

        void checkCyclePhase( const RateSample* rs)
        {
            if ( state_ == ProbeBW && isNextCyclePhase(rs) ) {
                advanceCyclePhase();
            }
        }

        bool isNextCyclePhase( const RateSample* rs)
        {
            bool is_next_cycle = ( CTimer::getTime() - cycle_stamp_ ) > rt_prop_;
            if (pacing_gain_ == 1.0)
                return is_next_cycle;
            if (pacing_gain_ > 1.0) {
                bool ret = is_next_cycle &&
                    ( rs->packetLost() || 
                      (rs->pktsInFlight() - 1) >= getBDP(pacing_gain_) ); 
                return ret;
            }
            else {
                return is_next_cycle ||
                    (rs->pktsInFlight() - 1) <= getBDP(1);
            }
        }

        void advanceCyclePhase()
        {
            cycle_stamp_ = CTimer::getTime();
            cycle_index_ = ( cycle_index_ + 1 ) % BBRGainCycleLen;
            pacing_gain_ = PacingGainCycle[ cycle_index_ ];
        }

        void handleRestartFromIdle( const RateSample* rs )
        {
            if ( rs->pktsInFlight() == 0 && rs->isAppLimited() ) {
                idle_restart_ = true;
                if (state_ == ProbeBW)
                    setPacingRateWithGain(1);
            }
        }

        void checkFullPipe( const RateSample* rs)
        {
            if ( filled_pipe_ || !round_start_ || rs->isAppLimited() )
                return;
            if ( btl_bw_ >= full_bw_ * 1.25 ) {
                full_bw_ = btl_bw_;
                full_bw_count_ = 0;
                return;
            }
            full_bw_count_ += 1;
            if ( full_bw_count_ >= 3 ) {
                filled_pipe_ = true;
            }
        }

        void checkDrain( const RateSample* rs )
        {
            if ( state_ == Startup && filled_pipe_ )
                enterDrain();
            if ( state_ == Drain && rs->pktsInFlight() <= getBDP(1.0) )
                enterProbeBW();
        }

        void updateRTprop(Block* block)
        {
            uint64_t rtt = CTimer::getTime() - block->sent_ts_;
            rtprop_expired_ = CTimer::getTime() > 
                (rtprop_stamp_ + RTpropFilterLen);
            if ( rtt >= 0 &&
                    (rtt <= rt_prop_ || rtprop_expired_) ) {
                fprintf(stderr, "bbr_update_rtt, rt_prop: %ld seq:%d\n", 
                        rtt, block->seq_);
                rt_prop_ = rtt;
                rtprop_stamp_ = CTimer::getTime();
            }
        }

        void checkProbeRTT( const RateSample* rs )
        {
            if ( state_ != ProbeRTT &&
                    rtprop_expired_ &&
                    !idle_restart_ ) {
                enterProbeRTT();
                probe_rtt_done_stamp_ = 0;
            }
            if ( state_ == ProbeRTT ) {
                handleProbeRTT( rs );
            }
            idle_restart_ = false;
        }

        void handleProbeRTT( const RateSample* rs )
        {
            if (probe_rtt_done_stamp_ == 0 &&
                    rs->pktsInFlight() <= BBRMinPipeCwnd) {
                probe_rtt_done_stamp_ = CTimer::getTime() + ProbeRTTDuration;
                probe_rtt_round_done_ = false;
                next_round_delivered_ = cumu_delivered_;
            }
            else if ( probe_rtt_done_stamp_ != 0 ) {
                if ( round_start_ ) {
                    probe_rtt_round_done_ = true;
                }
                if ( probe_rtt_round_done_ &&
                        CTimer::getTime() > probe_rtt_done_stamp_ ) {
                    rtprop_stamp_ = CTimer::getTime();
                    // exit probertt
                    if (filled_pipe_)
                        enterProbeBW();
                    else
                        enterStartup();
                }
            }
        }

        void updateControlParameters()
        {
            // set pacing rate
            setPacingRateWithGain( pacing_gain_ );
            m_dPktSndPeriod = (PacketMTU * 8.0) / pacing_rate_;
            // set cwnd
            setCwnd();
            fprintf(stderr, "bbr_status: rate: %f, cwnd: %f, btl_bw: %f, rt_prop: %ld, pacing_gain_: %f\n"\
                    , pacing_rate_, m_dCWndSize, btl_bw_, rt_prop_, pacing_gain_);
        }

        void setPacingRateWithGain( double gain )
        {
            double rate = btl_bw_ * gain;
            if ( filled_pipe_ || rate > pacing_rate_ ) {
                pacing_rate_ = rate;
            } 
        }

        void setCwnd()
        {
            updateTargetCwnd();
            setCwndForProbeRTT();
        }

        void updateTargetCwnd()
        {
            m_dCWndSize = getBDP( cwnd_gain_ );
        }

        void setCwndForProbeRTT()
        {
            if ( state_ == ProbeRTT ) {
                m_dCWndSize = (m_dCWndSize >= BBRMinPipeCwnd)
                    ? BBRMinPipeCwnd : m_dCWndSize;
            }
        }

        inline double getBDP(double gain)
        {
            double estimated_bdp = btl_bw_ * rt_prop_ / (8 * PacketMTU);
            return estimated_bdp * gain;
        }

        inline double getThroughput(double cwnd, int time)
        {
            return cwnd * 8 * PacketMTU / time;
        }

        inline void enterStartup()
        {
            fprintf(stderr, "enterStartup\n");
            state_ = Startup;
            pacing_gain_ = BBRHighGain;
            cwnd_gain_ = BBRHighGain;
        }

        inline void enterDrain()
        {
            fprintf(stderr, "enterDrain\n");
            state_ = Drain;
            pacing_gain_ = 1 / BBRHighGain;
            cwnd_gain_ = 1;
        }

        inline void enterProbeBW()
        {
            fprintf(stderr, "enterProbeBW\n");
            state_ = ProbeBW;
            pacing_gain_ = 1;
            cwnd_gain_ = 1;
            cycle_index_ = BBRGainCycleLen - 1;
            advanceCyclePhase();
        }

        inline void enterProbeRTT()
        {
            fprintf(stderr, "enterProbeRTT\n");
            state_ = ProbeRTT;
            pacing_gain_ = 1;
            cwnd_gain_ = 1;
        }

        static constexpr double BBRHighGain = 2.89;
        static const int BBRGainCycleLen    = 8;
        static const int BtlBWFilterLen     = 10;

        static const uint64_t RTpropFilterLen   = 10000000;
        static const uint64_t ProbeRTTDuration  = 200000; 
        static const int BBRMinPipeCwnd         = 4;
        static const int BBRInitRTT             = 100000;

        static const int PacketMTU = 1500;

        // important states
        double PacingGainCycle[8]   = {1.25, 0.75, 1, 1, 1, 1, 1, 1};
        BBRState    state_; 
        double       btl_bw_;            // unit: Mbps
        uint64_t     rt_prop_;           // unit: us
        double       pacing_gain_;
        double       cwnd_gain_;
        double       pacing_rate_;
        Filter       btl_bw_filter_;

        // round related
        uint64_t    cumu_delivered_;
        uint64_t    next_round_delivered_;
        int         round_count_; 
        bool        round_start_;

        // Startup/Drain related
        bool        filled_pipe_;
        double       full_bw_;
        int         full_bw_count_;

        // ProbeBW related
        int         cycle_index_;
        uint64_t    cycle_stamp_;

        // update rtt related
        bool        rtprop_expired_;
        uint64_t    rtprop_stamp_;
        uint64_t    probe_rtt_done_stamp_;
        bool        probe_rtt_round_done_;
        double      prior_cwnd_;

        // unclear
        bool        packet_conservation_;
        bool        idle_restart_;
};
