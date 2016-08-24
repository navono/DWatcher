#pragma once
#include "DirectoryChangeWatcher.h"
#include "DelayedDirectoryChangeHandler.h"
#include <memory>


/***********************************
A class to handle changes to files in a directory.
The virtual On_Filexxx() functions are called whenever changes are made to
a watched directory that is being handled by this object...
The On_Filexxx() functions execute in the context of the main thread if true 
is passed to the constructor of CDirectoryChangeWatcher,
OR they fire in the context of a worker thread if false is passed to the 
constructor of CDirectoryChangeWatcher

NOTES:
A CDirectoryChangeHandler can only be used by ONE CDirectoryChangeWatcher object,
but a CDirectoryChangeWatcher object may use multiple CDirectoryChangeHandler objects.

When this object is destroyed, whatever directories that it was being used 
to handle directory changes for will automatically be 'unwatched'.

The class is reference counted.  The reference count is increased every time 
it is used in a (successfull) call to CDirectoryChangeWatcher::WatchDirectory() 
is decremented whenever the directory becomes 'unwatched'.

The only notifications are File Added, Removed, Modified, and Renamed.
Even though the CDirectoryChangeWatcher::WatchDirectory (which'll call 
ReadDirectoryChangesW()) function allows you to specify flags to watch for 
changes to last access time, last write time, attributes changed, etc,
these changes all map out to On_FileModified() which doesn't specify the
type of modification.


NOTE:   The CDirectoryChangeHandler::On_Filexxx() functions
are called in the context of the main thread, the thread that called 
CDirectoryChangeWatcher::WatchDirectory(), if you pass true to the 
constructor of CDirectoryChangeWatcher. This is accomplished via a hidden window,
and REQUIRES that your app has a message pump.
For console applications, or applications w/out a message pump, 
you can pass false to the constructor of CDirectoryChangeWatcher, and these
notifications will fire in the context of a worker thread.  By passing false
to the constructor of CDirectoryChangeWatcher, you do NOT NEED a message 
pump in your application.


************************************/
class CDirectoryChangeHandler
{
public:
	CDirectoryChangeHandler();
	virtual ~CDirectoryChangeHandler();

	// this class is reference counted
	long AddRef();
	long Release();
	long CurrentRefCount() const;

	//causes CDirectoryChangeWatcher::UnwatchDirectory() to be called.
	BOOL UnWatchDirectory();

	const CString& GetChangedDirectoryName() const
	{
		return _strChangedDirectoryName;
	}

protected:
	//
	//	On_FileAdded()
	//
	//	This function is called when a file in one of the watched folders
	//  (or subfolders) has been created.
	//
	//	For this function to execute you'll need to specify 
	//  FILE_NOTIFY_CHANGE_FILE_NAME or FILE_NOTIFY_CHANGE_DIR_NAME(for directories)
	//  when you call CDirectoryChangeWatcher::WatchDirectory()
	//
	virtual void On_FileAdded(const CString& strFileName);

	//
	//	On_FileRemoved()
	//
	//	This function is called when a file in one of the watched folders
	//  (or subfolders) has been deleted(or moved to another directory)
	//
	//	For this function to execute you'll need to specify 
	//  FILE_NOTIFY_CHANGE_FILE_NAME or FILE_NOTIFY_CHANGE_DIR_NAME
	//  (for directories) when you call CDirectoryChangeWatcher::WatchDirecotry()
	//
	virtual void On_FileRemoved(const CString& strFileName);

	//
	//	On_FileNameChanged()
	//
	//	This function is called when a file in one of the watched folders
	//  (or subfolders) has been renamed.
	//
	//
	//	You'll need to specify FILE_NOTIFY_CHANGE_FILE_NAME 
	//  (or FILE_NOTIFY_CHANGE_DIR_NAME(for directories))
	//	when you call CDirectoryChangeWatcher::WatchDirectory()
	//
	//	
	virtual void On_FileNameChanged(const CString& strFileName, const CString& strNewFileName);

	//
	//	On_FileModified()
	//
	//	This function is called whenever an attribute specified by the watch
	//	filter has changed on a file in the watched directory or 
	//
	//	Specify any of the following flags when you call 
	//	CDirectoryChangeWatcher::WatchDirectory()
	//  
	//
	//	FILE_NOTIFY_CHANGE_ATTRIBUTES
	//	FILE_NOTIFY_CHANGE_SIZE 
	//	FILE_NOTIFY_CHANGE_LAST_WRITE 
	//	FILE_NOTIFY_CHANGE_LAST_ACCESS
	//	FILE_NOTIFY_CHANGE_CREATION (* See Note # 1* )
	//	FILE_NOTIFY_CHANGE_SECURITY
	//
	//	
	//	General Note)  Windows tries to optimize some of these notifications.  
	//				   You may not get a notification every single time a 
	//				   file is accessed for example. There's a MS KB article
	//				   or something on this(sorry forgot which one).
	//
	//	Note #1	)   This notification isn't what you might think(FILE_NOTIFY_CHANGE_CREATION). 
	//				See the help files for ReadDirectoryChangesW...
	//				This notifies you of a change to the file's creation 
	//				time, not when the file is created. Use 
	//				FILE_NOTIFY_CHANGE_FILE_NAME to know about newly created files.
	//
	virtual void On_FileModified(const CString& strFileName);

	//
	//	On_ReadDirectoryChangesError()
	//
	//	This function is called when ReadDirectoryChangesW() fails during normal
	//	operation (ie: some time after you've called CDirectoryChangeWatcher::WatchDirectory())
	//
	//
	//	NOTE:  *** READ THIS *** READ THIS *** READ THIS *** READ THIS ***
	//
	//	NOTE: If this function has been called, the watched directory has 
	//			been automatically unwatched. You will not receive any 
	//			further notifications for that directory until 
	//			you call CDirectoryChangeWatcher::WatchDirectory() again.
	//
	//	On_WatchStopped() will not be called.
	virtual void On_ReadDirectoryChangesError(DWORD dwError, const CString& strDirectoryName);

