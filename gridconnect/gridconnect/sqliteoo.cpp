/*

sqliteoo

a c++ helper for "more better life" with the sqlite3 C-API.

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

#include "sqliteoo.h"

/* your lack of comments is disturbing :) */

namespace satag
{
  namespace util
  {
    db::db()
    {
    }

    db::db(const char* databasename, int flags, const char* vfs)
    {
      open(databasename, flags, vfs);
    }

    db::~db()
    {
      close();
    }

    bool db::open(const char * databasename, int flags, const char * vfs)
    {
      if (!(sqlite3_open_v2(databasename, &mDB, flags, vfs) == SQLITE_OK))
      {
        mDB = nullptr;
      }
      return isOpen();
    }

    void db::close()
    {
      if (isOpen())
      {
        sqlite3_close_v2(mDB);
        mDB = nullptr;
      }
    }


    query::query(db & d, const char * sql)
    {
      prepare(d, sql);
    }

    query::~query()
    {
      mFields.clear();
      sqlite3_finalize(mStatement); // it's okay to be called with nullptr
    }

    bool query::prepare(db & d, const char * sql)
    {
      mDB = &d;
      auto result = sqlite3_prepare_v2(d.mDB, sql, -1, &mStatement, nullptr);
      if (!(result == SQLITE_OK))
      {
        mDB->getErrorMessage();
        mLastResult = result;
        mStatement = nullptr;
      }
      else
      {
        size_t numfields = sqlite3_column_count(mStatement);
        mFields.reserve(numfields);
        for (size_t i = 0; i < numfields; ++i)
        {
          mFields.push_back(field(*this, i));
        }
        size_t numbindings = sqlite3_bind_parameter_count(mStatement);
        // the binding 0 is forbidden, we still create it
        mBindings.reserve(numbindings);
        for (size_t i = 0; i < numbindings; ++i)
        {

          mBindings.push_back(binding(*this, i + 1));
        }
      }
      return isPrepared();
    }

    bool query::run(std::function<void(query& row)> fun)
    {
      bool result = true;
      int r = SQLITE_ERROR;
      if (fun)
      {
        do {
          r = sqlite3_step(mStatement);
          if (r == SQLITE_ROW)
          {
            fun(*this);
          }
          else
          {
            result = (r == SQLITE_DONE);
          }
        } while (r == SQLITE_ROW);
      }
      else
      {
        do {
          r = sqlite3_step(mStatement);
          result = (r == SQLITE_DONE);
        } while (r == SQLITE_ROW);
      }
      reset();
      return result;
    }

    field& query::operator[](int index)
    {
      if (index > (int)mFields.size())
      {
        index = 0;
      }
      return mFields[index];
    }

    binding & query::bind(int index)
    {
      return mBindings.at(index - 1);
    }

    bool query::finalize()
    {
      bool result = (SQLITE_OK == sqlite3_finalize(mStatement));
      mStatement = nullptr;
      return (result = SQLITE_OK);
    }

    bool db::execute(const char * sql)
    {
      int result = sqlite3_exec(mDB, sql, nullptr, nullptr, nullptr);
      return (result == SQLITE_OK);
    }

    bool db::begin()
    {
      return execute("BEGIN IMMEDIATE TRANSACTION;");
    }

    bool db::commit()
    {
      return execute("COMMIT TRANSACTION;");
    }

    bool db::rollback()
    {
      return execute("ROLLBACK TRANSACTION");
    }

    field::operator const int() const
    {
      return sqlite3_column_int(_q, _i);
    }

    field::operator const char*() const
    {
      return (const char*)sqlite3_column_text(_q, _i);
    }

    field::operator const double() const
    {
      return sqlite3_column_double(_q, _i);
    }

    field::operator const sqlite3_value*() const
    {
      return sqlite3_column_value(_q, _i);
    }


  }
}