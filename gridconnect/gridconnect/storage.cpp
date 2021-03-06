/*
  storage
  for
   __       ___ ___  ___  __
  |__)  /\   |   |  |__  |__) \_/
  |__) /--\  |   |  |___ |  \ / \
                     gridconnector

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
        // -------- ControlCommandsIn contains commands for the battery
        "CREATE TABLE IF NOT EXISTS ControlCommandsIn ("
        "`id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "`device` INTEGER NOT NULL,"
        "`text1` TEXT,"
        "`text2` TEXT,"
        "`exectime` INTEGER NOT NULL);"
        // -------- Eventlog contains logevents from the battery
        "CREATE TABLE `Eventlog` ("
        "`id`	INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "`eventid`	INTEGER NOT NULL,"
        "`source`	TEXT NOT NULL,"
        "`text1`	TEXT,"
        "`text2`	TEXT,"
        "`logtime`	INTEGER,"
        "`uploaded` INTEGER"
        " );"
        // -------- CurrentState (NetOut) stores states, outputs, responses from the battery
        "CREATE TABLE IF NOT EXISTS ControlStateOut ("
        "`device` INTEGER NOT NULL,"
        "`entity` INTEGER NOT NULL,"
        "`text1` TEXT,"
        "`text2` TEXT,"
        "`logtime` INTEGER NOT NULL,"
        "`uploaded` INTEGER NOT NULL);"
        // -------- CurrentState has an index that comprises of the device/entity pair
        "CREATE UNIQUE INDEX `ControlStateOutIndex` ON `ControlStateOut` (`device` ,`entity` );"
        // -------- Settings per device
        "CREATE TABLE `Settings` ("
        "`device` INTEGER NOT NULL,"
        "`entity` INTEGER NOT NULL,"
        "`entityvalue` INTEGER NOT NULL);"
        "CREATE UNIQUE INDEX `SettingsIndex` ON `Settings` (`device`, `entity`);"
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
          if (query(mDB, "pragma user_version;").run([&](query& row)
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
        std::lock_guard<std::mutex> lock(mLock);
        bool result = true;

        result = mDB.begin(); // begin transaction
        if (result)
        {
          mInsertToLog.bind(1) = device;
          mInsertToLog.bind(2) = entity;
          mInsertToLog.bind(3) = value;
          mInsertToLog.bind(4) = now();
          result &= mInsertToLog.run();
        }
        if (result)
        {
          mInsertToCurrent.bind(1) = device;
          mInsertToCurrent.bind(2) = entity;
          mInsertToCurrent.bind(3) = value;
          mInsertToCurrent.bind(4) = now();
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
        return result;
      }

      bool store::runEvent(std::function<bool(int device, const char*text1, const char*text2)> fun)
      {
        bool result = false;
        mGetNetCommand.run([&](query &row)
        {
          result = fun(row[1], row[2], row[3]);
          if (result)
          {
            mDeleteControlCommand.bind(1) = row[0];
            mDeleteControlCommand.run();
          }
        }
        );
        return result;
      }

      bool store::logEvent(int eventid, const char * source, int device, const char * text1, const char * text2, bool success)
      {
        std::lock_guard<std::mutex> lock(mLock);
        
        bool result = true;

        result = mDB.begin(); // begin transaction
        if (result)
        {
          mInsertToEventLog.bind(1) = device;
          mInsertToEventLog.bind(2) = source;
          mInsertToEventLog.bind(3) = text1;
          mInsertToEventLog.bind(4) = text2;
          mInsertToEventLog.bind(5) = now();
          result &= mInsertToEventLog.run();
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
 
        return result;

      }

      bool store::logState(int device, int entity, const char * text1, const char * text2)
      {
        mInsertToStateLog.bind(1) = device;
        mInsertToStateLog.bind(2) = entity;
        mInsertToStateLog.bind(3) = text1;
        mInsertToStateLog.bind(4) = text2;
        mInsertToStateLog.bind(5) = now();
        return mInsertToStateLog.run();
      }

      bool store::setSetting(int device, int entity, int value)
      {
        mSetSetting.bind(1) = device;
        mSetSetting.bind(2) = entity;
        mSetSetting.bind(3) = value;
        return mSetSetting.run();
      }

      int store::getSetting(int device, int entity)
      {
        int result = 0;
        mGetSetting.bind(1) = device;
        mGetSetting.bind(2) = entity;
        mGetSetting.run([&](query& row)
        {
          result = row[0];
        });
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
        if (result)
        {
          result = mGetNetCommand.prepare(mDB,
            "select id,device,text1,text2 from ControlCommandsIn order by exectime asc limit 0,1");
        }
        if (result)
        {
          result = mDeleteControlCommand.prepare(mDB,
            "delete from ControlCommandsIn where id=?1");
        }
        if (result)
        {
          result = mInsertToEventLog.prepare(mDB,
            "insert into Eventlog (eventid,source,text1,text2,logtime,uploaded) values "
            "(?1,?2,?3,?4,?5,0);");
        }
        if (result)
        {
          result = mInsertToStateLog.prepare(mDB,
            "insert into ControlStateOut (device,entity,text1,text2,logtime,uploaded) values "
            "(?1,?2,?3,?4,?5,0);");
        }
        if (result)
        {
          result = mSetSetting.prepare(mDB,
            "update Settings set entityvalue=?3 where device=?1 and entity=?2;");
        }
        if (result)
        {
          result = mGetSetting.prepare(mDB,
            "select entityvalue from Settings where device=?1 and entity=?2 limit 0,1;");
        }
        return result;
      }
      time_t store::now() const
      {
        auto now = chrono::system_clock::now();
        time_t n = chrono::system_clock::to_time_t(now);
        return n;
      }
    }
  }
}
