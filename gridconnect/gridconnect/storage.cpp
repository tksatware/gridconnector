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

#include "storage.h"

namespace satag
{
  namespace energy
  {
    namespace bx
    {
      static const char* schema =
        // user schema version 1
        "PRAGMA USER_VERSION=1;"
        // -------- CollectedData is the logging table which syncs upward to the server
        "CREATE TABLE IF NOT EXISTS CollectedData ("
        "`id`	INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "`device`	INTEGER NOT NULL,"
        "`entity`	INTEGER NOT NULL,"
        "`entityvalue`	INTEGER NOT NULL,"
        "`sampletime`	INTEGER NOT NULL,"
        "`uploadtime`	INTEGER"
        ");"
        // -------- CurrentData reflects the current state
        "CREATE TABLE IF NOT EXISTS CurrentState ("
        //"  `id`	INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "  `device`	INTEGER NOT NULL,"
        "  `entity`	INTEGER NOT NULL,"
        "  `entityvalue`	INTEGER NOT NULL,"
        "  `sampletime`	INTEGER NOT NULL"
        "  );"
        // -------- CurrentData has an index that comprises of the device/entity pair
        "CREATE UNIQUE INDEX `currentStateIndex` ON `CurrentState` (`device` ,`entity` );"
        ;

      store::store()
      {
      }

      store::~store()
      {
        close();
      }

      bool store::open(const char* source)
      {
        bool result = false;
        close();
        mDB.open(source, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX);
        if (mDB.isOpen())
        {
          mDB.execute("PRAGMA synchronous=FULL;");
          int version = 0;
          query q(mDB, "pragma user_version;");
          if (q.run([&](query& row)
          {
            version = row[0];
          }))
          {
            if (version < 1)
            {
              bool result = true;
              result &= createSchema();
            }
            else
            {
              result = true;
            }
          }
          if (result)
          {
            result = createQueries();
          }
          if (!result)
          {
            close();
          }
        }
        return isOpen();
      }

      void store::close()
      {
        mInsertToLog.finalize();
        mInsertToCurrent.finalize();
        mDB.close();
      }

      /*
        logEvent writes a sample data to CollectedData and CurrentState tables.
        it is protected by a std::mutex, which allows to use it from multiple threads
      */
      bool store::logDSPEvent(int device, int entity, int value)
      {
        mLock.lock();
        bool result = true;
        auto now = chrono::system_clock::now();
        time_t n = chrono::system_clock::to_time_t(now);

        result = mDB.begin(); // begin transaction
        if (result)
        {
          mInsertToLog.bind(1) = device;
          mInsertToLog.bind(2) = entity;
          mInsertToLog.bind(3) = value;
          mInsertToLog.bind(4) = n;
          result &= mInsertToLog.run();
        }
        if (result)
        {
          mInsertToCurrent.bind(1) = device;
          mInsertToCurrent.bind(2) = entity;
          mInsertToCurrent.bind(3) = value;
          mInsertToCurrent.bind(4) = n;
          result &= mInsertToCurrent.run();
        }
        if (!result)
        {
          mDB.getErrorMessage();
        }
        if (result)
        {
          mDB.commit();
        }
        else
        {
          mDB.rollback();
          // TODO: log an error about the impossibility to write data
        }
        mLock.unlock();
        return result;
      }

      /*
      creates the database schema and sets the pragma user version
      */
      bool store::createSchema()
      {
        bool result = false;
        if (mDB.isOpen())
        {
          result = mDB.execute(schema);
          if (!result)
          {
            // TODO: log this
            mDB.getErrorMessage();
            close();
          }
        }
        return result;
      }

      /*
      createQueries prepares the statements to be used with logging activities
      */
      bool store::createQueries()
      {
        bool result = true;
        result &= mInsertToLog.prepare(mDB,
          "insert into CollectedData (device,entity,entityvalue,sampletime,uploadtime)"
          "values (?1,?2,?3,?4,0);"
          );
        if (result)
        {
          result = mInsertToCurrent.prepare(mDB,
            "insert or replace into CurrentState (device,entity,entityvalue,sampletime)"
            "values (?1,?2,?3,?4);"
            );
        }
        return result;
      }
    }
  }
}
