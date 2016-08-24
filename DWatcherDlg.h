
// DWatcherDlg.h : header file
//

#pragma once


// CDWatcherDlg dialog
class CDWatcherDlg : public CDialogEx
{
// Construction
public:
	CDWatcherDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DWATCHER_DIALOG };
#endif

private:
	CString		m_strDirectoryToMonitor;
	//CDirectoryChangeWatcher	m_DirWatcher;

protected:
	HICON m_hIcon;
	
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedBtnBrowse();
	afx_msg void OnBnClickedBtnMonitor();
};
