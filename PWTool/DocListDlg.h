#pragma once

#include "PWHelper.h"

// CDocListDlg: PW 文档列表对话框
// 打开 = 单选（只读文档不可选）；链接 = 多选。
class CDocListDlg : public CDialogEx
{
public:
    enum Mode { MODE_OPEN, MODE_LINK };

    explicit CDocListDlg(Mode mode, CWnd* pParent = nullptr);
    virtual ~CDocListDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DOCLIST_DLG };
#endif

    // 输出
    CArray<PWHelper::PWDocItem, PWHelper::PWDocItem&> m_arrSelected;   // 选中文档
    CString m_strLocalPath;                                            // 本地下载目录
    LONG    m_lCurrentProjectId;                                       // 当前 PW 目录 ID

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    afx_msg void OnBnClickedSelectdir();
    afx_msg void OnBnClickedBrowseLocalpath();
    afx_msg void OnNMCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMClickList(NMHDR* pNMHDR, LRESULT* pResult);   // 点击"历史版本"列

    DECLARE_MESSAGE_MAP()

private:
    void FillDocumentList(LONG lProjectId);
    BOOL IsReadOnly(const PWHelper::PWDocItem& item) const;

    Mode m_mode;
    CListCtrl m_list;
    CString m_strCurFolder;
    CArray<PWHelper::PWDocItem, PWHelper::PWDocItem&> m_arrAll;
};
