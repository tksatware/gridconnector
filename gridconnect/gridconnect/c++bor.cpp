/*
  c++bor

  a c++ helper for "more better life" with rfc7049

  Copyright (c)   (c) 2016 tk@satware.com

  Permission is hereby granted, free of charge, to any person obtaining a copy of this
  software and associated documentation files (the "Software"), to deal in the Software
  without restriction, including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to the following
  conditions:

  The above copyright notice and this permission notice shall be included in all copies
  or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
  PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
  FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  The license above does not apply to and no license is granted for any Military Use.

*/

#include "c++bor.h"
#include <cassert>
#include <iostream>
#include <cmath>

namespace satag
{
  namespace cbor
  {

    namespace internal
    {
      float readHalfPrecisionBigEndian(uint8_t* mem)
      {
        // see rfc7049, appendix D
        int half = (mem[0] << 8) + mem[1];
        int exp = (half >> 10) & 0x1f;
        int mant = half & 0x3ff;
        float val;
        if (exp == 0) val = ldexp((float)mant, -24);
        else if (exp != 31) val = ldexp(float(mant) + 1024.f, exp - 25);
        else val = mant == 0 ? INFINITY : NAN;
        return half & 0x8000 ? -val : val;
      }
      float readSinglePrecisionBigEndian(uint8_t* mem)
      {
        uint32_t p;
        p = (mem[0] << 24) | (mem[1] << 16) | (mem[2] << 8) | (mem[3]);
        float result = *(float*)(&p);
        return result;
      }
      double readDoublePrecisionBigEndian(uint8_t* mem)
      {
        uint64_t v = (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        v = (v << 8) + (*mem++);
        double result = *(double*)(&v);
        return result;
      }
    }
    decoder::~decoder()
    {
      delete[] mBuffer;
    }
    void decoder::reset()
    {
      mState = kSigma;
      mStack.clear();
      mErrorcode = none;
    }
    bool decoder::parse(const uint8_t * mem, size_t bytesleft)
    {
      mMem = mem;
      mBytesLeft = bytesleft;

      while (mBytesLeft > 0)
      {
        switch (mState)
        {
          case kSigma:
            {
              uint8_t cur = take1();
              mMajor = cur >> 5;
              int minor = cur & 0x1f;
              switch (mMajor)
              {
                case 0: // positive int
                case 1: // negative int
                  readPositiveOrNegativeInt(minor);
                  break;
                case 2: // byte string
                case 3: // text string
                  readStringOrByteItem(minor);
                  break;
                case 4: // array
                  readArray(minor);
                  break;
                case 5: // map
                  readMap(minor);
                  break;
                case 6: // tag
                  readTagItem(minor);
                  break;
                case 7:
                  readSimpleDataTypes(minor);
                  break;
              } // switch (mMajor)
              break;
            }
            break;
          case kReadInt8:
            mValue = take1();
            mState = kReadInt7;
            break;
          case kReadInt7:
            mValue = (mValue << 8) + take1();
            mState = kReadInt6;
            break;
          case kReadInt6:
            mValue = (mValue << 8) + take1();
            mState = kReadInt5;
            break;
          case kReadInt5:
            mValue = (mValue << 8) + take1();
            mState = kReadInt4;
            break;
          case kReadInt4:
            mValue = (mValue << 8) + take1();
            mState = kReadInt3;
            break;
          case kReadInt3:
            mValue = (mValue << 8) + take1();
            mState = kReadInt2;
            break;
          case kReadInt2:
            mValue = (mValue << 8) + take1();
            mState = kReadInt1;
            break;
          case kReadInt1:
            mValue = (mValue << 8) + take1();
            switch (mMajor)
            {
              case 0:
                // if the value is larger than positive signed 32bitvalue
                if (mValue > 0x7fffffff)
                {
                  if (mValue > 0x7fffffffffffffff)
                  {

                    mOut.int64p(mValue);
                  }
                  else
                  {
                    mOut.int64(mValue);
                  }
                }
                else
                {
                  mOut.int32((int)mValue);
                }
                countItem();
                break;
              case 1:
                // if the value is larger than a positive signed 32bit value
                if (mValue > 0x7fffffff)
                {
                  if (mValue > 0x7fffffffffffffff)
                  {

                    mOut.int64n(mValue+1);
                  }
                  else
                  {
                    int64_t val = mValue;
                    val = -(val+1);
                    mOut.int64(val);
                  }
                }
                else
                {
                  int val = (int)mValue;
                  val = -(val + 1);

                  mOut.int32(val);
                }
                countItem();
                break;
              case 2:
                mLength = mValue;
                mState = kReadString;
                break;
              case 3:
                mLength = mValue;
                mState = kReadBinary;
                break;
              case 4:
                {
                  // array
                  mLength = mValue;
                  if (mLength == 0)
                  {
                    // notify client about an empty array and return to start
                    mOut.array(0);
                    mOut.breakend();
                    countItem();
                  }
                  else
                  {
                    // array is opened and will be closed on break item
                    mStack.push_back(stackitem(mState, mLength));
                    mOut.array(mLength);
                  }
                  // in the end, state is on the stack and needs continuing
                  mState = kSigma;
                }
                break;
              case 5:
                {
                  // map
                  mLength = mValue;
                  if (mLength == 0)
                  {
                    // notify client about an empty array and return to start
                    mOut.map(0);
                    mOut.breakend();
                    countItem();
                  }
                  else
                  {
                    // array is opened and will be closed on break item
                    mStack.push_back(stackitem(mState, mLength));
                    mOut.map(mLength);
                    mMapKeyAhead = true;
                  }
                  // in the end, state is on the stack and needs continuing
                  mState = kSigma;
                }
                break;
              case 6:
                if (mLength <= 0x7fffffff)
                {
                  mOut.tag((int)mLength);
                  mState = kSigma;
                }
                else
                {
                  raiseError(illegaltag);
                }
            }
            break;
          case kReadString:
            if (mLength != kIndefinite)
            {
              if (available(mLength) && (mCollected == 0))
              {
                mOut.string((const char*)mMem, (size_t) mLength, true);
                countItem();
              }
              else
              {
                // the rest of the bytes must be part of the buffer
                addToBuffer(mMem, bytesleft);
                skip(bytesleft);
                assert(bytesleft == 0);
              }
            }
            else
            {
              if (mIndefiniteString)
              { 
                raiseError(nestedindefstring);
              }
              else
              {
                // from now on, we will expect chunks of length definite strings
                mIndefiniteString = true;
                mOut.indefinitestring();
                mStack.push_back(stackitem(kReadString, kIndefinite));
              }
            }
            break;
          case kReadBinary:
            if (mLength != kIndefinite)
            {
              if (available(mLength) && (mCollected == 0))
              {
                mOut.bytes(mMem, (size_t)mLength, true);
                countItem();
              }
              else
              {
                // the rest of the bytes must be part of the buffer
                addToBuffer(mMem, bytesleft);
                skip(bytesleft);
                assert(bytesleft == 0);
              }
            }
            else
            {
              if (mIndefiniteBytes)
              {
                raiseError(nestedindefstring);
              }
              else
              {
                // from now on, we will expect chunks of length definite strings
                mIndefiniteBytes = true;
                mOut.indefinitebytes();
                mStack.push_back(stackitem(kReadBinary, kIndefinite));
              }
            }
            break;
          case kReadSimpleValue:
            {
              mState = kSigma;  // return to sigma state
              auto val = take1(); // see 2.3, table 2
              switch (val)
              {
                case 20:
                  mOut.boolean(false);
                  countItem();
                  break;
                case 21:
                  mOut.boolean(true);
                  countItem();
                  break;
                case 22:
                  mOut.null();
                  countItem();
                  break;
                default:
                  // everything else is unassigned or reserved
                  raiseError(illegalsimple);
                  break;
              }
            }
            break;
          case kReadFloatValue:
            {
              mBuffer[mCollected++] = take1();
              if (mCollected == mLength)
              {
                readFloatValue();
              }
            }
            break;
          case kError:
            // consume the bytes until reset
            take1();
            // do nothing, it is broken now...
            // TODO: Consider resetting on a self describing CBOR
            break;
        }
      }
      return (mErrorcode != none);
    }

