#pragma once

#include <stdio.h>
#include<iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <cmath>
#include <time.h>
#include<algorithm>
#include<queue>
#include<vector>

using int128_t = __int128_t;
using uint128_t = __uint128_t;
template <typename T>
struct lr_int_T_arrow{//theta0+theta1*x
    double theta0;
    double theta1;

    
void caltheta(const T *y, int m){

    double sumx = 0;
    double sumy = 0;
    double sumxy = 0;
    double sumxx = 0;
    for(int i=0;i<m;i++){
        sumx = sumx + (double)i;
        sumy = sumy + (double)y[i];
        sumxx = sumxx+(double)i*i;
        sumxy = sumxy+(double)i*y[i];
    }
    
    double ccc= sumxy * m - sumx * sumy;
    double xxx = sumxx * m - sumx * sumx;

    theta1 = ccc/xxx;
    theta0 = (sumy - theta1 * sumx)/(double)m;
    
}

};

template <typename T>
inline uint32_t bits_int_T_arrow(T v) {
  if(v<0){
    v=-v;
  }
  if (v == 0) return 0;
#if defined(__clang__) || defined(__GNUC__)
  // std::cout<<__builtin_clzll(v)<<" "<<64 - __builtin_clzll(v)<<std::endl;
  return 64 - __builtin_clzll(v);
#else
  assert(false);
#endif
}




template <typename T>
inline uint8_t * write_delta_int_T_arrow(std::vector<T>& in,std::vector<bool>& signvec, uint8_t *out, uint8_t l, int numbers)
{
    uint128_t code = 0;
    int occupy = 0;
    uint64_t endbit = (l * (uint64_t)numbers);
    uint64_t end = 0;
    int writeind = 0;
    
    int readind = 0;
    if (endbit % 8 == 0)
    {
        end = endbit / 8;
    }
    else
    {
        end = endbit / 8 + 1;
    }
    uint8_t *last = out + end;
    uint64_t left_val = 0;

    while (out <= last)
    {
        while (occupy < 8)
        {
            if (readind >= numbers)
            {
                occupy = 8;
                break;
            }

            
            T tmpnum = in[readind];
            bool sign = signvec[readind];
            T value1 =
                (tmpnum & (((T)1 << (uint8_t)(l - 1)) - 1)) 
               + (((T)sign) << (uint8_t)(l - 1));


            code += ((uint128_t)value1 << (uint8_t)occupy);
            occupy += l;
           
            readind++;
        } //end while
        while (occupy >= 8)
        {
            left_val = code >> (uint8_t)8;
            //std::cout<<code<<std::endl;
            code = code & ((1 << 8) - 1);
            uint8_t tmp_char = code;
            occupy -= 8;
            out[0] = tmp_char;
            code = left_val;
            //std::cout<< writeind<<std::endl;
            //std::cout<<occupy<<" "<<left_val<<" "<<unsigned(out[0])<<std::endl;
            out++;
        }
    }
    
    int pad = ceil((sizeof(T)*8 - l)/8);
    for (int i = 0; i < pad; i++)
    {
        out[0] = 0;
        out++;
    }
    return out;
}



template <typename T>
inline void read_all_bit_fix_arrow(const uint8_t* in, int start_byte, int start_index, int numbers, int l, double slope, double start_key, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  T* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)writeind * slope);
      // int128_t decode_val = (long long)(start_key + (double)writeind * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }

      *res = (T)decode_val;
      res++;
      writeind++;
      if(writeind >= numbers){
        return;
      }
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
}


template <typename T>
inline void read_range_bit_fix_arrow(const uint8_t* in, int start_byte, int start_index, int numbers, int l, double slope, double start_key, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)(writeind+start_index) * slope);
      // int128_t decode_val = (long long)(start_key + (double)writeind * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }

      out[writeind]=decode_val;
      writeind++;
      if(writeind >= numbers){
        return;
      }
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return;
}



template <typename T>
inline T sum_all_bit_fix_arrow(const uint8_t* in, int start_byte, int start_index, int numbers, int l, double slope, double start_key)
{
  T returnvalue = 0;
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)writeind * slope);
      // int128_t decode_val = (long long)(start_key + (double)writeind * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }

      returnvalue+=decode_val;
      writeind++;
      if(writeind >= numbers){
        return returnvalue;
      }
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return returnvalue;
}



