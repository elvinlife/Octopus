#include <udt.h>
#include <ccc.h>
#include <cstdio>
#include "common.h"
#include <cassert>
#include <string>

enum BBRState {
    Startup,
    Drain,
    ProbeBW,
    ProbeRTT
};

struct Filter
{
private:
    const int       window_capacity_ = 100;
    int             window_len_;
    int    first_round_;
    int    last_round_;    // inclusive
    double          *rate_array_ = NULL;
    
public:
    Filter( int window_len, float init_value )
        : window_len_( window_len ),
        first_round_( 0 ),
        last_round_( 0 ),
        rate_array_( new double[window_capacity_] ) {
            for ( int i = 0; i < window_capacity_; ++i )
                rate_array_[i] = 0;
            rate_array_[0] = init_value;
        }
    
    ~Filter() { delete [] rate_array_; }
    
    double update( int round, double value )
    {
        double max = 0;
        // reset those skipped rounds
        if (round < last_round_)
            goto exit;
        // clear the outdated data
        last_round_ = round;
        if (first_round_ <= round - window_len_ ) {
            for (int j = first_round_; j <= round - window_len_; ++j) {
                rate_array_[ j % window_len_ ] = 0;
            }
            first_round_ = last_round_ - window_len_ + 1;
        }
        if ( value > rate_array_[ round % window_len_ ] ) {
            rate_array_[ round % window_len_ ] = value;
        }
exit:
        for (int i = first_round_; i <= last_round_; ++i)
            if (max < rate_array_[i % window_len_])
                max = rate_array_[i % window_len_];
        if ( first_round_ + window_len_ <= last_round_ ) {
            std::string error_code = "first_round: " + std::to_string(first_round_)
                + " last_round: " + std::to_string(last_round_)
                + " round: " + std::to_string(round)
                + " window_len: " + std::to_string(window_len_);
            throw std::runtime_error( error_code );
        }
        assert( max != 0 );
        return max;
    }

    void clear( float init_value )
    {
        first_round_ = 0;
        last_round_ = 0;
        for ( int i = 0; i < window_len_; ++i ) {
            rate_array_[i] = 0;
        }
        rate_array_[0] = init_value; 
    }

    void setWndLen( int len )
    {
        double tmp_array[ window_capacity_ ];
        for ( int i = 0; i < window_capacity_; ++i ) {
            tmp_array[i] = 0;
        }
        for ( int i = first_round_; i <= last_round_; ++i ) {
            tmp_array[ i % len ] = rate_array_[ i % window_len_ ]; 
        }
        for ( int i = 0; i < window_capacity_; ++i ) {
            rate_array_[i] = tmp_array[i];
        }
        window_len_ = len;
    }

    int getWndLen()
    {
        return window_len_;
    }
};

class CBBR: public CCC
{
    public:
        using CCC::CCC;

        CBBR() 
            :btl_bw_ ( getThroughput(BBRMinPipeCwnd, BBRInitRTT) ),
            rt_prop_( BBRInitRTT ),
            pacing_gain_( 1 ),
            cwnd_gain_( 1 ),
            btl_bw_filter_( BtlBWFilterLen, btl_bw_ ), 
            rtprop_stamp_( CTimer::getTime() ),
            probe_rtt_done_stamp_( 0 ),
            probe_rtt_round_done_( false ),
            prior_cwnd_( 0 ) {
                initRoundCounting();
                initFullPipe();
                initPacingRate();
                enterStartup();
                updateControlParameters();
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
            m_dPktSndPeriod = PacketMTU / pacing_rate_ / Ratio;
        }

        virtual void onAck ( Block* block, const RateSample* rs ) override
        {
            updateModelAndState( block, rs );
            updateControlParameters();
        }

        virtual void onTimeout () override
        {
            round_start_ = false;
            round_count_ = 0;
            next_round_stamp_ = CTimer::getTime(); 
            initFullPipe();
            btl_bw_ = pacing_rate_ = getThroughput(BBRMinPipeCwnd, rt_prop_);
            btl_bw_filter_.clear( btl_bw_ );
            enterStartup();
            updateControlParameters();
        }

    protected:
        void updateModelAndState( Block* block, const RateSample* rs)
        {
            updateRTprop( block );
            updateBtlBw( block, rs );
            updateAckAggregation( rs );
            checkCyclePhase( rs );
            checkFullPipe( rs );
            checkDrain( rs );
            checkProbeRTT( rs );
        }

        void updateBtlBw( Block* block, const RateSample* rs)
        {
            updateRound( block, rs );
            // ratesample hasn't started yet
            float delivery_rate = rs->deliveryRate();
            if ( delivery_rate < getThroughput(BBRMinPipeCwnd, rt_prop_) ) {
                delivery_rate = getThroughput(BBRMinPipeCwnd, rt_prop_);
            }
            if ( rs->deliveryRate() >= btl_bw_
                    || !rs->isAppLimited() ) {
                btl_bw_ = btl_bw_filter_.update(
                        round_count_,
                        delivery_rate );
            }
        }