    void decoder::raiseError(error err)
    {
      mErrorcode = err;
      mState = kError;
      mOut.onerror(err);
    }

    void decoder::readPositiveOrNegativeInt(int minor)
    {
      {
        int len = minor; //  &0x1f;
        if (len < 24)
        {
          // len is immediate value (see 
          mOut.int32((mMajor == 0) ? len : (-(len+1)));
          countItem();
          // state stays kSigma
        }
        else
        {
          mLength = len;
          mValue = 0;
          // if our memory provides enough bytes, we are omitting the way via state machine
          // for performance reasons
          switch (mLength)
          {
            case 24:  // one byte
              {
                if (available(1))
                {
                  int value = take1();
                  mOut.int32(mMajor == 0 ? value : -(value+1));
                  countItem();
                }
                else
                {
                  mState = kReadInt1;
                }
              }
              break;
            case 25: // two bytes
              {
                if (available(2))
                {
                  int value = take2();
                  mOut.int32(mMajor == 0 ? value : -(value + 1));
                  countItem();
                }
                else
                {
                  mState = kReadInt2;
                }
              }
              break;
            case 26: // four bytes
              {
                if (available(4))
                {
                  int value = take4();
                  mOut.int32(mMajor == 0 ? value : -(value + 1));
                  countItem();
                }
                else
                {
                  mState = kReadInt4;
                }

              }
              break;
            case 27: // eight bytes
              {
                if (available(8))
                {
                  uint64_t value = take8();
                  if (mMajor == 0)
                  {
                    mOut.int64p(value);
                  }
                  else
                  {
                    mOut.int64n(value + 1);
                  }
                  countItem();
                }
                else
                {
                  mState = kReadInt8;
                }
              }
              break;
            default: // error
              raiseError(illegalminor);
              break;
          }
        }
      }
    }

