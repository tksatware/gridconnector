/*
  storage

  bx::store is the datastore for the battery logging/control to and from the batterx grid

  Copyright (c)   (c) 2015,2016 tk@satware.com

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

#include <functional>
#include <mutex>
#include <chrono>

#include "sqliteoo.h"

namespace satag
{
  namespace energy
  {
    namespace bx
    {
      using namespace std;
      using namespace satag::util;
      // classes
      class store
      {
      public:
        store();
        ~store();
        bool open(const char* source);
        void close();
        bool isOpen() const { return mDB.isOpen(); }
        bool logDSPEvent(int device, int entity, int value);
        bool runEvent(std::function<bool(int device, const char* text1, const char* text2)> fun);
        bool logEvent(int eventid,const char * source, int device,  const char* text1, const char* text2, bool success);
        bool logState(int eventid, int device, const char* text1, const char* text2);
        bool setSetting(int device, int entity, int value);
        int getSetting(int device, int entity);
      protected:
        bool createSchema();
        bool createQueries();
        time_t now() const; 
      private:
        db mDB;                       // the database object
        query mInsertToLog;           // the statement to log data to CollectedData
        query mInsertToCurrent;       // the statement to log data to CurrentState
        query mGetNetCommand;         // the statement to retrieve a command for the battery
        query mDeleteControlCommand;  // removes a command from the ControlCommandsIn Table
        query mInsertToEventLog;      // the statement to insert into the event log
        query mInsertToStateLog;      // the statement to insert into the state log
        query mSetSetting;            // the statement to save a setting
        query mGetSetting;            // the statement to retrieve a setting
        mutex mLock;                  // lock to use prepared statements from multiple threads
      };
    }
  }
}