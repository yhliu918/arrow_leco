
#pragma once
#ifndef PIECEWISEFIX_INTEGER_TEMPLATE_H_
#define PIECEWISEFIX_INTEGER_TEMPLATE_H_

#include "leco_bitop.h"

#define INF 0x7f7fffff

namespace Codecset {
template <typename T>
class Leco_int {
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

    lr_int_T_arrow<T> mylr;
    mylr.caltheta(data, length);
    double theta0 = mylr.theta0;
    double theta1 = mylr.theta1;

    int64_t max_error_delta = INT64_MIN;
    int64_t min_error_delta = INT64_MAX;
    for (auto i = 0; i < length; i++) {
      int64_t tmp_val = (int64_t)data[i] - (int64_t)(theta0 + theta1 * (double)i);
      if (tmp_val > max_error_delta) max_error_delta = tmp_val;
      if (tmp_val < min_error_delta) min_error_delta = tmp_val;
    }
    theta0 += (max_error_delta + min_error_delta) / 2.;

    std::vector<T> delta;
    std::vector<bool> signvec;
    T max_error = 0;
    for (auto i = 0; i < length; i++) {
      T tmp_val;
      int64_t pred = theta0 + theta1 * (double)i;
      if (int64_t(data[i]) > pred) {
        tmp_val = int64_t(data[i])  - pred;
        signvec.emplace_back(true);  // means positive
      } else {
        tmp_val = pred - int64_t(data[i]) ;
        signvec.emplace_back(false);  // means negative
      }

      delta.emplace_back(tmp_val);

      if (tmp_val > max_error) {
        max_error = tmp_val;
      }
    }

    /* double pred = theta0;
    T max_error = 0;
    for (auto i = 0; i < length; i++)
    {
        T pred_mul = (theta0 + theta1 * (double)i);
        T pred_add = pred;
        if(pred_mul> pred_add){
            mul_add_diff_set.push_back(std::make_pair(i+nvalue*block_size,pred_mul -
    pred_add));
        }
        if(pred_mul< pred_add){
            mul_add_diff_set.push_back(std::make_pair(i+nvalue*block_size,-(int)(pred_add
    - pred_mul)));
        }
        pred +=theta1;

        T tmp_val;
        if (data[i] > pred_mul)
        {
            tmp_val = data[i] - pred_mul;
            signvec.emplace_back(true); // means positive
        }
        else
        {
            tmp_val = pred_mul - data[i];
            signvec.emplace_back(false); // means negative
        }

        delta.emplace_back(tmp_val);

        if (tmp_val > max_error)
        {
            max_error = tmp_val;
        }
    } */

    uint8_t max_bit = 0;
    if (max_error) {
      max_bit = bits_int_T_arrow(max_error) + 1;
    }
    // std::cout<< "max_bit: " << (int)max_bit << std::endl;
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

    memcpy(out, &theta0, sizeof(double));
    out += sizeof(double);
    memcpy(out, &theta1, sizeof(double));
    out += sizeof(double);
    if (max_bit) {
      out = write_delta_int_T_arrow(delta, signvec, out, max_bit, length);
    }

    return out;
  }

  T* decodeArray8(const uint8_t* in, const size_t length, T* out, size_t nvalue) {
    T* res = out;
    // start_index + bit + theta0 + theta1 + numbers + delta
    double theta0;
    double theta1;
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
    if (maxerror >= sizeof(T) * 8 - 1) {
      // out = reinterpret_cast<T*>(tmpin);
      memcpy(out, tmpin, sizeof(T) * length);
      return out;
      // read_all_default(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
    }

    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);

    if (maxerror) {
      // read_all_bit_fix_add<T>(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
      read_all_bit_fix_arrow<T>(tmpin, 0, 0, length, maxerror, theta1, theta0, res);

    } else {
      for (int j = 0; j < length; j++) {
        res[j] = (long long)(theta0 + theta1 * (double)j);
      }
      // double pred = theta0;
      // for (int i = 0;i < length;i++) {
      //     res[i] = (long long)pred;
      //     pred += theta1;
      // }
    }