    void decoder::readStringOrByteItem(int minor)
    {
      int len = minor;
      if (len < 24)
      {
        // len is immediate value, so take the expected length and switch mode
        mLength = (size_t)len;
      }
      else
      {
        mLength = len;
        mValue = 0;
        // if our memory provides enough bytes, we are omitting the way via state machine
        // for performance reasons
        switch (mLength)
        {
          case 24:  // one byte
            {
              if (available(1))
              {
                mLength = take1();
              }
              else
              {
                mState = kReadInt1;
              }
            }
            break;
          case 25: // two bytes
            {
              if (available(2))
              {
                mLength = take2();
              }
              else
              {
                mState = kReadInt2;
              }
            }
            break;
          case 26: // four bytes
            {
              if (available(4))
              {
                mLength = take4();
              }
              else
              {
                mState = kReadInt4;
              }

            }
            break;
          case 27: // eight bytes
            {
              if (available(8))
              {
                mLength = take8();
              }
              else
              {
                mState = kReadInt8;
              }
            }
            break;
          case 28: // illegal
          case 29: // illegal
          case 30: // illegal
            raiseError(illegalminor);
            break;
          default: // indefinite
            mLength = kIndefinite;
            if (mMajor == 2)
            {
              mState = kReadBinary;
              mOut.indefinitebytes();
            }
            else  // must be 3!
            {
              mState = kReadString;
              mOut.indefinitestring();
            }
            mStack.push_back(stackitem(kReadArray, kIndefinite));
            // stack gets popped when a break (0xff) comes in
            break;
        }
      }
      // if we haven't decided to go to the read length part of the statemachine, we might immediately check connect this
      if (mState == kSigma)
      {
        // if the current buffer part contains enough bytes for the data, it should 
        // be emitted immediately, skipping the state machine
        if (available(mLength))
        {
          if (mMajor == 2)
          {
            mOut.bytes(mMem, (size_t) mLength, !mIndefiniteBytes);
          }
          else
          {
            mOut.string((const char*)mMem, (size_t)mLength, !mIndefiniteBytes);
          }
          skip((size_t)mLength);
          countItem();
        }
        else
        {
          // mLength contains the the length of the text/bytes expected
          // which need to be read via the state machine
          mState = (mMajor == 2) ? kReadBinary : kReadString;
        }
      }
    }

