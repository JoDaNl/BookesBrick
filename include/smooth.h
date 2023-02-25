//
// smooth
//

#ifndef __SMOOTH_H__
#define __SMOOTH_H__

#define SAMPLE_WINDOW 7 // must be odd number

class Smooth
{
public:

    Smooth();
    void setFilterPoints(int points);
    void setValue(int value);
    bool isValid(void);
    int getValue(void);

private:
    int _samples[SAMPLE_WINDOW];
    int _samples_sorted[SAMPLE_WINDOW];
    int _sample_count;
    int _state;
    int _max_deviation;

    int _filter_points;
    int _value;
    int _output_valid;
};

#endif