        void updateAckAggregation( const RateSample* rs )
        {
            if ( rs->cumuDelivered() <= 0 )
                return;
            if ( round_start_ ) {
               // last k-2k (5-10) epoches 
               extra_acked_win_rtts_ = extra_acked_win_rtts_ + 1;
               if ( extra_acked_win_rtts_ >= ExtraAckedMaxRtts ) {
                   extra_acked_win_rtts_ = 0;
                   extra_acked_win_idx_ = extra_acked_win_idx_ ? 0 : 1;
                   extra_acked_[extra_acked_win_idx_] = 0;
               }
            }
            int32_t epoch_us = rs->delivered_mstamp_ - ack_epoch_mstamp_; 
            // time a ratio
            int expected_acked = btl_bw_ * epoch_us * Ratio / PacketMTU;
            if ( expected_acked >= ack_epoch_acked_ ||
                    ( (ack_epoch_acked_ + rs->acked_sacked_) >= AckEpochResetThresh ) ) {
                ack_epoch_acked_ = 0;
                ack_epoch_mstamp_ = rs->delivered_mstamp_;
                expected_acked = 0;
            }
            ack_epoch_acked_ = ack_epoch_acked_ + rs->acked_sacked_;
            if ( ack_epoch_acked_ > 0xFFFFF ) {
                ack_epoch_acked_ = 0xFFFFF;
            }
            int extra_acked = ack_epoch_acked_ - expected_acked;
            extra_acked = ( extra_acked < m_dCWndSize ) ? extra_acked : m_dCWndSize; 
            if( extra_acked > extra_acked_[extra_acked_win_idx_] )
                extra_acked_[extra_acked_win_idx_] = extra_acked;
        }

        int ackAggregationCwnd()
        {
            int aggr_cwnd = 0;
            if ( filled_pipe_ ) {
                int max_aggr_cwnd = btl_bw_ * 100000 * Ratio / PacketMTU;
                aggr_cwnd = extra_acked_[0];
                if ( aggr_cwnd < extra_acked_[1] )
                    aggr_cwnd = extra_acked_[1];
                if ( aggr_cwnd > max_aggr_cwnd ) {
                    aggr_cwnd = max_aggr_cwnd;
                }
            }
            return aggr_cwnd;
        }

        /*
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
        */

        void updateRound( Block* block, const RateSample* rs)
        {
            uint64_t now = CTimer::getTime();
            if (now > next_round_stamp_) {
                round_count_ += ( ( now - next_round_stamp_ ) / rt_prop_ + 1 );
                next_round_stamp_ = now + rt_prop_;
                round_start_ = true;
            }
            else
                round_start_ = false;
        }

        void checkCyclePhase( const RateSample* rs)
        {
            if ( state_ == ProbeBW && isNextCyclePhase(rs) ) {
                advanceCyclePhase( rs );
            }
        }