    return out;
  }


  void decodeRange(const uint8_t* in, T* out, const int start_idx, int length) {
    T* res = out;
    // start_index + bit + theta0 + theta1 + numbers + delta
    double theta0;
    double theta1;
    const uint8_t* tmpin = in;

    uint8_t maxerror = tmpin[0];
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      // out = reinterpret_cast<T*>(tmpin);
      memcpy(out, tmpin+start_idx*sizeof(T), sizeof(T) * length);
      return;
      // read_all_default(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
    }

    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);

    if (maxerror) {
      // read_all_bit_fix_add<T>(tmpin, 0, 0, length, maxerror, theta1, theta0, res);
      read_range_bit_fix_arrow<T>(tmpin, 0, start_idx, length, maxerror, theta1, theta0, res);

    } else {
      for (int j = start_idx; j < start_idx+length; j++) {
        res[j-start_idx] = (long long)(theta0 + theta1 * (double)j);
      }
      // double pred = theta0;
      // for (int i = 0;i < length;i++) {
      //     res[i] = (long long)pred;
      //     pred += theta1;
      // }
    }

    return;
  }


  T summation(const uint8_t* in, const size_t length, size_t nvalue) {
    T returnvalue = 0;
    // start_index + bit + theta0 + theta1 + numbers + delta
    double theta0;
    double theta1;
    const uint8_t* tmpin = in;

    uint8_t maxerror = tmpin[0];
    tmpin++;
    if (maxerror >= sizeof(T) * 8 - 1) {
      const T* castval = reinterpret_cast<const T*>(in);
      for (int i = 0; i < length; i++) {
        returnvalue += castval[i];
      }
      return returnvalue;
    }

    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);

    if (maxerror) {
      returnvalue =
          sum_all_bit_fix_arrow<T>(tmpin, 0, 0, length, maxerror, theta1, theta0);

    } else {
      for (int j = 0; j < length; j++) {
        returnvalue += (long long)(theta0 + theta1 * (double)j);
      }
    }

    return returnvalue;
  }

  T summation_range(const uint8_t* in, int start_idx, int end_idx) {
    T returnvalue = 0;
    // [start_idx, end_idx]

    double theta0;
    double theta1;
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

    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);

    if (maxerror) {
      returnvalue = sum_all_bit_range<T>(tmpin, 0, start_idx, end_idx - start_idx + 1,
                                         maxerror, theta1, theta0);

    } else {
      for (int j = start_idx; j < end_idx + 1; j++) {
        returnvalue += (long long)(theta0 + theta1 * (double)j);
      }
    }

    return returnvalue;
  }

  int filter_range(const uint8_t* in, const size_t length, T filter, uint32_t* out,
                   int block_start) {
    // only consider > filter, return [return_value, end] for demo
    int counter = 0;
    double theta0;
    double theta1;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    maxerror = tmpin[0];
    tmpin++;
    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);
    int64_t delta_interval = 0;
    if (maxerror) {
      delta_interval = (1L << (maxerror - 1));
    }
    int thre = std::max((double)(filter + 1 - delta_interval - theta0) / theta1, 0.);
    if (thre >= length) {
      return counter;
    } else {
      if (maxerror) {
        counter = read_all_bit_fix_range_arrow<T>(
            tmpin, 0, thre, length, maxerror, theta1, theta0, out, filter, block_start);
      } else {
        for (int i = thre; i < length; i++) {
          long long pred = (long long)(theta0 + theta1 * (double)i);
          if (pred > filter) {
            out[0] = block_start + i;
            out++;
            counter++;
          }
        }
      }
    }
    return counter;
  }

  int filter_range_close(const uint8_t* in, const size_t length, uint32_t* out,
                         int block_id, T filter1, T filter2) {
    // only  filter2 > consider > filter1, return [return_value, end] for demo
    int block_start = block_id ;
    int counter = 0;
    double theta0;
    double theta1;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    maxerror = tmpin[0];
    tmpin++;
    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);
    int64_t delta_interval = 0;
    if (maxerror) {
      delta_interval = (1L << (maxerror - 1));
    }
    int thre1 = std::max((double)(filter1 - delta_interval - theta0) / theta1 - 1, 0.);
    int thre2 = std::min((double)(filter2 + delta_interval - theta0) / theta1 + 1,
                         (double)length);
    if (thre1 >= length || thre2 <= 0) {
      return counter;
    } else {
      if (maxerror) {
        counter = read_all_bit_fix_range_close_arrow<T>(tmpin, 0, thre1, thre2, length,
                                                        maxerror, theta1, theta0, out,
                                                        filter1, filter2, block_start);
      } else {
        for (int i = thre1; i <= thre2; i++) {
          long long pred = (long long)(theta0 + theta1 * (double)i);
          if (pred > filter1 && pred < filter2) {
            out[0] = block_start + i;
            out++;
            counter++;
          }
        }
      }
    }
    return counter;
  }

  int filter_range_close_mod(const uint8_t* in, const size_t length, uint32_t* out,
                             int block_id, T filter1, T filter2, T base) {
    // only  filter2 > consider > filter1, return [return_value, end] for demo
    int block_start = block_id ;
    int counter = 0;
    double theta0;
    double theta1;
    uint8_t maxerror;
    const uint8_t* tmpin = in;
    maxerror = tmpin[0];
    tmpin++;
    memcpy(&theta0, tmpin, sizeof(theta0));
    tmpin += sizeof(theta0);
    memcpy(&theta1, tmpin, sizeof(theta1));
    tmpin += sizeof(theta1);
    int64_t delta_interval = 0;
    if (maxerror) {
      delta_interval = (1L << (maxerror - 1));
    }
    int64_t min_val = theta0 - delta_interval;
    int64_t tmp_filter1 =
        ceil((double)(min_val - filter1) / (double)base) * base + filter1;
    int64_t tmp_filter2 =
        ceil((double)(min_val - filter1) / (double)base) * base + filter2;
    if (min_val % base < filter2 && min_val % base > filter1) {
      tmp_filter1 -= base;
      tmp_filter2 -= base;
    }

    int thre1 =
        std::max((double)(tmp_filter1 - delta_interval - theta0) / theta1 - 1, 0.);
    int thre2 = std::min((double)(tmp_filter2 + delta_interval - theta0) / theta1 + 1,
                         (double)length);

    if (thre1 >= length || thre2 <= 0) {
      // std::cout<<thre1<<" "<<thre2<<std::endl;
      tmp_filter1 += base;
      tmp_filter2 += base;
      thre1 = std::max((double)(tmp_filter1 - delta_interval - theta0) / theta1 - 1, 0.);
      thre2 = std::min((double)(tmp_filter2 + delta_interval - theta0) / theta1 + 1,
                       (double)length);
    }
    while (thre1 < length && thre2 > 0) {
      // std::cout<<thre1<<" "<<thre2<<std::endl;
      if (maxerror) {
        counter += read_all_bit_fix_range_close_arrow<T>(
            tmpin, 0, thre1, thre2, length, maxerror, theta1, theta0, out+counter, tmp_filter1,
            tmp_filter2, block_start);
      } else {
        for (int i = thre1; i <= thre2; i++) {
          long long pred = (long long)(theta0 + theta1 * (double)i);
          if (pred > tmp_filter1 && pred < tmp_filter2) {
            out[0] = block_start + i;
            out++;
            counter++;
          }
        }
      }
      tmp_filter1 += base;
      tmp_filter2 += base;
      thre1 = std::max((double)(tmp_filter1 - delta_interval - theta0) / theta1 - 1, 0.);
      thre2 = std::min((double)(tmp_filter2 + delta_interval - theta0) / theta1 + 1,
                       (double)length);
    }

    return counter;
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

    double theta0;
    memcpy(&theta0, tmpin, sizeof(double));
    tmpin += sizeof(double);

    double theta1;
    memcpy(&theta1, tmpin, sizeof(double));
    tmpin += sizeof(double);

    T tmp_val;
    if (maxbits) {
      tmp_val =
          read_bit_fix_int_wo_round_arrow<T>(tmpin, maxbits, to_find, theta1, theta0);
    } else {
      tmp_val = (theta0 + (double)to_find * theta1);
    }
    return tmp_val;
  }
};

}  // namespace Codecset

#endif /* SIMDFASTPFOR_H_ */