	//
	//	void On_WatchStarted()
	//
	//	This function is called when a directory watch has begun.  
	//	It will be called whether CDirectoryChangeWatcher::WatchDirectory() 
	//	is successful or not. Check the dwError parameter.
	//
	//	PARAMETERS:
	//	DWORD dwError					 -- 0 if successful, else it's the 
	//										return value of GetLastError() 
	//										indicating why the watch failed.
	//	const CString & strDirectoryName -- The full path and name of the 
	//										directory being watched.
	virtual void On_WatchStarted(DWORD dwError, const CString & strDirectoryName);

	//
	//	void On_WatchStopped()
	//
	//	This function is called when a directory is unwatched (except on 
	//	the case of the directory being unwatched due to an error)
	//
	//	WARNING:  *** READ THIS *** READ THIS *** READ THIS *** READ THIS ***
	//
	//	This function MAY be called before the destructor of CDirectoryChangeWatcher 
	//	finishes.  
	//
	//	Be careful if your implementation of this function
	//	interacts with some sort of a window handle or other object(a class, a file, etc.).  
	//	It's possible that that object/window handle will NOT be valid anymore the very last time
	//	that On_WatchStopped() is called.  
	//	This scenario is likely if the CDirectoryChangeWatcher instance is currently watching a
	//	directory, and it's destructor is called some time AFTER these objects/windows
	//	your change handler interacts with have been destroyed.
	//	
	//	If your CDirectoryChangeHandler derived class interacts w/ a window or other
	//	object, it's a good idea to unwatch any directories before the object/window is destroyed.
	//	Otherwise, place tests for valid objects/windows in the implementation of this function.
	//
	//  Failure to follow either tip can result in a mysterious RTFM error, or a 'Run time errors'
	//
	virtual void On_WatchStopped(const CString & strDirectoryName);

	//
	//	bool On_FilterNotification(DWORD dwNotifyAction, LPCTSTR szFileName, LPCTSTR szNewFileName);
	//
	//	This function gives your class a chance to filter unwanted notifications.
	//
	//	PARAMETERS: 
	//			DWORD	dwNotifyAction	-- specifies the event to filter
	//			LPCTSTR szFileName		-- specifies the name of the file for the event.
	//			LPCTSTR szNewFileName	-- specifies the new file name of a file that has been renamed.
	//
	//			**	szFileName and szNewFileName will always be the full path and file name with extension.
	//
	//	RETURN VALUE:
	//			return true , and you will receive the notification.
	//			return false, and your class will NOT receive the notification.
	//
	//	Valid values of dwNotifyAction:
	//		FILE_ACTION_ADDED			-- On_FileAdded() is about to be called.
	//		FILE_ACTION_REMOVED			-- On_FileRemoved() is about to be called.
	//		FILE_ACTION_MODIFIED		-- On_FileModified() is about to be called.
	//		FILE_ACTION_RENAMED_OLD_NAME-- On_FileNameChanged() is about to be call.
	//
	//	  
	//	NOTE:  When the value of dwNotifyAction is FILE_ACTION_RENAMED_OLD_NAME,
	//			szFileName will be the old name of the file, and szNewFileName will
	//			be the new name of the renamed file.
	//
	//  The default implementation always returns true, indicating that all notifications will 
	//	be sent.
	//
	//	NOTE:	This function may or may not be called depending upon the flags you specify to control
	//			filter behavior.
	//			If you are specifying filters when watching the directory, you will not get this notification
	//			if the file name does not pass the filter test, even if this function returns true.
	//
	virtual bool On_FilterNotification(DWORD dwNotifyAction, LPCTSTR szFileName, LPCTSTR szNewFileName);



	//please don't use this function, it will be removed in future releases.
	void SetChangedDirectoryName(const CString & strChangedDirName);

private:
	long	_nRefCount;
	long	_nWatcherRefCount;
	CString	_strChangedDirectoryName;

	friend class CDirectoryChangeWatcher;

	//
	//	This class keeps a reference to the CDirectoryChangeHandler 
	//	that was used when an object of this type is passed 
	//	to CDirectoryChangeWatcher::WatchDirectory().
	//
	//	This way, when the CDirectoryChangeWatcher object is destroyed
	//  (or if CDirectoryChangeHandler::UnwatchDirectory() is called)
	//	AFTER CDirectoryChangeWatcher::UnwatchDirecotry() or 
	//  CDirectoryChangeWatcher::UnwatchAllDirectories() is called 
	//	the directory(or directories) that this CDirectoryChangeWatcher 
	//  object is handling will be automatically unwatched
	//	If the CDirectoryChangeWatcher object is destroyed before the 
	//  CDirectoryChangeHandler objects that are being used with that 
	//  watcher are destroyed, the reference counting prevents this class 
	//  from referencing a destructed object.
	//	Basically, neither class needs to worry about the lifetime of the 
	//  other(CDirectoryChangeWatcher && CDirectoryChangeHandler)
	//
	friend class CDelayedDirectoryChangeHandler;

	std::unique_ptr<CDirectoryChangeWatcher> _pDirChangeWatcher;
	CRITICAL_SECTION		_csWatcher;

private:
	long	_ReferencesWatcher(CDirectoryChangeWatcher *pDirChangeWatcher);
	long	_ReleaseReferenceToWatcher(CDirectoryChangeWatcher *pDirChangeWatcher);

};