template <typename T>
inline T sum_all_bit_range(const uint8_t* in, int start_byte, int start_index, int numbers, int l, double slope, double start_key){
  T returnvalue = 0;
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)(writeind+start_index) * slope);
      // int128_t decode_val = (long long)(start_key + (double)writeind * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }

      returnvalue+=decode_val;
      writeind++;
      if(writeind >= numbers){
        return returnvalue;
      }
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return returnvalue;
}


template <typename T>
T sum_range_bit_FOR(const uint8_t* in, int start_index, int numbers, int l, T base){
  T returnvalue = 0;
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      
      returnvalue+=decode_val;
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return returnvalue;
}



template <typename T>
inline int read_all_bit_fix_range_arrow(const uint8_t* in, int start_byte, int start_index, int numbers, int l, double slope, double start_key, uint32_t* out, T filter, int block_start)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end)
  {
    while (left >= l && writeind+start_index<numbers)
    {
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)(writeind+start_index) * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }
      if(decode_val > filter){
        *res = block_start+writeind+start_index;
        res++;
      }
      
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}

template <typename T>
inline int read_all_bit_fix_range_close_arrow(const uint8_t* in, int start_byte, int start_index, int end_index, int numbers, int l, double slope, double start_key, uint32_t* out, T filter1, T filter2, int block_start)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * end_index;
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end)
  {
    while (left >= l && writeind+start_index<end_index)
    {
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      int64_t decode_val = (long long)(start_key + (double)(writeind+start_index) * slope);
      if (!sign)
      {
        decode_val = decode_val - tmpval;
      }
      else
      {
        decode_val = decode_val + tmpval;
      }
      if(decode_val > filter1 && decode_val < filter2){
        *res = block_start+writeind+start_index;
        res++;
      }
      
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}



template <typename T>
inline T read_bit_fix_int_wo_round_arrow(const uint8_t* in, uint8_t l, int to_find, double slope, double start_key)
{
  uint64_t find_bit = to_find * (int)l;
  uint64_t start_byte = find_bit / 8;
  uint8_t start_bit = find_bit % 8;
  uint64_t occupy = start_bit;
  uint64_t total = 0;

  uint128_t decode = (reinterpret_cast<const uint128_t*>(in + start_byte))[0];
  // memcpy(&decode, in+start_byte, sizeof(uint64_t));
  decode >>= start_bit;
  uint64_t decode_64 = decode & (((T)1 << l) - 1);
  // decode &= (((T)1 << l) - 1);

  bool sign = (decode_64 >> (l - 1)) & 1;
  T value = (decode_64 & (((T)1 << (uint8_t)(l - 1)) - 1));
  int64_t out = (start_key + (double)to_find * slope);
  if (!sign)
  {
    out = out - value;
  }
  else
  {
    out = out + value;
  }

  return (T)out;

}


template <typename T>
inline uint8_t * write_FOR_int_T_arrow(T *in, uint8_t *out, uint8_t l, int numbers)
{
    uint128_t code = 0;
    int occupy = 0;
    uint64_t endbit = (l * (uint64_t)numbers);
    uint64_t end = 0;
    int writeind = 0;
    T *tmpin = in;
    int readind = 0;
    if (endbit % 8 == 0)
    {
        end = endbit / 8;
    }
    else
    {
        end = endbit / 8 + 1;
    }
    uint8_t *last = out + end;
    uint64_t left_val = 0;

    while (out <= last)
    {
        while (occupy < 8)
        {
            if (tmpin >= in + numbers)
            {
                occupy = 8;
                break;
            }

            
            T tmpnum = tmpin[0];
            T value1 =
                (tmpnum & (((T)1 << l) - 1));


            code += ((uint128_t)value1 << (uint8_t)occupy);
            occupy += l;
            tmpin++;
            readind++;
        } //end while
        while (occupy >= 8)
        {
            left_val = code >> (uint8_t)8;
            //std::cout<<code<<std::endl;
            code = code & ((1 << 8) - 1);
            uint8_t tmp_char = code;
            occupy -= 8;
            out[0] = tmp_char;
            code = left_val;
            //std::cout<< writeind<<std::endl;
            //std::cout<<occupy<<" "<<left_val<<" "<<unsigned(out[0])<<std::endl;
            out++;
        }
    }
    
    int pad = ceil((sizeof(uint32_t)*8 - l)/8);
    for (int i = 0; i < pad; i++)
    {
        out[0] = 0;
        out++;
    }
    return out;
}


template <typename T>
inline void read_all_bit_FOR_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  T* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      
      *res = decode_val;
      res++;
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
}
template <typename T>
inline void read_range_bit_FOR_arrow(const uint8_t* in, int start_index, int numbers, uint8_t l,T base, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = (int)(start_index * l / (sizeof(uint64_t) * 8));
  int occupy = (start_index * l)%(sizeof(uint64_t) * 8);
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
  decode += tmp_64>>occupy;
  start++;
  left += (sizeof(uint64_t) * 8 -occupy);

  while (start <= end && writeind < numbers)
  {
    while (left >= l && writeind < numbers)
    {
      // int128_t tmp = decode & (((T)1 << l) - 1);
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      
      out[writeind]=decode_val;
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return;
}

template <typename T>
inline void read_range_bit_Delta_arrow(const uint8_t* in, int start_index, int numbers, uint8_t l,T base, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = 0;
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  T* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind<numbers+start_index)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
    
      if (!sign)
      {
        base -= tmpval;
      }
      else
      {
        base += tmpval;
      }
      if(writeind>=start_index){
        *res = (T)base;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
}



template <typename T>
inline T sum_all_bit_FOR_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  T returnval = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      returnval += decode_val;
      
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return returnval;
}


template <typename T>
inline int filter_read_all_bit_FOR_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, T filter, int block_start)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      if(decode_val > filter){
        *res = block_start+writeind;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}



