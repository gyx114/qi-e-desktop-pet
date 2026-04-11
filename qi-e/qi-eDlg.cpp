
// qi-eDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "qi-e.h"
#include "qi-eDlg.h"
#include "afxdialogex.h"
// for GDI+
#include <gdiplus.h>
using namespace Gdiplus;
// for CreateStreamOnHGlobal / IStream
#include <objidl.h>
#include <vector>
#include <shellapi.h>

// target maximum display dimension for skins (shrink-to)
#define SKIN_DISPLAY_MAX 150

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CqieDlg 对话框



CqieDlg::CqieDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_QIE_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_gdiplusToken = 0;
	m_pBitmap = nullptr;
	m_imgWidth = m_imgHeight = 0;
	m_isLayeredShown = false;
	m_dragging = false;
    m_trayAdded = false;
	m_trayID = 1000;
}

void CqieDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CqieDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
   ON_WM_QUERYDRAGICON()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
    ON_WM_RBUTTONUP()
	ON_WM_DESTROY()
    ON_MESSAGE(WM_TRAYICON, &CqieDlg::OnTrayIcon)
	ON_COMMAND(IDC_TRAY_RESTORE, &CqieDlg::OnMenuRestore)
	ON_COMMAND(IDC_TRAY_HIDE, &CqieDlg::OnMenuHideTray)
	ON_COMMAND(IDC_EXE_EXIT, &CqieDlg::OnMenuExit)
   ON_COMMAND_RANGE(IDC_SKIN_BASE, IDC_SKIN_BASE+1000, &CqieDlg::OnSkinChange)
END_MESSAGE_MAP()


// CqieDlg 消息处理程序

