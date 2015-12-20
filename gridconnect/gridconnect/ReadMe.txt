========================================================================
    gridconnect Project Overview
========================================================================

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

/////////////////////////////////////////////////////////////////////////////

The gridconnect will be a service for a communication device. It connects a battery
to a C&c grid. 

This is completely work in progress and early prototype state.

/////////////////////////////////////////////////////////////////////////////

gridconnect.vcxproj
    This is the main project file for VC++ projects generated using an Application Wizard.
    It contains information about the version of Visual C++ that generated the file, and
    information about the platforms, configurations, and project features selected with the
    Application Wizard.

gridconnect.vcxproj.filters
    This is the filters file for VC++ projects generated using an Application Wizard. 
    It contains information about the association between the files in your project 
    and the filters. This association is used in the IDE to show grouping of files with
    similar extensions under a specific node (for e.g. ".cpp" files are associated with the
    "Source Files" filter).

gridconnect.cpp
    This is the main application source file.


///////battery///////////////////////////////////////////////////////////////

storage.cpp
	This handles the log/command storage as semantic readable functions.

sqliteoo.cpp
	This is a lightweight wrapper to sqlite3 which allows seamless use of the
	original sqlite3 C API.

/////////////////////////////////////////////////////////////////////////////

sqlite.c
	sqlite 3.9.2 amalgation from http://www.sqlite.org
	

/////////////////////////////////////////////////////////////////////////////