template <typename T>
inline int filter_read_all_bit_Delta_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, T filter, int block_start)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }
  T decodeval = base;
  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);

      if (!sign)
      {
        decodeval -= tmpval;
      }
      else
      {
        decodeval += tmpval;
      }
      if(decodeval > filter){
        *res = block_start+writeind;
        res++;
      }

      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}


template <typename T>
inline int filter_read_all_bit_FOR_close_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, int block_start, T filter1, T filter2)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      if(decode_val > filter1 && decode_val < filter2){
        *res = block_start+writeind;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}


template <typename T>
inline int filter_read_all_bit_Delta_close_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, int block_start, T filter1, T filter2)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }
  T decodeval = base;
  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);

      if (!sign)
      {
        decodeval -= tmpval;
      }
      else
      {
        decodeval += tmpval;
      }

      if(decodeval > filter1 && decodeval < filter2){
        *res = block_start+writeind;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}

template <typename T>
inline int filter_read_all_bit_delta_close_mod_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, int block_start, T filter1, T filter2, T base_val)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }
  T decodeval = base;
  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);

      if (!sign)
      {
        decodeval -= tmpval;
      }
      else
      {
        decodeval += tmpval;
      }

      if(decodeval%base_val > filter1 && decodeval%base_val < filter2){
        *res = block_start+writeind;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}


template <typename T>
inline int filter_read_all_bit_FOR_close_mod_arrow(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, uint32_t* out, int block_start, T filter1, T filter2, T base_val)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  uint32_t* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind <numbers)
    {
      
      T tmpval = decode & (((T)1 << l) - 1);
      decode = (decode >> l);
      T decode_val = base + tmpval;
      T decode_val_mod = decode_val % base_val;
      if(decode_val_mod > filter1 && decode_val_mod < filter2){
        *res = block_start+writeind;
        res++;
      }
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return res-out;
}



template <typename T>
inline T read_FOR_int_arrow(const uint8_t* in, uint8_t l, int to_find)
{
  uint64_t find_bit = to_find * (int)l;
  uint64_t start_byte = find_bit / 8;
  uint8_t start_bit = find_bit % 8;
  uint64_t occupy = start_bit;
  uint64_t total = 0;

  uint128_t decode = (reinterpret_cast<const uint128_t*>(in + start_byte))[0];
  // memcpy(&decode, in+start_byte, sizeof(uint64_t));
  decode >>= start_bit;
  decode &= (((T)1 << l) - 1);
  // T one = 1;
  // one.left_shift((uint8_t)(l+8) ,*result);
  // decode &= (*result - 1);


  return (T)decode;

}