BOOL CqieDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}

	// scan skins directory next to exe
	wchar_t buf[MAX_PATH];
	if (GetModuleFileNameW(NULL, buf, MAX_PATH))
	{
		CString exePath(buf);
		int pos = exePath.ReverseFind(L'\\');
		CString exeDir = exePath.Left(pos);
		CString skinsDir = exeDir + L"\\skins";
		WIN32_FIND_DATA fd;
		CString pattern = skinsDir + L"\\*.png";
		HANDLE h = FindFirstFile(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				CString f = skinsDir + L"\\" + fd.cFileName;
				m_skinFiles.push_back(f);
			} while (FindNextFile(h, &fd));
			FindClose(h);
		}
	}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	// Initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);

	// Change window style to popup layered and topmost
	LONG ex = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	ex |= WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
	SetWindowLong(m_hWnd, GWL_EXSTYLE, ex);

	// Remove caption and borders so layered window has only client area (no title bar)
	LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
	style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_BORDER);
	style |= WS_POPUP;
	SetWindowLong(m_hWnd, GWL_STYLE, style);
	SetWindowPos(NULL, 0,0,0,0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // load image from embedded PNG resource IDB_PNG1
	HRSRC hRes = FindResource(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PNG1), L"PNG");
	if (hRes)
	{
		HGLOBAL hMem = LoadResource(AfxGetInstanceHandle(), hRes);
		DWORD resSize = SizeofResource(AfxGetInstanceHandle(), hRes);
		void* pResData = hMem ? LockResource(hMem) : nullptr;
		if (pResData && resSize > 0)
		{
			IStream* pStream = nullptr;
			if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &pStream)) && pStream)
			{
				ULONG written = 0;
				HRESULT hr = pStream->Write(pResData, resSize, &written);
				if (SUCCEEDED(hr))
				{
					LARGE_INTEGER li = {0};
					pStream->Seek(li, STREAM_SEEK_SET, NULL);
					m_pBitmap = Bitmap::FromStream(pStream);
                    if (m_pBitmap && m_pBitmap->GetLastStatus() == Ok)
					{
						int origW = (int)m_pBitmap->GetWidth();
						int origH = (int)m_pBitmap->GetHeight();
                        // compute scale to limit maximum size
						const int MAX_DIM = SKIN_DISPLAY_MAX; // max width/height to display (reduced)
						double scale = 1.0;
						if (origW > MAX_DIM || origH > MAX_DIM)
						{
							double sx = (double)MAX_DIM / origW;
							double sy = (double)MAX_DIM / origH;
							scale = min(sx, sy);
						}
						// additionally enforce user-requested factor: display at most 1/3 of original
						const double FORCE_FACTOR = 1.0/3.0;
						scale = min(scale, FORCE_FACTOR);
						m_imgWidth = max(1, (int)floor(origW * scale + 0.5));
						m_imgHeight = max(1, (int)floor(origH * scale + 0.5));

						TRACE("penguin original size: %d x %d, scaled to: %d x %d\n", origW, origH, m_imgWidth, m_imgHeight);

						// sample center pixel alpha to see if PNG has transparency
						Gdiplus::Color sample;
						m_pBitmap->GetPixel(origW/2, origH/2, &sample);
						TRACE("sample alpha at center = %d\n", sample.GetA());

						// Resize window to image size
						SetWindowPos(nullptr, 100, 100, m_imgWidth, m_imgHeight, SWP_NOZORDER | SWP_NOACTIVATE);
						ShowLayered();
					}
				}
				pStream->Release();
			}
		}
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CqieDlg::ShowLayered()
{
    if (!m_pBitmap) return;

	// Render into a premultiplied 32bpp GDI+ bitmap first
	Bitmap temp(m_imgWidth, m_imgHeight, PixelFormat32bppPARGB);
	{
		Graphics g(&temp);
		g.Clear(Color::Transparent);
		g.DrawImage(m_pBitmap, Rect(0,0,m_imgWidth,m_imgHeight));
	}

	// Lock bits to access pixel buffer
	BitmapData bd;
	Rect r(0, 0, m_imgWidth, m_imgHeight);
	if (temp.LockBits(&r, ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Ok)
		return;

	// create 32bpp DIBSection (top-down)
	BITMAPINFO bmi = {0};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = m_imgWidth;
	bmi.bmiHeader.biHeight = -m_imgHeight; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pvBits = nullptr;
	HDC hdcScreen = ::GetDC(NULL);
	HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
	HDC hMem = CreateCompatibleDC(hdcScreen);
	HBITMAP hOld = (HBITMAP)SelectObject(hMem, hbmp);

    // copy pixels from GDI+ buffer to DIBSection (handle stride signs)
	BYTE* src = static_cast<BYTE*>(bd.Scan0);
	BYTE* dst = static_cast<BYTE*>(pvBits);
	int srcStride = bd.Stride;
	int dstStride = m_imgWidth * 4;

    // detect if temp contains any non-opaque alpha
	bool hasAlpha = false;
	for (int y = 0; y < m_imgHeight && !hasAlpha; ++y)
	{
		BYTE* row = src + (srcStride > 0 ? y * srcStride : (m_imgHeight - 1 - y) * (-srcStride));
		for (int x = 0; x < m_imgWidth; ++x)
		{
			BYTE a = row[x*4 + 3];
			if (a != 255)
			{
				hasAlpha = true;
				break;
			}
		}
	}

	TRACE("temp hasAlpha = %d, srcStride=%d, dstStride=%d\n", hasAlpha ? 1 : 0, srcStride, dstStride);

	// prepare alpha mask
	m_alphaMask.assign(m_imgWidth * m_imgHeight, 0);

	if (hasAlpha)
	{
		// copy premultiplied pixels and fill mask from alpha channel
		for (int y = 0; y < m_imgHeight; ++y)
		{
			BYTE* srcRow = src + (srcStride > 0 ? y * srcStride : (m_imgHeight - 1 - y) * (-srcStride));
			BYTE* dstRow = dst + y * dstStride;
			for (int x = 0; x < m_imgWidth; ++x)
			{
				BYTE pb = srcRow[x*4 + 0];
				BYTE pg = srcRow[x*4 + 1];
				BYTE pr = srcRow[x*4 + 2];
				BYTE pa = srcRow[x*4 + 3];
				dstRow[x*4 + 0] = pb;
				dstRow[x*4 + 1] = pg;
				dstRow[x*4 + 2] = pr;
				dstRow[x*4 + 3] = pa;
				m_alphaMask[y * m_imgWidth + x] = pa;
			}
		}
	}
	else
	{
		// fallback: color-key using corner sampling (legacy behavior)
		int tol = 50; // color tolerance for background detection
		int sampleCount = 0;
		int sumB = 0, sumG = 0, sumR = 0, sumA = 0;
		auto sampleAt = [&](int sx, int sy){
			BYTE* p = src + (srcStride > 0 ? sy * srcStride : (m_imgHeight - 1 - sy) * (-srcStride)) + sx*4;
			sumB += p[0]; sumG += p[1]; sumR += p[2]; sumA += p[3]; sampleCount++; };
		sampleAt(0,0);
		sampleAt(m_imgWidth-1,0);
		sampleAt(0,m_imgHeight-1);
		sampleAt(m_imgWidth-1,m_imgHeight-1);
		BYTE bgB = (sampleCount>0) ? (BYTE)(sumB/sampleCount) : 0;
		BYTE bgG = (sampleCount>0) ? (BYTE)(sumG/sampleCount) : 0;
		BYTE bgR = (sampleCount>0) ? (BYTE)(sumR/sampleCount) : 0;
		BYTE bgA = (sampleCount>0) ? (BYTE)(sumA/sampleCount) : 0;
		TRACE("fallback bg sample premultiplied BGRA = (%d,%d,%d,%d), tol=%d\n", bgB, bgG, bgR, bgA, tol);

		for (int y = 0; y < m_imgHeight; ++y)
		{
			BYTE* srcRow = src + (srcStride > 0 ? y * srcStride : (m_imgHeight - 1 - y) * (-srcStride));
			BYTE* dstRow = dst + y * dstStride;
			for (int x = 0; x < m_imgWidth; ++x)
			{
				BYTE b = srcRow[x*4 + 0];
				BYTE g = srcRow[x*4 + 1];
				BYTE r = srcRow[x*4 + 2];
				BYTE a = srcRow[x*4 + 3];
				int diff = abs((int)b - bgB) + abs((int)g - bgG) + abs((int)r - bgR);
				if (diff <= tol)
				{
					dstRow[x*4 + 0] = 0;
					dstRow[x*4 + 1] = 0;
					dstRow[x*4 + 2] = 0;
					dstRow[x*4 + 3] = 0;
					m_alphaMask[y * m_imgWidth + x] = 0;
				}
				else
				{
					// premultiply channels
					dstRow[x*4 + 0] = (BYTE)((b * a + 127) / 255);
					dstRow[x*4 + 1] = (BYTE)((g * a + 127) / 255);
					dstRow[x*4 + 2] = (BYTE)((r * a + 127) / 255);
					dstRow[x*4 + 3] = a;
					m_alphaMask[y * m_imgWidth + x] = a;
				}
			}
		}
	}

	temp.UnlockBits(&bd);

	POINT ptSrc = {0,0};
	SIZE sizeWnd = { m_imgWidth, m_imgHeight };
	CRect rc;
	GetWindowRect(&rc);
	POINT ptWnd = { rc.left, rc.top };

	BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
	::UpdateLayeredWindow(m_hWnd, hdcScreen, &ptWnd, &sizeWnd, hMem, &ptSrc, 0, &bf, ULW_ALPHA);

	SelectObject(hMem, hOld);
	DeleteObject(hbmp);
	DeleteDC(hMem);
	::ReleaseDC(NULL, hdcScreen);
	m_isLayeredShown = true;
}

static inline bool PixelHitTest(Bitmap* bmp, int x, int y)
{
    if (!bmp) return false;
	int w = (int)bmp->GetWidth();
	int h = (int)bmp->GetHeight();
	if (x < 0 || y < 0 || x >= w || y >= h) return false;
	Color c;
	bmp->GetPixel(x, y, &c);
	return c.GetA() > 10; // threshold
}

bool CqieDlg::IsOpaque(int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_imgWidth || y >= m_imgHeight) return false;
	if (m_alphaMask.empty()) return false;
	return m_alphaMask[y * m_imgWidth + x] > 10;
}

void CqieDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (!m_pBitmap) return;
	// point is client coords (display scale); map to original bitmap coords for hit test
	int origW = (int)m_pBitmap->GetWidth();
	int origH = (int)m_pBitmap->GetHeight();
	int tx = point.x;
	int ty = point.y;
	if (m_imgWidth > 0 && m_imgHeight > 0 && (m_imgWidth != origW || m_imgHeight != origH))
	{
		tx = (int)((double)point.x * origW / m_imgWidth);
		ty = (int)((double)point.y * origH / m_imgHeight);
	}
    // map tx/ty from original bitmap coords to displayed coords for mask test
	int dx = (int)((double)tx * m_imgWidth / (double)origW);
	int dy = (int)((double)ty * m_imgHeight / (double)origH);
	if (IsOpaque(dx, dy))
	{
		m_dragging = true;
		CPoint ptScreen;
		GetCursorPos(&ptScreen);
		CRect rc;
		GetWindowRect(&rc);
		m_dragOffset = ptScreen - rc.TopLeft();
		SetCapture();
	}
	CDialogEx::OnLButtonDown(nFlags, point);
}

void CqieDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		m_dragging = false;
		ReleaseCapture();
	}
	CDialogEx::OnLButtonUp(nFlags, point);
}

void CqieDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		CPoint ptScreen;
		GetCursorPos(&ptScreen);
		CPoint topleft = ptScreen - m_dragOffset;
		SetWindowPos(NULL, topleft.x, topleft.y, 0,0, SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
	}
	CDialogEx::OnMouseMove(nFlags, point);
}

void CqieDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
    RemoveTrayIcon();
	if (m_pBitmap) { delete m_pBitmap; m_pBitmap = nullptr; }
	m_alphaMask.clear();
	if (m_gdiplusToken)
	{
		GdiplusShutdown(m_gdiplusToken);
		m_gdiplusToken = 0;
	}
}

void CqieDlg::AddTrayIcon()
{
    NOTIFYICONDATA nid = {0};
	nid.cbSize = sizeof(nid);
	nid.hWnd = m_hWnd;
	nid.uID = m_trayID;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = m_hIcon;
	wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(WCHAR), L"企鹅桌宠");
	Shell_NotifyIcon(NIM_ADD, &nid);
	m_trayAdded = true;
}

void CqieDlg::RemoveTrayIcon()
{
    NOTIFYICONDATA nid = {0};
	nid.cbSize = sizeof(nid);
	nid.hWnd = m_hWnd;
	nid.uID = m_trayID;
	Shell_NotifyIcon(NIM_DELETE, &nid);
	m_trayAdded = false;
}

void CqieDlg::OnRButtonUp(UINT nFlags, CPoint point)
{
	// right-click on opaque pixel shows menu
	if (!m_pBitmap) return;
	int origW = (int)m_pBitmap->GetWidth();
	int origH = (int)m_pBitmap->GetHeight();
	int tx = point.x;
	int ty = point.y;
	if (m_imgWidth > 0 && m_imgHeight > 0 && (m_imgWidth != origW || m_imgHeight != origH))
	{
		tx = (int)((double)point.x * origW / m_imgWidth);
		ty = (int)((double)point.y * origH / m_imgHeight);
	}
	int dx = (int)((double)tx * m_imgWidth / (double)origW);
	int dy = (int)((double)ty * m_imgHeight / (double)origH);
	if (!IsOpaque(dx, dy)) return;

    CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, IDC_TRAY_HIDE, L"隐藏到托盘");

	// add skins submenu if external skins found
	if (!m_skinFiles.empty())
	{
		CMenu skinMenu;
		skinMenu.CreatePopupMenu();
		for (size_t i = 0; i < m_skinFiles.size() && i < 1000; ++i)
		{
			CString full = m_skinFiles[i];
			int p = full.ReverseFind(L'\\');
			CString name = (p >= 0) ? full.Mid(p+1) : full;
			skinMenu.AppendMenu(MF_STRING, IDC_SKIN_BASE + (UINT)i, name);
		}
		menu.AppendMenu(MF_POPUP, (UINT_PTR)skinMenu.GetSafeHmenu(), L"换肤");
	}

	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, IDC_EXE_EXIT, L"退出");

	CPoint screenPt;
	GetCursorPos(&screenPt);
	SetForegroundWindow();
	menu.TrackPopupMenu(TPM_RIGHTBUTTON, screenPt.x, screenPt.y, this);

	CDialogEx::OnRButtonUp(nFlags, point);
}

