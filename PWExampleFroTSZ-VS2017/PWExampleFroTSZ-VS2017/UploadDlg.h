#pragma once

#include "PWHelper.h"

// CDlgUpload: 上传对话框
// 按当前模型的 PW 地址来源上传新版本；无地址时选目录作为新文档上传。
class CDlgUpload : public CDialogEx
{
public:
	explicit CDlgUpload(const CString& strModelPath, CWnd* pParent = nullptr);
	virtual ~CDlgUpload();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UPLOAD_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()

private:
	BOOL DoUpload();

	CString m_strModelPath;
	CString m_strVersionComment;
	PWHelper::PWAddrInfo m_addr;
	BOOL    m_bHasAddr;
};
