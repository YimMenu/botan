/*
* Safe(r) Integer Handling
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_UTILS_SAFE_INT_H_
#define BOTAN_UTILS_SAFE_INT_H_

#include <botan/exceptn.h>
#include <string>

namespace Botan {

class Integer_Overflow_Detected final : public Exception
   {
   public:
      Integer_Overflow_Detected(const std::string& file, int line) :
         Exception("Integer overflow detected at " + file + ":" + std::to_string(line))
         {}

      ErrorType error_type() const noexcept override { return ErrorType::InternalError; }
   };

inline size_t checked_add(size_t x, size_t y, const char* file, int line)
   {
   // TODO: use __builtin_x_overflow on GCC and Clang
   size_t z = x + y;
   if(z < x) [[unlikely]]
      {
      throw Integer_Overflow_Detected(file, line);
      }
   return z;
   }

template<typename RT, typename AT>
RT checked_cast_to(AT i)
   {
   RT c = static_cast<RT>(i);
   if(i != static_cast<AT>(c))
      throw Internal_Error("Error during integer conversion");
   return c;
   }

#define BOTAN_CHECKED_ADD(x,y) checked_add(x,y,__FILE__,__LINE__)

}

#endif
