#pragma once

/*******************************

A Class to more easily traverse the FILE_NOTIFY_INFORMATION records returned
by ReadDirectoryChangesW().

FILE_NOTIFY_INFORMATION is defined in Winnt.h as:

typedef struct _FILE_NOTIFY_INFORMATION {
DWORD NextEntryOffset;
DWORD Action;
DWORD FileNameLength;
WCHAR FileName[1];
} FILE_NOTIFY_INFORMATION, *PFILE_NOTIFY_INFORMATION;

ReadDirectoryChangesW basically puts x amount of these records in a
buffer that you specify.
The FILE_NOTIFY_INFORMATION structure is a 'dynamically sized' structure (size depends on length
of the file name (+ sizeof the DWORDs in the struct))

Because each structure contains an offset to the 'next' file notification
it is basically a singly linked list.  This class treats the structure in that way.


Sample Usage:
BYTE Read_Buffer[ 4096 ];

...
ReadDirectoryChangesW(...Read_Buffer, 4096,...);
...

CFileNotifyInformation notify_info( Read_Buffer, 4096);
do{
switch( notify_info.GetAction() )
{
case xx:
notify_info.GetFileName();
}

while( notify_info.GetNextNotifyInformation() );

********************************/
class CFileNotifyInfomation
{
public:
	CFileNotifyInfomation(BYTE* lpFileNotifyInfoBuffer, DWORD dwBuffSize);

	BOOL GetNextNotifyInformation();
	BOOL CopyCurrentRecordToBeginningOfBuffer(OUT DWORD& dwSizeOfCurrentRecord);
	
	DWORD	GetAction() const;
	CString	GetFileName() const;
	CString	GetFileNameWithPath(const CString& rootPath) const;

private:
	BYTE	*_pBuffer;
	DWORD	_dwBufferSize;
	PFILE_NOTIFY_INFORMATION	_pCurrentRecord;

};

