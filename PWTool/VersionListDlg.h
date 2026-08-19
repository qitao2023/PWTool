#pragma once

#include "PWHelper.h"

// CDlgVersionList: 版本选择对话框
// 枚举某 PW 文档的所有版本，让用户选择要下载的版本（默认最新，可回退到历史版本）。
class CDlgVersionList : public CDialogEx
{
public:
    explicit CDlgVersionList(LONG lProjectId, LONG lDocumentId,
        LPCTSTR pszCurVersionDate, CWnd* pParent = nullptr);
    virtual ~CDlgVersionList();

    PWHelper::PWDocVersionItem m_sel;   // 用户选中的版本

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_VERSIONLIST_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    CListCtrl m_list;
    CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> m_arrVersions;
    LONG m_lProjectId;
    LONG m_lDocumentId;
    CString m_strCurVersionDate;    // 本地当前版本的更新时间（INI versionDate），用于标记

    DECLARE_MESSAGE_MAP()
};
