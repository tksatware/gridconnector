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


#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <vector>
#include <functional>

namespace satag
{
  namespace cbor
  {
    enum error : int_fast16_t
    {
      none = 0,           // all is good
      stateerror,         // statemachine gone to error state
      wrongusage,         // a function has been used wrong in the code
      illegalminor,       // minor doesn't match, like major=0,1 and minor > 27
      nestedindefstring,  // indefinite strings must not be nested
      nestedindefbytes,   // indefinite bytes must not be nested
      illegalsimple,      // some simpletype is not supported
      illegaltag,         // a tag above 32bit might be... illegal?
      unevenmap,          // a map wasn't even
      unexpectedbreak,    // a break came in, but we have nothing on our stack
      notimplemented,     // bummer, I was lazy
      nodata,   // the input array stalled
    };

    class listener
    {
    public:
      virtual void int32(int32_t value) = 0;
      virtual void int64(int64_t value) = 0;
      virtual void int64p(uint64_t value) = 0;
      virtual void int64n(uint64_t value) = 0;
      virtual void string(const char* value, size_t len, bool complete) = 0;
      virtual void bytes(const uint8_t* mem, size_t len, bool complete) = 0;
      virtual void float16(float value) = 0;
      virtual void float32(float value) = 0;
      virtual void float64(double value) = 0;
      virtual void boolean(bool value) = 0;
      virtual void null() = 0;
      virtual void tag(uint64_t tag) = 0;
      virtual void array(uint64_t nums) = 0;
      virtual void map(uint64_t nums) = 0;
      virtual void stringahead(uint64_t len) = 0;
      virtual void bytesahead(uint64_t len) = 0;
      virtual void breakend(bool wasIndefinite, bool stackempty) {}
      virtual void time(const char* value) = 0;
      virtual void time(int64_t value) = 0;
      virtual void onerror(error _err) {}
    protected:
      listener() {}
      virtual ~listener() {}
    };

    enum state_t : int_fast16_t
    {
      kSigma = 0,
      kReadInt8,
      kReadInt7,
      kReadInt6,
      kReadInt5,
      kReadInt4,
      kReadInt3,
      kReadInt2,
      kReadInt1,
      kReadString,
      kReadBinary,
      kReadArray,
      kReadMap,
      kReadSimpleValue,
      kReadFloatValue,
      kError,
    };

    class stackitem
    {
    public:
      stackitem(state_t s, uint64_t num)
        : mState(s)
        , numItems(num)
      {}
      state_t mState;
      uint64_t numItems;
    };

    const uint64_t kIndefinite = 0xffffffffffffffff;

    class decoder
    {
    public:
      decoder(listener& _listener, const size_t bufferlen)
        : mOut(_listener)
        , mBufferLen(bufferlen)
      {
        mBuffer = new uint8_t[mBufferLen];
      }
      ~decoder();
      void reset();
      bool parse(const uint8_t* mem, size_t bytesleft);
      bool ok() const { return mState == kSigma && mStack.empty(); }
      error getError() const { return mErrorcode; }
    private:
      state_t mState = kSigma;        // statemachine
      listener& mOut;                 // the event listener
      int mMajor = 0;                 // the current major code
      size_t mBytesLeft = 0;          // bytes left to read in the current streaming block
      uint64_t mLength = 0;           // length of an item
      uint64_t mValue = 0;
      error mErrorcode = none;
      bool mIndefiniteString = false; // true, if chunks are being read for indefinite text string
      bool mIndefiniteBytes = false;  // true, if chunks are being read for indefinite byte string
      bool mMapKeyAhead = false;      // true, if a MAP is being read and we are expecting a key (first item of a pair)
      const uint8_t* mMem = nullptr;  // pointer to the current position in the streaming block
      std::vector<stackitem> mStack;
      size_t mBufferLen = 0;          // size of the intermediate buffer
      uint8_t* mBuffer = nullptr;     // pointer the intermediate buffer
      size_t mCollected = 0;          // bytes collected currently in the intermediate buffer
      size_t mCollectedTotal = 0;     // total bytes collected in the intermediate buffer

