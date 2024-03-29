
#ifndef FOR_INTEGER_TEMPLATE_H_
#define FOR_INTEGER_TEMPLATE_H_

#include "leco_bitop.h"
#define INF 0x7f7fffff

namespace Codecset {
template <typename T>
class FOR_int {
 public:
  std::vector<std::pair<int, int>> mul_add_diff_set;
  int blocks;
  int block_size;

  void init(int block, int block_s) {
    blocks = block;
    block_size = block_s;
  }
  uint8_t* encodeArray8_int(const T* data, const size_t length, uint8_t* res,
                            size_t nvalue) {
    uint8_t* out = res;

    T m = data[0];
    T M = data[0];
    for (uint32_t i = 1; i < length; ++i) {
      if (data[i] > M) M = data[i];
      if (data[i] < m) m = data[i];
    }

    std::vector<T> delta;

    for (auto i = 0; i < length; i++) {
      T tmp_val = data[i] - m;
      delta.emplace_back(tmp_val);
    }

    T max_error = M - m;
    uint8_t max_bit = 0;

    if (max_error) {
      max_bit = bits_int_T_arrow(max_error);  // without sign bit
    }
    if (max_bit > sizeof(T) * 8) {
      max_bit = sizeof(T) * 8;
    }

    memcpy(out, &max_bit, sizeof(uint8_t));
    out += sizeof(uint8_t);

    if (max_bit == sizeof(T) * 8) {
      for (auto i = 0; i < length; i++) {
        memcpy(out, &data[i], sizeof(T));
        out += sizeof(T);
      }
      return out;
    }

    memcpy(out, &m, sizeof(m));
    out += sizeof(m);
    memcpy(out, &M, sizeof(M));
    out += sizeof(M);
    if (max_bit) {
      out = write_FOR_int_T_arrow(delta.data(), out, max_bit, length);
    }

    return out;
  }

  T* decodeArray8(const uint8_t* in, const size_t length, T* out, size_t nvalue) {
    T* res = out;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      memcpy(out, tmpin, length * sizeof(T));
      return out;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);

    if (maxerror) {
      read_all_bit_FOR_arrow<T>(tmpin, 0, length, maxerror, base, out);

    } else {
      for (int i = 0; i < length; i++) {
        out[i] = base;
      }
    }

    return out;
  }



  void decodeRange(const uint8_t* in, T* out, const int start_idx, int length) {
    T* res = out;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      memcpy(out, tmpin+start_idx*sizeof(T), sizeof(T) * length);
      return;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);