    void decoder::readArray(int minor)
    {
      mLength = minor;
      if (minor < 24)
      {
        mOut.array(minor);
        if (minor == 0)
        {
          mOut.breakend();
          countItem();
        }
        else
        {
          mStack.push_back(stackitem(kReadArray, mLength));
        }
      }
      else
      {
        switch (minor)
        {
          case 24:
            mState = kReadInt1;
            break;
          case 25:
            mState = kReadInt2;
            break;
          case 26:
            mState = kReadInt4;
            break;
          case 27:
            mState = kReadInt8;
            break;
          case 31:
            mLength = kIndefinite;
            mStack.push_back(stackitem(kReadArray, mLength));
            mOut.array(mLength);
            break;
          default:
            raiseError(illegalminor);
        }
      }

    }

    void decoder::readMap(int minor)
    {
      mLength = minor;
      if (minor < 24)
      {
        mOut.map(minor);
        if (minor == 0)
        {
          mOut.breakend();
          countItem();
        }
        else
        {
          mStack.push_back(stackitem(kReadMap, mLength));
          mMapKeyAhead = true;
        }
      }
      else
      {
        switch (minor)
        {
          case 24:
            mState = kReadInt1;
            break;
          case 25:
            mState = kReadInt2;
            break;
          case 26:
            mState = kReadInt4;
            break;
          case 27:
            mState = kReadInt8;
            break;
          case 31:
            mLength = kIndefinite;
            mStack.push_back(stackitem(kReadMap, mLength));
            mOut.map(mLength);
            mMapKeyAhead = true;
            break;
          default:
            raiseError(illegalminor);
        }
      }

    }

    void decoder::readTagItem(int minor)
    {
      // note that tags do not count as item!
      {
        int len = minor; //  &0x1f;
        if (len < 24)
        {
          // len is immediate value (see 
          mOut.tag(len);
          // state stays kSigma
        }
        else
        {
          mLength = len;
          mValue = 0;
          // if our memory provides enough bytes, we are omitting the way via state machine
          // for performance reasons
          switch (mLength)
          {
            case 24:  // one byte
              {
                if (available(1))
                {
                  mOut.tag(take1());
                }
                else
                {
                  mState = kReadInt1;
                }
              }
              break;
            case 25: // two bytes
              {
                if (available(2))
                {
                  int value = take2();
                  mOut.tag(value);
                }
                else
                {
                  mState = kReadInt2;
                }
              }
              break;
            case 26: // four bytes
              {
                if (available(4))
                {
                  int value = take4();
                  mOut.tag(value);
                }
                else
                {
                  mState = kReadInt4;
                }

              }
              break;
            case 27: // eight bytes
              {
                if (available(8))
                {
                  int64_t value = take8();
                  mOut.tag(value);
                }
                else
                {
                  mState = kReadInt8;
                }
              }
              break;
            default: // error
              raiseError(illegalminor);
              break;
          }
        }
      }
    }

