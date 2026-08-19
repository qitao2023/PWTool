#pragma once


// CPWToolDlg 对话框
class CPWToolDlg : public CDialogEx
{
// 构造
public:
    CPWToolDlg(CWnd* pParent = nullptr); // 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PWTOOL_DIALOG };
#endif

    protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV 支持


// 实现
protected:
    HICON m_hIcon;
    CString m_strBaseTitle;   // 窗口默认标题（不含账号后缀），首次 UpdateLoginTitle 时记录

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

    // 登录状态同步到窗口标题：已登录时标题显示当前账号，退出登录时恢复默认标题
    void UpdateLoginTitle();

    // 当前打开的模型文件（供上传使用）
    CString m_strCurrentModelPath;
};