LRESULT CqieDlg::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
    if ((int)wParam == m_trayID)
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)
		{
			CPoint pt;
			GetCursorPos(&pt);
            CMenu menu;
			menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, IDC_TRAY_RESTORE, L"显示");
			menu.AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);
			menu.AppendMenu(MF_STRING, IDC_EXE_EXIT, L"退出");
			SetForegroundWindow();
			menu.TrackPopupMenu(TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
		}
		else if (lParam == WM_LBUTTONDBLCLK)
		{
			OnMenuRestore();
		}
	}
	return 0;
}

void CqieDlg::OnMenuHideTray()
{
	ShowWindow(SW_HIDE);
	AddTrayIcon();
}

void CqieDlg::OnMenuRestore()
{
	RemoveTrayIcon();
	ShowWindow(SW_SHOW);
	SetForegroundWindow();
}

void CqieDlg::OnMenuExit()
{
	PostMessage(WM_CLOSE);
}

void CqieDlg::OnSkinChange(UINT nID)
{
	int idx = nID - IDC_SKIN_BASE;
	if (idx < 0 || idx >= (int)m_skinFiles.size()) return;
	CString path = m_skinFiles[idx];

	// load selected PNG into m_pBitmap
	// free old
	if (m_pBitmap) { delete m_pBitmap; m_pBitmap = nullptr; }

	// open file as IStream
	FILE* f = nullptr;
	_wfopen_s(&f, path, L"rb");
	if (!f) return;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
	if (!hMem) { fclose(f); return; }
	void* p = GlobalLock(hMem);
	if (p) fread(p, 1, size, f);
	GlobalUnlock(hMem);
	fclose(f);

	IStream* pStream = nullptr;
	if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)) && pStream)
	{
		LARGE_INTEGER li = {0};
		pStream->Seek(li, STREAM_SEEK_SET, NULL);
		m_pBitmap = Bitmap::FromStream(pStream);
		pStream->Release();
	}

	if (m_pBitmap && m_pBitmap->GetLastStatus() == Ok)
	{
        int origW = (int)m_pBitmap->GetWidth();
		int origH = (int)m_pBitmap->GetHeight();
		const int MAX_DIM = SKIN_DISPLAY_MAX;
		double scale = 1.0;
		if (origW > MAX_DIM || origH > MAX_DIM)
		{
			double sx = (double)MAX_DIM / origW;
			double sy = (double)MAX_DIM / origH;
			scale = min(sx, sy);
		}
		const double FORCE_FACTOR = 1.0/3.0;
		scale = min(scale, FORCE_FACTOR);
		m_imgWidth = max(1, (int)floor(origW * scale + 0.5));
		m_imgHeight = max(1, (int)floor(origH * scale + 0.5));
		SetWindowPos(nullptr, 0,0, m_imgWidth, m_imgHeight, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		ShowLayered();
	}
}

void CqieDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CqieDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CqieDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

