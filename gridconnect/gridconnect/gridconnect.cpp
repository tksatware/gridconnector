/* gridconnect.cpp : Defines the entry point for the console application.

status: early stage prototyping - architectural decisions aren't finalized

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

#include <vector>
#include <conio.h>    // just for _getch(), remove it later
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

#include "storage.h"

using namespace std;

static satag::energy::bx::store gStore;

int main(int argc, char *argv[], char *envp[])
{
  cout << "batterx communication starting..." << endl;

  cout << "opening database...";
  if (gStore.open("battery.sq3"))
  {
    cout << "done" << endl;

    // vector container stores threads
    vector<thread> workers;

    workers.push_back(
      thread([]()
    {
      cout << "starting battery watcher\n";
      gStore.logDSPEvent(1, 1, 3);
    }));

    workers.push_back(
      thread([]()
    {
      cout << "starting database logger\n";
      gStore.logDSPEvent(1, 3, 7);

    }));

    workers.push_back(
      thread([]()
    {
      cout << "starting command execution\n";
      gStore.logDSPEvent(1, 3, 8);
    }));

    // wait... this will be changed later, but for now we just need this
    cout << "all threads started, press space to terminate\n";

    do
    {
      auto c = _getch();
      if (c == ' ')
        break;
    } while (true);

    cout << "waiting for threads to terminate..." << endl;

    for (auto& w : workers)
    {
      w.join();
    }
    cout << "closing database..." << endl;

    gStore.close();
  }
  else
  {
    cout << "failed" << endl;
  }



}

