
// qi-eDlg.h: 头文件
//

#pragma once

#include <vector>
#include <deque>
#include <utility>

// forward declare GDI+ Bitmap to avoid heavy include in header
namespace Gdiplus { class Bitmap; }

#define WM_TRAYICON (WM_USER + 1)
#define IDC_TRAY_RESTORE 40001
#define IDC_TRAY_HIDE    40002
#define IDC_EXE_EXIT     40003
// id for random skin action
#define IDC_SKIN_RANDOM  49999
// base id for dynamic skin menu items
#define IDC_SKIN_BASE    50000


// CqieDlg 对话框
class CqieDlg : public CDialogEx
{
// 构造
public:
	CqieDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_QIE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;
	// GDI+ token
	ULONG_PTR m_gdiplusToken;
	// image
	Gdiplus::Bitmap* m_pBitmap;
	int m_imgWidth;
	int m_imgHeight;
	bool m_isLayeredShown;
	// dragging state
	bool m_dragging;
	CPoint m_dragOffset;

	// show layered with per-pixel alpha
	void ShowLayered();
	// alpha mask of displayed image (one byte per pixel)
    std::vector<unsigned char> m_alphaMask;
	bool IsOpaque(int x, int y) const;

    // tray
	int m_trayID;
	bool m_trayAdded;

	// available external skin files (full paths)
	std::vector<CString> m_skinFiles;
	// skins directory path
	CString m_skinsDir;

	// handler for dynamic skin menu commands
	afx_msg void OnSkinChange(UINT nID);
	// random skin
	afx_msg void OnSkinRandom();

    // timer handler (not used for shake animation anymore)
	// (OnTimer removed - kept for potential future use)

	// user-drag move history for shake detection: pair<timestamp(ms), top-left point>
	std::deque<std::pair<ULONGLONG, CPoint>> m_moveHistory;
	ULONGLONG m_lastShakeTime;

	// check move history for shake gesture
	void CheckForShake();

	// load skin helper
	void LoadSkinFromFile(const CString& path);
	void AddTrayIcon();
	void RemoveTrayIcon();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMenuHideTray();
	afx_msg void OnMenuRestore();
	afx_msg void OnMenuExit();

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};