      void raiseError(error err);     // set state machine to an error, notify the event listener and store the error code
      void readPositiveOrNegativeInt(int minor);
      void readStringOrByteItem(int minor);
      void readArray(int minor);
      void readMap(int minor);
      void readTagItem(int minor);
      void readSimpleDataTypes(int minor);
      void readFloatValue();

      bool addToBuffer(const uint8_t* mem, size_t len);
      void flushBuffer();

      void countItem();

      inline bool available(uint64_t num) const {
        return mBytesLeft >= num;
      }

      inline void skip(size_t len)
      {
        assert(mBytesLeft >= len);
        mBytesLeft -= len;
        mMem += len;
      }

      inline int take1()  // read 1 byte
      {
        assert(mBytesLeft >= 1);
        mBytesLeft--;
        return *mMem++;
      }
      inline int take2()  // read 2 bytes big endian
      {
        assert(mBytesLeft >= 2);
        mBytesLeft -= 2;
        int v = (*mMem++) << 8;
        v |= *mMem++;
        return v;
      }
      // read 4 bytes big endian
      inline int take4()
      {
        assert(mBytesLeft >= 4);
        mBytesLeft -= 4;
        int v = (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        return v;
      }
      // read 8 bytes big endian
      inline uint64_t take8()
      {
        assert(mBytesLeft >= 8);
        mBytesLeft -= 8;
        uint64_t v = (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        v = (v << 8) + (*mMem++);
        return v;
      }
    };

    class encoder : public listener
    {
      typedef listener super;
    public:
      encoder(std::function<void(const uint8_t* mem, size_t len)> fun)
        : super()
        , mOut(fun)
      {}
      virtual void int32(int32_t value) override;
      virtual void int64(int64_t value)  override;
      virtual void int64p(uint64_t value)  override;
      virtual void int64n(uint64_t value)  override;
      virtual void string(const char* value, size_t len, bool complete)  override;
      virtual void bytes(const uint8_t* mem, size_t len, bool complete)  override;
      virtual void float16(float value)  override;
      virtual void float32(float value)  override;
      virtual void float64(double value)  override;
      virtual void boolean(bool value)  override;
      virtual void null() override;
      virtual void tag(uint64_t tag) override;
      virtual void array(uint64_t nums) override;
      virtual void map(uint64_t nums) override;
      virtual void stringahead(uint64_t len) override;
      virtual void bytesahead(uint64_t len) override;
      virtual void breakend(bool wasIndefinite, bool stackempty) override;
      virtual void time(const char* value) override;
      virtual void time(int64_t value) override;
    private:
      bool mInDefiniteString = false;
      bool mInDefiniteBytes = false;
      bool mDefiniteStringAnnounced = false;
      bool mDefiniteBytesAnnounced = false;
      std::function<void(const uint8_t* mem, size_t len)> mOut;
      // write major code with length
      size_t writeMajor(uint8_t* mem, uint8_t major, uint64_t length);
      // write 16 bit big endian
      inline void write2(uint8_t* mem, uint16_t value)
      {
        mem[0] = (value >> 8);
        mem[1] = (value & 0xff);
      }
      // write 32 bit big endian
      inline void write4(uint8_t* mem, uint32_t value)
      {
        mem[0] = (value >> 24) & 0xff;
        mem[1] = (value >> 16) & 0xff;
        mem[2] = (value >> 8) & 0xff;
        mem[3] = (value & 0xff);
      }
      inline void write8(uint8_t* mem, uint64_t value)
      {
        mem[0] = (value >> 56) & 0xff;
        mem[1] = (value >> 48) & 0xff;
        mem[2] = (value >> 40) & 0xff;
        mem[3] = (value >> 32) & 0xff;
        mem[4] = (value >> 24) & 0xff;
        mem[5] = (value >> 16) & 0xff;
        mem[6] = (value >> 8) & 0xff;
        mem[7] = (value & 0xff);
      }
    };

  }
}
