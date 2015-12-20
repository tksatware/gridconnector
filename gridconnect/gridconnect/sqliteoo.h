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

#include <functional>
#include <vector>

#include "sqlite3.h"

namespace satag
{
  namespace util
  {
    class query;

    /*
      the db class abstracts a database entity
      it supports direct type conversion to sqlite3*, so it can seamlessly be used with the 
      sqlite3 API functions without any "getDBHandle()" function or similar
    */
    class db
    {
      friend class query;
    public:
      explicit db();
      explicit db(const char* databasename, int flags, const char* vfs = nullptr);
      ~db();
      bool open(const char* databasename, int flags, const char* vfs = nullptr);
      bool isOpen() const { return (mDB != nullptr); }
      void close();
      operator sqlite3*() const { return mDB; }
      operator sqlite3*() { return mDB; }
      bool execute(const char* sql);
      const char* getErrorMessage() const { return sqlite3_errmsg(mDB); }
      bool begin();
      bool commit();
      bool rollback();
    protected:
      sqlite3* mDB = nullptr;
    };

    class field;
    class binding;

    /*
      the query class abstracts a statement/query for a database.

      examples:

        query(db,"create table a (id integer, name text);").run();

        query q(db,"select id,name from a;");
        q.run([](query& row)
        {
          int id = row[0];
          const char* name = row[1];
          cout << id << ":" << name;
        }
        );

        you might derive from query, using specific member functions instead
        of the generic bind() call.
    */
    class query
    {
    public:
      friend class field;
      query() {}
      query(db& d, const char* sql);
      ~query();
      bool prepare(db& d, const char* sql);
      bool isPrepared() const { return (mStatement != nullptr); }
      inline operator sqlite3_stmt*() const { return mStatement; }
      inline operator sqlite3_stmt*() { return mStatement; }
      bool reset() { return (SQLITE_OK == sqlite3_reset(mStatement)); }
      bool run(std::function<void(query& row)> fun = nullptr);
      field& operator[](int index);
      binding& bind(int index);
      int getError() const { return mLastResult; }
      const char* getErrorMessage() const { return sqlite3_errmsg(*mDB); }
      bool finalize();
    protected:
      db* mDB = nullptr;
      sqlite3_stmt* mStatement = nullptr;
      std::vector<field> mFields;
      std::vector<binding> mBindings;
      int mLastResult = SQLITE_OK;
    };

    /*
      class field provides binding to a value so we can use the [] operator on the
      query class and use a suitable auto type conversion from the field value
    */
    class field
    {
    public:
      field(query& q, size_t index)
        : _q(q)
        , _i(index)
      {}
      operator const int() const;
      operator const char*() const;
      operator const double() const;
      operator const sqlite3_value*() const;
      int Type() const { return sqlite3_column_type(_q, _i); }
    protected:
      query& _q;
      size_t _i;
    };

    /*
      the binding object is being used to manage parameter binding. Parameters
      are directly passed to the sqlite3_stmt object.
    */

    class binding
    {
    public:
      binding(query& q, size_t index)
        : _q(q)
        , _i(index)
      {}
      void operator=(const int i) { sqlite3_bind_int(_q, _i, i); }
      void operator=(const int64_t i) { sqlite3_bind_int64(_q, _i, i); }
      void operator=(const char* s) { sqlite3_bind_text(_q, _i, s, -1, SQLITE_TRANSIENT); }
      void operator=(const std::string &s) { sqlite3_bind_text(_q, _i, s.c_str(), -1, SQLITE_TRANSIENT); }
      void operator=(const sqlite3_value* val) { sqlite3_bind_value(_q, _i, val); }
    protected:
      query& _q;
      size_t _i;
    };

  }
}