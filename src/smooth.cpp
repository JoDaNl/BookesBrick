//
// smooth
//

#include <Arduino.h>
#include "smooth.h"


static int compare_int(const void* a, const void* b)
{
  int a_val = *((long*)a);
  int b_val = *((long*)b);
  
  if (a_val == b_val) 
  {
    return 0;
  }
  else if (a_val < b_val)
  {
    return -1;
  }
  else
  {
    return 1;
  }
};



// constructor
Smooth::Smooth()
{
  _sample_count = 0;
  _state = 0; // 0 = accumulating data

  _max_deviation = 100; // TODO : set via class-method

  _filter_points = 7; // TODO : implement window size
  _output_valid = false;
  _value = 0;
};


void Smooth::setFilterPoints(int points)
{
  _filter_points = points;
};

void Smooth::setValue(int value)
{
  int  median;
  long deviation;
  long sum_samples;
  int  num_valid_samples;


  switch (_state)
  {
    case 0:  
    {
      _samples[_sample_count] = value;
      _sample_count++;
      // printf("_sample_count = %d\n", _sample_count);
      
      if (_sample_count == SAMPLE_WINDOW)
      {
        _sample_count = 0;
        _state = 1;
      }      
    }
    break;

    case 1:
    {
      // SHIFT SAMPLES
      for(int i=1; i< SAMPLE_WINDOW; i++)
      {
        _samples[i-1] = _samples[i];
      }
      _samples[SAMPLE_WINDOW-1] = value;

      // SORT SAMPLES
      // copy first
      for(int i=1; i< SAMPLE_WINDOW; i++)
      {
        _samples_sorted[i] = _samples[i];
      }
      qsort(_samples_sorted, SAMPLE_WINDOW, sizeof(int), compare_int);
      
      median = _samples_sorted[SAMPLE_WINDOW/2];
  
      // AVERAGE SAMPLES
      sum_samples = 0;
      num_valid_samples = 0;
      for(int i=0; i< SAMPLE_WINDOW; i++)
      {
        deviation = _samples[i] - median; 
        
        // calculate sum & outlyer detection
        if (abs(deviation) < _max_deviation)
        {
          sum_samples += _samples[i];
          num_valid_samples++;
        }
      } 
    
      if (num_valid_samples > 0)
      {
        _output_valid = true;
        _value = sum_samples / num_valid_samples;
      }
      else
      {
        _output_valid = false;
        _value  = 0;
      }

      // printf("median        = %d\n", median);
      // printf("valid_samples = %d\n", num_valid_samples);
      // printf("_value        = %d\n", _value);
    }
    break;
  }
}


    
bool Smooth::isValid(void)
{
  return _output_valid;
};


int Smooth::getValue(void)
{
  return _value;
}





// end of file

