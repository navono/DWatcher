
// DWatcher.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CDWatcherApp:
// See DWatcher.cpp for the implementation of this class
//

inline static BOOL IsDirectory(const CString& path)
{
	auto dwAttri = GetFileAttributes(path);
	return ((dwAttri != 0xffffffff
		&& (dwAttri & FILE_ATTRIBUTE_DIRECTORY)));
}

class CDWatcherApp : public CWinApp
{
public:
	CDWatcherApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CDWatcherApp theApp;