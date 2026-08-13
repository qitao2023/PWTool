#pragma once


// CPWExampleFroTSZVS2017Dlg 对话框
class CPWExampleFroTSZVS2017Dlg : public CDialogEx
{
// 构造
public:
	CPWExampleFroTSZVS2017Dlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PWEXAMPLEFROTSZVS2017_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	// PW协同
	afx_msg void OnBnClickedPwLogin();
	afx_msg void OnBnClickedPwLogout();
	afx_msg void OnBnClickedPwOpen();
	afx_msg void OnBnClickedPwLink();
	afx_msg void OnBnClickedPwLinkMgr();
	afx_msg void OnBnClickedPwUpload();

	// 当前打开的模型文件（供上传使用）
	CString m_strCurrentModelPath;
};