        bool isNextCyclePhase( const RateSample* rs)
        {
            bool is_next_cycle = ( rs->delivered_mstamp_ - cycle_stamp_ ) > rt_prop_;
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

        void advanceCyclePhase( const RateSample* rs)
        {
            cycle_stamp_ = rs->delivered_mstamp_;
            cycle_index_ = ( cycle_index_ + 1 ) % BBRGainCycleLen;
            pacing_gain_ = PacingGainCycle[ cycle_index_ ];
        }

        void checkFullPipe( const RateSample* rs)
        {
            //if ( filled_pipe_ || !round_start_ || rs->isAppLimited() )
            if ( filled_pipe_ || !round_start_ )
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
            if ( state_ == Startup && 
                    ( filled_pipe_ || rs->packetLost() ) )
                enterDrain();
            if ( state_ == Drain && rs->pktsInFlight() <= getBDP(1.0) )
                enterProbeBW();
        }

        void updateRTprop(Block* block)
        {
            // don't count retransmitted packet to calculate rtt
            if ( block->is_retrans_ )
                return;
            uint64_t rtt = CTimer::getTime() - block->sent_ts_;
            rtprop_expired_ = CTimer::getTime() > 
                (rtprop_stamp_ + RTpropFilterLen);
            if ( rtt >= 0 &&
                    (rtt <= rt_prop_ || rtprop_expired_) ) {
                rt_prop_ = rtt;
                //setRTO( 3 * rt_prop_ );
                rtprop_stamp_ = CTimer::getTime();
                // increase the btl_bw_filter window if rtt is low
                if ( btl_bw_filter_.getWndLen() * rt_prop_ < BtlBWFilterMinWnd ) {
                    btl_bw_filter_.setWndLen( BtlBWFilterMinWnd / rt_prop_ );
                }
                fprintf( stderr, "bbr_update_rtt, rt_prop: %ld seq: %d btl_filter_wnd: %d\n",
                        rtt, block->seq_, btl_bw_filter_.getWndLen() );
            }
        }

        void checkProbeRTT( const RateSample* rs )
        {
            if ( state_ == ProbeBW &&
                    rtprop_expired_ ) {
                enterProbeRTT();
                probe_rtt_done_stamp_ = 0;
            }
            if ( state_ == ProbeRTT ) {
                handleProbeRTT( rs );
            }
        }

        void handleProbeRTT( const RateSample* rs )
        {
            if (probe_rtt_done_stamp_ == 0 &&
                    rs->pktsInFlight() <= BBRMinPipeCwnd) {
                probe_rtt_done_stamp_ = CTimer::getTime() + ProbeRTTDuration;
                probe_rtt_round_done_ = false;
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
            // set Quantum
            setSendQuantum();
            // set cwnd
            setCwnd();
            m_dPktSndPeriod = PacketMTU / pacing_rate_ / Ratio;
            m_dBtlBw = btl_bw_;
            //m_dVideoRate = btl_bw_;
            m_dVideoRate = pacing_rate_ > btl_bw_ ? pacing_rate_ : btl_bw_;
            //m_dVideoRate = pacing_rate_ > 1.25 * btl_bw_ ? pacing_rate_ : 1.25 * btl_bw_;

            fprintf( stderr, "bbr_status rate: %.2f cwnd: %.2f "
                    "videorate: %.2f btl_bw: %.2f "
                    "rt_prop: %ld pacing_gain_: %.2f "
                    "round: %d round_start: %d "
                    "full_bw_cnt: %d ts: %ld ms\n",
                    pacing_rate_,
                    m_dCWndSize,
                    m_dVideoRate,
                    btl_bw_,
                    rt_prop_,
                    pacing_gain_,
                    round_count_,
                    round_start_ ? 1 : 0,
                    full_bw_count_,
                    CTimer::getTime() / 1000
                    );
        }

        void setPacingRateWithGain( double gain )
        {
            double rate = btl_bw_ * gain;
            if ( filled_pipe_ || rate > pacing_rate_ ) {
                pacing_rate_ = rate;
            } 
        }

        void setSendQuantum()
        {
            if ( pacing_rate_ < 12 )
                send_quantum_ = 2;
            else if ( pacing_rate_ < 24 )
                send_quantum_ = 4;
            else
                send_quantum_ = (pacing_rate_ / 6.0) > 43 ? 43 : (pacing_rate_ / 6.0);
        }

        void setCwnd()
        {
            updateTargetCwnd();
            setCwndForProbeRTT();
        }

        void updateTargetCwnd()
        {
            m_dCWndSize = getBDP( cwnd_gain_ ) + ackAggregationCwnd();
            m_dCWndSize += 3 * send_quantum_;
            if ( state_ == ProbeBW && cycle_index_ == 0 )
                m_dCWndSize += 2;
            if ( m_dCWndSize < BBRMinPipeCwnd ) {
                m_dCWndSize = BBRMinPipeCwnd;
            }
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
            double estimated_bdp = btl_bw_ * rt_prop_ * Ratio / PacketMTU;
            return estimated_bdp * gain;
        }

        inline double getThroughput(double cwnd, int time)
        {
            return cwnd * PacketMTU / time / Ratio;
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
            cwnd_gain_ = BBRHighGain;
        }

        inline void enterProbeBW()
        {
            fprintf(stderr, "enterProbeBW\n");
            state_ = ProbeBW;
            cycle_index_ = 0;
            cycle_stamp_ = CTimer::getTime();
            pacing_gain_ = PacingGainCycle[cycle_index_];
            cwnd_gain_ = 2;
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
        static const int BtlBWFilterMinWnd  = 600000;

        //static const uint64_t RTpropFilterLen   = 1000000000;
        static const uint64_t RTpropFilterLen   = 1000000000;
        static const uint64_t ProbeRTTDuration  = 200000; 
        static const int BBRMinPipeCwnd         = 4;
        static const int BBRInitRTT             = 300000;
        static const int PacketMTU              = 1500;
        static const int AckEpochResetThresh    = 1 << 20;
        static const int ExtraAckedMaxRtts      = 5;
        static constexpr double Ratio           = 0.125;

        // important states
        double PacingGainCycle[8]   = {1.25, 0.75, 1, 1, 1, 1, 1, 1};
        BBRState    state_; 
        double      btl_bw_;            // unit: Mbps
        uint64_t    rt_prop_;           // unit: us
        double      pacing_gain_;
        double      cwnd_gain_;
        double      pacing_rate_;       // unit: Mbps
        int         send_quantum_;
        Filter      btl_bw_filter_;

        // round related
        uint64_t    cumu_delivered_;
        uint64_t    next_round_delivered_;
        uint64_t    next_round_stamp_;
        int         round_count_; 
        bool        round_start_;

        // Startup/Drain related
        bool        filled_pipe_;
        double      full_bw_;
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

        // ack aggregation related
        uint64_t    ack_epoch_mstamp_; 
        int         ack_epoch_acked_;
        int         extra_acked_[2];
        int         extra_acked_win_idx_;
        int         extra_acked_win_rtts_;
};
