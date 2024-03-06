
#ifndef DELTA_INTEGER_TEMPLATE_H_
#define DELTA_INTEGER_TEMPLATE_H_

#include "leco_bitop.h"
#define INF 0x7f7fffff

namespace Codecset {
template <typename T>
class Delta_int {
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

    std::vector<T> delta;
    std::vector<bool> signvec;
    T max_error = 0;

    for (auto i = 0; i < length - 1; i++) {
      T tmp_val;
      if (data[i + 1] > data[i]) {
        tmp_val = data[i + 1] - data[i];
        signvec.emplace_back(true);  // means positive
      } else {
        tmp_val = data[i] - data[i + 1];
        signvec.emplace_back(false);  // means negative
      }

      delta.emplace_back(tmp_val);

      if (tmp_val > max_error) {
        max_error = tmp_val;
      }
    }

    uint8_t max_bit = 0;
    if (max_error) {
      max_bit = bits_int_T_arrow(max_error) + 1;
    }

    if (max_bit > sizeof(T) * 8) {
      max_bit = sizeof(T) * 8;
    }
    memcpy(out, &max_bit, sizeof(max_bit));
    out += sizeof(max_bit);
    if (max_bit == sizeof(T) * 8) {
      for (auto i = 0; i < length; i++) {
        memcpy(out, &data[i], sizeof(T));
        out += sizeof(T);
      }
      return out;
    }

    memcpy(out, &data[0], sizeof(T));
    out += sizeof(T);

    if (max_bit) {
      out = write_delta_int_T(delta, signvec, out, max_bit, length);
    }

    return out;
  }

  T summation(const uint8_t* in, const size_t length, size_t nvalue) {
    T returnvalue = summation_range(in, 0, length - 1);
    // [start_idx, end_idx]
    return returnvalue;
  }

  T summation_range(const uint8_t* in, int start_idx, int end_idx) {
    T returnvalue = 0;
    // [start_idx, end_idx]

    const uint8_t* tmpin = in;
    uint8_t maxerror = tmpin[0];
    tmpin++;

    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* castval = reinterpret_cast<const T*>(in);
      for (int i = start_idx; i < end_idx + 1; i++) {
        returnvalue += castval[i];
      }
      return returnvalue;
    }

    T base;
    memcpy(&base, tmpin, sizeof(T));
    tmpin += sizeof(T);
    if (maxerror) {
      returnvalue = sum_all_bit_range_delta<T>(tmpin, 0, start_idx,
                                               end_idx - start_idx + 1, maxerror, base);
    } else {
      returnvalue = base * (end_idx - start_idx + 1);
    }

    return returnvalue;
  }

  T* decodeArray8(const uint8_t* in, const size_t length, T* out, size_t nvalue) {
    T* res = out;
    // start_index + bit + theta0 + theta1 + numbers + delta
    const uint8_t* tmpin = in;
    uint8_t maxerror = tmpin[0];
    tmpin++;
    if (maxerror == 127) {
      T tmp_val;
      memcpy(&tmp_val, tmpin, sizeof(tmp_val));
      res[0] = tmp_val;
      res++;
      return out;
    }
    if (maxerror == 126) {
      T tmp_val;
      memcpy(&tmp_val, tmpin, sizeof(tmp_val));
      res[0] = tmp_val;
      res++;
      memcpy(&tmp_val, tmpin + sizeof(T), sizeof(tmp_val));
      res[0] = tmp_val;
      res++;
      return out;
    }

    T base;
    memcpy(&base, tmpin, sizeof(T));
    tmpin += sizeof(T);
    if (maxerror) {
      res[0] = base;
      read_all_bit_Delta<T>(tmpin, 0, length - 1, maxerror, base, res + 1);
    } else {
      for (int j = 0; j < length; j++) {
        res[j] = base;
      }
    }

    return out;
  }



  void decodeRange(const uint8_t* in, T* out, const int start_idx, int length) {
    T* res = out;

    const uint8_t* tmpin = in;

    uint8_t maxerror = tmpin[0];
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      // out = reinterpret_cast<T*>(tmpin);
      memcpy(out, tmpin+start_idx*sizeof(T), sizeof(T) * length);
      return;
      // read_all_default(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
    }

    T base;
    memcpy(&base, tmpin, sizeof(T));
    tmpin += sizeof(T);

    if (maxerror) {
      if(start_idx==0){
        res[0] = base;
        res++;
        read_range_bit_Delta_arrow<T>(tmpin, start_idx, length-1, maxerror,base, res);
      }
      else{

      // read_all_bit_fix_add<T>(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
      read_range_bit_Delta_arrow<T>(tmpin, start_idx-1, length, maxerror,base, res);
      }

    } else {
      for (int j = 0; j < length; j++) {
        res[j] = base;
      }
    }

    return;
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

    T base;
    memcpy(&base, tmpin, sizeof(T));
    tmpin += sizeof(T);

    T tmp_val = base;
    if (maxbits) {
      tmp_val = read_Delta_int(tmpin, maxbits, to_find, base);
    }
    return tmp_val;
  }

  int filter_range(const uint8_t* in, const size_t length, T filter, uint32_t* out,
                   size_t nvalue) {
    int block_start = nvalue;
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
    T base;
    memcpy(&base, tmpin, sizeof(T));
    tmpin += sizeof(T);

    if (maxerror) {
      if (base > filter) {
        *out = block_start;
        out++;
        count++;
      }
      count += filter_read_all_bit_Delta_arrow<T>(tmpin, 0, length - 1, maxerror, base,
                                                  out, filter, block_start+1);
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
    int block_start = nvalue;
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

    if (maxerror) {
      if (base > filter1 && base < filter2) {
        *out = block_start;
        out++;
        count++;
      }
      count += filter_read_all_bit_Delta_close_arrow<T>(
          tmpin, 0, length - 1, maxerror, base, out, block_start+1, filter1, filter2);
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
    int block_start = nvalue;
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
    if (maxerror) {
      if (base % base_val > filter1 && base % base_val < filter2) {
        *out = block_start;
        out++;
        count++;
      }
      count += filter_read_all_bit_delta_close_mod_arrow<T>(
          tmpin, 0, length - 1, maxerror, base, out, block_start+1, filter1, filter2,
          base_val);
    } else {
      if (base % base_val > filter1 && base % base_val < filter2) {
        for (int i = 0; i < length; i++) {
          out[i] = block_start + i;
        }
        count += length;
      }
    }

    return count;
  }

};

}  // namespace Codecset

#endif /* SIMDFASTPFOR_H_ */
