//
// smooth
//

#ifndef __SMOOTH_H__
#define __SMOOTH_H__

template <int SAMPLE_WINDOW> class Smooth
{

private:

  int _samples[SAMPLE_WINDOW];
  int _samplesSorted[SAMPLE_WINDOW];
  int _sampleCount;
  int _state;
  int _maxDeviation;
  int _value;
  int _outputValid;

  // compare function required for qsort function
  static int compare_int(const void *a, const void *b)
  {
    int a_val = *((long *)a);
    int b_val = *((long *)b);

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


public:

  // C-tor
  Smooth()
  {
    _sampleCount = 0;
    _state = 0; // 0 = accumulating data

    _maxDeviation = 1;
    _outputValid = false;
    _value = 0;
  };

  // provide a new input-value to smoothing operator
  void setValue(int value)
  {
    int median;
    long deviation;
    long sum_samples;
    int num_valid_samples;

    switch (_state)
    {
    case 0:
    {
      // Consume new samples until _samples buffer is full. Output remains invalid
      _samples[_sampleCount] = value;
      _sampleCount++;
      // printf("_sample_count = %d\n", _sampleCount);

      if (_sampleCount == SAMPLE_WINDOW)
      {
        _sampleCount = 0;
        _state = 1;
      }
    }
    break;

    case 1:
    {
      // Shift new samples into _samples buffer
      for (int i = 1; i < SAMPLE_WINDOW; i++)
      {
        _samples[i - 1] = _samples[i];
      }
      _samples[SAMPLE_WINDOW - 1] = value;

      // Sort all samples into _samplesSorted buffer 
      // ...first create copy
      for (int i = 1; i < SAMPLE_WINDOW; i++)
      {
        _samplesSorted[i] = _samples[i];
      }
      qsort(_samplesSorted, SAMPLE_WINDOW, sizeof(int), compare_int);

      // Get median from sorted samples
      median = _samplesSorted[SAMPLE_WINDOW / 2];

      // Average all samples. skipping outlyers
      sum_samples = 0;
      num_valid_samples = 0;
      for (int i = 0; i < SAMPLE_WINDOW; i++)
      {
        deviation = _samples[i] - median;

        // calculate sum & outlyer detection
        if (abs(deviation) < _maxDeviation)
        {
          sum_samples += _samples[i];
          num_valid_samples++;
        }
      }

      // Compute average of valid samples
      if (num_valid_samples > 0)
      {
        _outputValid = true;
        _value = sum_samples / num_valid_samples;
      }
      else
      {
        _outputValid = false;
        _value = 0;
      }

      // printf("median        = %d\n", median);
      // printf("valid_samples = %d\n", num_valid_samples);
      // printf("_value        = %d\n", _value);
    }
    break;
    }
  }

  void setMaxDeviation(int maxDeviation)
  {
    _maxDeviation = maxDeviation;
  }

  bool isValid(void)
  {
    return _outputValid;
  };

  int getValue(void)
  {
    return _value;
  }

};

#endif