    if (maxerror) {
      read_range_bit_FOR_arrow<T>(tmpin, start_idx, length, maxerror, base, out);

    } else {
      for (int i = 0; i < length; i++) {
        out[i] = base;
      }
    }
    return;
  }


  T summation(const uint8_t* in, const size_t length, size_t nvalue) {
    T returnvalue = 0;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* castval = reinterpret_cast<const T*>(in);
      for (int i = 0; i < length; i++) {
        returnvalue += castval[i];
      }
      return returnvalue;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);

    if (maxerror) {
      returnvalue = sum_all_bit_FOR_arrow<T>(tmpin, 0, length, maxerror, base);

    } else {
      returnvalue = length * base;
    }

    return returnvalue;
  }

  T summation_range(const uint8_t* in, int start_idx, int end_idx) {
    T returnvalue = 0;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* castval = reinterpret_cast<const T*>(in);
      for (int i = start_idx; i < end_idx + 1; i++) {
        returnvalue += castval[i];
      }
      return returnvalue;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);

    if (maxerror) {
      returnvalue =
          sum_range_bit_FOR<T>(tmpin, start_idx, end_idx - start_idx + 1, maxerror, base);

    } else {
      returnvalue = (end_idx - start_idx + 1) * base;
    }

    return returnvalue;
  }

  int filter_range(const uint8_t* in, const size_t length, T filter, uint32_t* out,
                   size_t nvalue) {
    int block_start = nvalue ;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    int count = 0;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* in_value = reinterpret_cast<const T*>(tmpin);
      for (int i = 0; i < length; i++) {
        if (in_value[i] > filter) {
          *out = block_start + i;
          out++;
          count++;
        }
      }
      return count;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);
    if (max <= filter) {
      // std::cout<<max<<" "<<filter<<std::endl;
      return count;
    }

    if (maxerror) {
      count = filter_read_all_bit_FOR_arrow<T>(tmpin, 0, length, maxerror, base, out,
                                               filter, block_start);
    } else {
      if (base > filter) {
        for (int i = 0; i < length; i++) {
          out[i] = block_start + i;
        }
        count += length;
      }
    }

    return count;
  }

  int filter_range_close(const uint8_t* in, const size_t length, uint32_t* out,
                         size_t nvalue, T filter1, T filter2) {
    int block_start = nvalue ;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    int count = 0;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* in_value = reinterpret_cast<const T*>(tmpin);
      for (int i = 0; i < length; i++) {
        if (in_value[i] > filter1 && in_value[i] < filter2) {
          *out = block_start + i;
          out++;
          count++;
        }
      }
      return count;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);
    if (max <= filter1 || base >= filter2) {
      return count;
    }

    if (maxerror) {
      count = filter_read_all_bit_FOR_close_arrow<T>(tmpin, 0, length, maxerror, base,
                                                     out, block_start, filter1, filter2);
    } else {
      if (base > filter1 && base < filter2) {
        for (int i = 0; i < length; i++) {
          out[i] = block_start + i;
        }
        count += length;
      }
    }

    return count;
  }

  int filter_range_close_mod(const uint8_t* in, const size_t length, uint32_t* out,
                             size_t nvalue, T filter1, T filter2, T base_val) {
    int block_start = nvalue ;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    memcpy(&maxerror, tmpin, 1);
    tmpin++;
    int count = 0;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* in_value = reinterpret_cast<const T*>(tmpin);
      for (int i = 0; i < length; i++) {
        if (in_value[i] % base_val > filter1 && in_value[i] % base_val < filter2) {
          *out = block_start + i;
          out++;
          count++;
        }
      }
      return count;
    }
    T base = 0;
    memcpy(&base, tmpin, sizeof(base));
    tmpin += sizeof(base);
    T max = 0;
    memcpy(&max, tmpin, sizeof(max));
    tmpin += sizeof(max);
    // if(max<=filter1 || base >=filter2 ){
    //     return count;
    // }
    int64_t tmp_filter1 =
        ceil((double)(base - filter1) / (double)base_val) * base_val + filter1;
    int64_t tmp_filter2 =
        ceil((double)(base - filter1) / (double)base_val) * base_val + filter2;
    if (base % base_val < filter2 && base % base_val > filter1) {
      tmp_filter1 -= base_val;
      tmp_filter2 -= base_val;
    }
    while (tmp_filter1 < max) {
      if (maxerror) {
        count += filter_read_all_bit_FOR_close_arrow<T>(
            tmpin, 0, length, maxerror, base, out+count, block_start, tmp_filter1, tmp_filter2);
      } else {
        if (base > tmp_filter1 && base < tmp_filter2) {
          for (int i = 0; i < length; i++) {
            out[i] = block_start + i;
          }
          count += length;
        }
      }
      tmp_filter1 += base_val;
      tmp_filter2 += base_val;
    }

    return count;
  }

  T randomdecodeArray8(const uint8_t* in, int to_find, uint32_t* out, size_t nvalue) {
    const uint8_t* tmpin = in;
    uint8_t maxbits;
    memcpy(&maxbits, tmpin, sizeof(uint8_t));
    tmpin += sizeof(uint8_t);
    if (maxbits == sizeof(T) * 8) {
      T tmp_val = reinterpret_cast<const T*>(tmpin)[to_find];
      return tmp_val;
    }

    T m;
    memcpy(&m, tmpin, sizeof(m));
    tmpin += sizeof(m);

    T M;
    memcpy(&M, tmpin, sizeof(M));
    tmpin += sizeof(M);

    T tmp_val = m;
    if (maxbits) {
      tmp_val += read_FOR_int_arrow<T>(tmpin, maxbits, to_find);
    }
    return tmp_val;
  }
};

}  // namespace Codecset

#endif /* SIMDFASTPFOR_H_ */
