#pragma once

#include "PWHelper.h"

// CDlgLinkMgr: 链接管理对话框
// 管理 LinkModel 目录下的链接模型：查看最新版本、添加/删除/链接/卸载/更新。
class CDlgLinkMgr : public CDialogEx
{
public:
	explicit CDlgLinkMgr(CWnd* pParent = nullptr);
	virtual ~CDlgLinkMgr();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LINKMGR_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedAdd();
	afx_msg void OnBnClickedDel();
	afx_msg void OnBnClickedLink();
	afx_msg void OnBnClickedUnlink();
	afx_msg void OnBnClickedUpdate();

	DECLARE_MESSAGE_MAP()

private:
	// 列表行数据
	struct LinkItem
	{
		CString strLocalPath;               // 本地完整路径
		CString strFileName;                // 文件名
		PWHelper::PWAddrInfo addr;          // PW 地址
		BOOL    bHasAddr;                   // 是否有 PW 地址
		BOOL    bLink;                      // 链接状态

		LinkItem() : bHasAddr(FALSE), bLink(FALSE) {}
	};

	void ReloadList();                      // 扫描各链接目录 + INI 合并构建列表
	void DetectLatestVersions();            // 逐项查询 PW 最新版本日期
	int  GetSelectedRow() const;
	int  FindItem(LPCTSTR pszFileName) const;   // 按文件名找已存在的行号，找不到返回 -1
	void SetStatusText(LPCTSTR psz);

	CListCtrl m_list;
	CArray<LinkItem, LinkItem&> m_arrItems;
};