    void decoder::readSimpleDataTypes(int minor)
    {
      switch (minor)
      {
        case 20:
          mOut.boolean(false);
          countItem();
          break;
        case 21:
          mOut.boolean(true);
          countItem();
          break;
        case 22:
          mOut.null();
          countItem();
          break;
        //case 23:
        //  break;
        case 24:
          // simple value in next byte (see 2.3 and table 2)
          mState = kReadSimpleValue;
          break;
        case 25:
          // read half precision
          assert(mCollected == 0);
          mLength = 2;
          mState = kReadFloatValue;
          break;
        case 26:
          // read single precision
          assert(mCollected == 0);
          mLength = 4;
          mState = kReadFloatValue;
          break;
        case 27:
          // read doubleprecision
          assert(mCollected == 0);
          mLength = 8;
          mState = kReadFloatValue;
          break;
        case 31:
          // BREAK
          if (mStack.size() > 0)
          {
            auto& o = mStack.back();
            mState = o.mState;
            switch (mState)
            {
              case kReadString:
              case kReadBinary:
                assert(mIndefiniteString || mIndefiniteBytes);
                flushBuffer();
                mOut.breakend();
                countItem();
                mIndefiniteString = false;
                mIndefiniteBytes = false;
                mState = kSigma;
                break;
              case kReadArray:
                mOut.breakend();
                countItem();
                mState = kSigma;
                break;
              case kReadMap:
                if (mMapKeyAhead)
                {
                  raiseError(unevenmap);
                }
                else
                {
                  mOut.breakend();
                  countItem();
                  mState = kSigma;
                }
                break;
              default:
                raiseError(unexpectedbreak);
            }
            mStack.pop_back();
          }
          else
          {
            raiseError(unexpectedbreak);
          }
          break;
        default:
          raiseError(illegalsimple);
      }
    }

    void decoder::readFloatValue()
    {
      // check length etc.
      if (mLength == 8)
      {
        // read double
        mOut.float64(internal::readDoublePrecisionBigEndian(mBuffer));
      }
      else
      {
        if (mLength == 4)
        {
          // read ieee765
          mOut.float32(internal::readSinglePrecisionBigEndian(mBuffer));
        }
        else
        {
          assert(mLength == 2);
          // read half precision
          mOut.float16(internal::readHalfPrecisionBigEndian(mBuffer));
        }
      }
      mCollected = 0;
      mState = kSigma;
      countItem();
    }

    bool decoder::addToBuffer(const uint8_t * mem, size_t len)
    {
      bool result = false;

      // check, if the buffer exceeds
      if (mCollected > 0)
      {
        if (len + mCollected > mBufferLen)
        {
          if (mState == kReadBinary)
          {
            mOut.bytes(mBuffer, mCollected, false);
          }
          else
          {
            mOut.string((const char*)mBuffer, mCollected, false);
          }
          mCollected = 0;
        }
        // the buffer is clear now
      }

      memcpy(mBuffer + mCollected, mem, len);
      mCollected += len;
      mCollectedTotal += len;
      if (mCollectedTotal == mLength)
      {
        if (mState == kReadBinary)
        {
          mOut.bytes(mBuffer, mCollected, !mIndefiniteString);
          if (!mIndefiniteString)
          {
            countItem();
          }
          result = true;
        }
        else
        {
          mOut.string((const char*)mBuffer, mCollected, !mIndefiniteString);
          if (!mIndefiniteBytes)
          {
            countItem();
          }
          result = true;
        }
        mCollected = 0;
        mCollectedTotal = 0;
      }
      return result;
    }

    void decoder::flushBuffer()
    {
      if (mState == kReadString)
      {
        mOut.string((const char*) mBuffer, mCollected, true);
        countItem();
      }
      if (mState == kReadBinary)
      {
        mOut.bytes(mBuffer, mCollected, true);
        countItem();
      }
      mCollected = 0;
    }

