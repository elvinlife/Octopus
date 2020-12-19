#include <cstdio>
#include <cstdlib>

struct Filter
{
    const int window_len_;
    int first_round_;
    int last_round_;    // inclusive
    double *value_array_;// = NULL;
    
    Filter( int window_len, float init_value )
        : window_len_( window_len ),
        first_round_( 0 ),
        last_round_( 0 ),
        value_array_( NULL ) {
            value_array_ = new double[window_len+1];
            value_array_[0] = init_value;
        }
    
    ~Filter() { delete value_array_; }
    
    double update( int round, double value )
    {
        double max = -100;
        // reset those skipped rounds
        if (round < last_round_)
            goto exit;
        // clear the outdated data
        if (first_round_ <= round - window_len_ ) {
            for (int j = first_round_; j <= round - window_len_; ++j) {
                value_array_[ j % window_len_ ] = -100;
            }
            first_round_ = last_round_ - window_len_ + 1;
        }
        if ( value > value_array_[ round % window_len_ ] ) {
            value_array_[ round % window_len_ ] = value;
        }
        last_round_ = round;
exit:
        for (int i = first_round_; i <= last_round_; ++i)
            if (max < value_array_[i % window_len_])
                max = value_array_[i % window_len_];
        return max;
    }
};

int main()
{
    Filter a(10, 0);
    for (int i = 1; i <= 10; i++) {
        double value = a.update( i, i);
        fprintf(stdout, "round: %d, value: %f\n",
                i, value);
    }
    for (int i = 11; i <= 25; i++) {
        double value = a.update( i, -i + 20.0);
        fprintf(stdout, "round: %d, value: %f\n",
                i, value);
    }
}
