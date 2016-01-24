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
#include <iterator>
#include <thread>
#include <chrono>

#include "c++bor.h"
#include "storage.h"

#include "curl/curl.h"

#if WIN32
// link the library
#pragma comment(lib, "libcurl.lib")
#endif

using namespace std;

static satag::energy::bx::store gStore;

#if 0
// this is for the curl check
uint8_t blob[65536];
size_t bloblen = 0;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{  
  size_t len = nmemb*size;
  if (bloblen + len < sizeof(blob))
  {
    memcpy(blob + bloblen, ptr, len);
    bloblen += len;
  }
  else
  {
    len = 0;
  }
  return len;
}
#endif

int main(int argc, char *argv[], char *envp[])
{
#if 0
  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();

  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.satware.com/");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // curl_easy_setopt(curl, CURLOPT_CAINFO, "certs");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
#endif
#if 0
  {
    // testing cbor decoder/encoder
    uint8_t l[] = { 
      // 0xf9,0x7e,0x00,                                     // infinity
      0xd9,0xd9,0xf7,0x05,0x03,0x63,65,66,67,0x82,0x01,0x82,0x02,0x03,01,
      0x65,0x54,0x65,0x73,0x74,0x73,
      0x18,0x64,      // dez 100
      0x1a,0x00,0x0f,0x42,0x40, // 1000000
      0x1b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, // 18446744073709551615
      0x20,                                         // -1
      0x29,                                         // -10
      0xf9,0x7e,0x00,                               // 16 bit NaN
      0xf9,0x7c,0x00,                               // 16 bit infinity
      0xf4,                                         // false
      0xf5,                                         // true
      0xf6,                                         // null
      0xa2,0x01,0x02,0x03,0x04,                     // map [1:2, 3:4]
      0xf6,                                         // null

    };
    std::vector<uint8_t> out;
    out.reserve(32768);
    satag::cbor::encoder e(
      [&](const uint8_t* mem, size_t len) 
    {
      out.insert(out.end(), &mem[0], &mem[len]);
    });
    
    satag::cbor::decoder z(e,8192);
    z.parse(l, sizeof(l));
    std::cout << "parse result: " << (z.ok() ? "ok" : "failed") << std::endl;
  }

  return 0;
#endif

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
      gStore.runEvent([&](int device, const char* text1, const char* text2)
      {
        // actual execution
        cout << "executing '" << text1 << "/" << text2 << "' on device: " << device << endl;
        // log the event if it worked
        gStore.logEvent(100, "NetIn", device, text1, text2, true);
        return true;  // return true if execution took place to remove the command from the command queue
      });

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