template <typename T>
inline uint8_t * write_delta_int_T(std::vector<T>& in,std::vector<bool>& signvec, uint8_t *out, uint8_t l, int numbers)
{
    uint128_t code = 0;
    int occupy = 0;
    uint64_t endbit = (l * (uint64_t)numbers);
    uint64_t end = 0;
    int writeind = 0;
    
    int readind = 0;
    if (endbit % 8 == 0)
    {
        end = endbit / 8;
    }
    else
    {
        end = endbit / 8 + 1;
    }
    uint8_t *last = out + end;
    uint64_t left_val = 0;

    while (out <= last)
    {
        while (occupy < 8)
        {
            if (readind >= numbers)
            {
                occupy = 8;
                break;
            }

            
            T tmpnum = in[readind];
            bool sign = signvec[readind];
            T value1 =
                (tmpnum & (((T)1 << (uint8_t)(l - 1)) - 1)) 
               + (((T)sign) << (uint8_t)(l - 1));


            code += ((uint128_t)value1 << (uint8_t)occupy);
            occupy += l;
           
            readind++;
        } //end while
        while (occupy >= 8)
        {
            left_val = code >> (uint8_t)8;
            //std::cout<<code<<std::endl;
            code = code & ((1 << 8) - 1);
            uint8_t tmp_char = code;
            occupy -= 8;
            out[0] = tmp_char;
            code = left_val;
            //std::cout<< writeind<<std::endl;
            //std::cout<<occupy<<" "<<left_val<<" "<<unsigned(out[0])<<std::endl;
            out++;
        }
    }
    
    int pad = 8 - end % 8;
    for (int i = 0; i < pad; i++)
    {
        out[0] = 0;
        out++;
    }
    return out;
}



template <typename T>
inline T sum_all_bit_range_delta(const uint8_t* in, int start_byte, int start_index, int numbers, int l, T base){
  T returnvalue = 0;
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * (numbers+start_index);
  int writeind = 0;
  end =  (int)(total_bit / (sizeof(uint64_t) * 8));
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }
  T decodeval = base;

  while (start <= end)
  {
    while (left >= l)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
      if( writeind>=start_index+numbers){
        return returnvalue;
      }
      if(writeind>=start_index){
        returnvalue += decodeval;
      }
      
    
      if (!sign)
      {
        decodeval -= tmpval;
      }
      else
      {
        decodeval += tmpval;
      }

      
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
  return returnvalue;
}

template <typename T>
inline void read_all_bit_Delta(const uint8_t* in, int start_byte, int numbers, uint8_t l,T base, T* out)
{
  int left = 0;
  uint128_t decode = 0;
  uint64_t start = start_byte;
  uint64_t end = 0;
  uint64_t total_bit = l * numbers;
  int writeind = 0;
  end = start + (int)(total_bit / (sizeof(uint64_t) * 8));
  T* res = out;
  if (total_bit % (sizeof(uint64_t) * 8) != 0)
  {
    end++;
  }

  while (start <= end)
  {
    while (left >= l && writeind<numbers)
    {
      
      int64_t tmp = decode & (((T)1 << l) - 1);
      bool sign = (tmp >> (l - 1)) & 1;
      T tmpval = (tmp & (((T)1 << (uint8_t)(l - 1)) - 1));
      decode = (decode >> l);
    
      if (!sign)
      {
        base -= tmpval;
      }
      else
      {
        base += tmpval;
      }

      *res = (T)base;
      res++;
      writeind++;
      left -= l;
      if (left == 0)
      {
        decode = 0;
      }
      // std::cout<<"decode "<<(T)decode_val<<"left"<<left<<std::endl;
    }
    const uint64_t tmp_64 = (reinterpret_cast<const uint64_t*>(in))[start];
    decode += ((uint128_t)tmp_64 << left);
    // decode = decode<<64 + tmp_64;
    start++;
    left += sizeof(uint64_t) * 8;
  }
}




template <typename T>
inline T read_Delta_int(const uint8_t* in, uint8_t l, int to_find, T base)
{
  for (int i = 0;i < to_find;i++) {
    uint64_t find_bit = i * (int)l;
    uint64_t start_byte = find_bit / 8;
    uint8_t start_bit = find_bit % 8;

    uint128_t decode = (reinterpret_cast<const uint128_t*>(in + start_byte))[0];
    // memcpy(&decode, in+start_byte, sizeof(uint64_t));
    decode >>= start_bit;
    decode &= (((T)1 << l) - 1);

    bool sign = (decode >> (l - 1)) & 1;
    T value = (decode & (((T)1 << (uint8_t)(l - 1)) - 1));
    if (!sign) {
      base -= value;
    }
    else {
      base += value;
    }

  }
  return base;

}