    void decoder::countItem()
    {
      if (mStack.size() > 0)
      {
        auto& o = mStack.back();
        switch (o.mState)
        {
          case kReadArray:
            if (o.numItems != kIndefinite)
            {
              assert(o.numItems > 0);
              o.numItems--;
              if (o.numItems == 0)
              {
                // issue break, pop back
                mOut.breakend();
                mStack.pop_back();
                countItem();
                mState = kSigma;
              }
            }
            break;
          case kReadMap:
            if (mMapKeyAhead)
            {
              // the key doesn't count alone, a value must follow
              mMapKeyAhead = false;
            }
            else
            {
              if (o.numItems != kIndefinite)
              {
                assert(o.numItems > 0);
                o.numItems--;
                if (o.numItems == 0)
                {
                  // issue break, pop back
                  mOut.breakend();
                  mStack.pop_back();
                  countItem();
                  mState = kSigma;
                }
                else
                {
                  mMapKeyAhead = true;
                }
              }
              else
              {
                // expecting another Key (or break)
                mMapKeyAhead = true;
              }
            }
            break;
        }
      }
    }


    // ----------------------------------------------------------------------------

    void encoder::int32(int32_t value)
    {
      std::cout << tab.c_str() << "int: " << value << std::endl;
    }

    void encoder::int64(int64_t value)
    {
      std::cout << tab.c_str() << "int64: " << value << std::endl;
    }

    void encoder::int64p(uint64_t value)
    {
      std::cout << tab.c_str() << "uint64: " << value << std::endl;
    }

    void satag::cbor::encoder::int64n(uint64_t value)
    {
      std::cout << tab.c_str() << "uint64: -" << value << std::endl;
    }

    void encoder::string(const char * value, size_t len, bool complete)
    {
      std::string z;
      z.insert(0,value, len);      
      std::cout << tab.c_str() << "string: " << z.c_str() << std::endl;
    }

    void encoder::bytes(const uint8_t * mem, size_t len, bool complete)
    {
      std::cout << tab.c_str() << "bytes (" << len << ")" << std::endl;
    }

    void encoder::float16(float value)
    {
      std::cout << tab.c_str() << "half precision float: " << value << std::endl;
    }

    void encoder::float32(float value)
    {
      std::cout << tab.c_str() << "float: " << value << std::endl;
    }

    void encoder::float64(double value)
    {
      std::cout << tab.c_str() << "double: " << value << std::endl;
    }

    void encoder::boolean(bool value)
    {
      std::cout << tab.c_str() << "bool: " << value << std::endl;
    }

    void encoder::null()
    {
      std::cout << tab.c_str() << "null" << std::endl;
    }

    void encoder::tag(uint64_t tag)
    {
      if (tag == 55799) // byte representation: 0xd9d9f7
      {
        std::cout << tab.c_str() << "This is a CBOR item" << std::endl;
      }
      else
      {
        std::cout << tab.c_str() << "tag: " << tag << std::endl;
      }
    }

    void encoder::array(uint64_t nums)
    {
      if (nums == kIndefinite)
      {
        std::cout << tab.c_str() << "array with indefinite items " << std::endl;
      }
      else
      {
        std::cout << tab.c_str() << "array with "<< nums << " items " << std::endl;
      }
      tab.push_back(' ');
    }

    void encoder::map(uint64_t nums)
    {
      if (nums == kIndefinite)
      {
        std::cout << tab.c_str() << "map with indefinite items " << std::endl;
      }
      else
      {
        std::cout << tab.c_str() << "map with " << nums << " items " << std::endl;
      }
      tab.push_back(' ');
    }

    void encoder::indefinitestring()
    {
      std::cout << tab.c_str() << "begin indefinite string" << std::endl;
    }

    void encoder::indefinitebytes()
    {
      std::cout << tab.c_str() << "begin indefinite bytes" << std::endl;
    }

    void encoder::breakend()
    {
      tab.pop_back();
    }

    void encoder::time(const char * value)
    {
    }

    void encoder::time(int64_t value)
    {
    }

}
}
