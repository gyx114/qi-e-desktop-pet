
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
#include <cstdlib>
#include <ctime>
#include <string>
// for CreateStreamOnHGlobal / IStream
#include <objidl.h>
#include <vector>
#include <deque>
#include <utility>
#include <shellapi.h>
#include <shobjidl.h>

// PromptForText: create a standard modal dialog (built from DLGTEMPLATE in memory)
struct InputDlgParam { HWND owner; CString title; CString prompt; CString init; CString result; BOOL ok; };

static INT_PTR CALLBACK QieInputDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	InputDlgParam* p = (InputDlgParam*)GetWindowLongPtr(hDlg, DWLP_USER);
	switch (message)
	{
	case WM_INITDIALOG:
		p = (InputDlgParam*)lParam;
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		// set texts
		SetWindowTextW(hDlg, (LPCWSTR)p->title);
		SetDlgItemTextW(hDlg, 100, (LPCWSTR)p->prompt);
		SetDlgItemTextW(hDlg, 101, (LPCWSTR)p->init);
		// center over owner
		if (p->owner)
		{
			RECT rcOwner; GetWindowRect(p->owner, &rcOwner);
			RECT rc; GetWindowRect(hDlg, &rc);
			int x = (rcOwner.left + rcOwner.right - (rc.right-rc.left)) / 2;
			int y = (rcOwner.top + rcOwner.bottom - (rc.bottom-rc.top)) / 2;
			SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			wchar_t buf[1024];
			GetDlgItemTextW(hDlg, 101, buf, _countof(buf));
			InputDlgParam* pp = (InputDlgParam*)GetWindowLongPtr(hDlg, DWLP_USER);
			pp->result = buf;
			pp->ok = TRUE;
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			InputDlgParam* pp = (InputDlgParam*)GetWindowLongPtr(hDlg, DWLP_USER);
			pp->ok = FALSE;
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static bool PromptForText(HWND owner, const CString& title, const CString& prompt, const CString& init, CString& out)
{
	// build a simple dialog template in memory
	const int DLG_W = 380, DLG_H = 140;
	// Allocate buffer
	const int BUF_SIZE = 1024;
	std::vector<BYTE> buf(BUF_SIZE);
	BYTE* p = buf.data();
	// align helper
	auto align = [&](size_t a){ size_t off = (size_t)(p - buf.data()); size_t need = (a - (off % a)) % a; for (size_t i=0;i<need;i++) *p++ = 0; };

	// DLGTEMPLATE
	align(4);
	DLGTEMPLATE* dt = (DLGTEMPLATE*)p; p += sizeof(DLGTEMPLATE);
	dt->style = DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_MODALFRAME;
	dt->dwExtendedStyle = 0;
	dt->cdit = 4; // static, edit, ok, cancel
	dt->x = 0; dt->y = 0; dt->cx = DLG_W; dt->cy = DLG_H;
	// no menu
	*((WORD*)p) = 0; p += sizeof(WORD);
	// default window class
	*((WORD*)p) = 0; p += sizeof(WORD);
	// title
	size_t tlen = (title.GetLength()+1);
	memcpy(p, title.GetString(), tlen * sizeof(wchar_t)); p += tlen * sizeof(wchar_t);
	// font
	*((WORD*)p) = 8; p += sizeof(WORD);
	// now DLGITEMTEMPLATEs
	// control 1: STATIC (prompt) id 100
	align(4);
	DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p; p += sizeof(DLGITEMTEMPLATE);
	it->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
	it->dwExtendedStyle = 0;
	it->x = 8; it->y = 8; it->cx = DLG_W - 16; it->cy = 20;
	it->id = 100;
	// class: STATIC
	*((WORD*)p) = 0xFFFF; p += sizeof(WORD); *((WORD*)p) = 0x0082; p += sizeof(WORD); // 0x0082 = STATIC
	// title (prompt)
	size_t plen = (prompt.GetLength()+1);
	memcpy(p, prompt.GetString(), plen * sizeof(wchar_t)); p += plen * sizeof(wchar_t);
	*((WORD*)p) = 0; p += sizeof(WORD);

	// control 2: EDIT (id 101)
	align(4);
	it = (DLGITEMTEMPLATE*)p; p += sizeof(DLGITEMTEMPLATE);
	it->style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL;
	it->dwExtendedStyle = 0;
	it->x = 8; it->y = 34; it->cx = DLG_W - 16; it->cy = 22;
	it->id = 101;
	// class: EDIT
	*((WORD*)p) = 0xFFFF; p += sizeof(WORD); *((WORD*)p) = 0x0081; p += sizeof(WORD); // 0x0081 = EDIT
	// no title text
	*((WORD*)p) = 0; p += sizeof(WORD);
	*((WORD*)p) = 0; p += sizeof(WORD);

	// control 3: OK button id IDOK
	align(4);
	it = (DLGITEMTEMPLATE*)p; p += sizeof(DLGITEMTEMPLATE);
	it->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
	it->dwExtendedStyle = 0;
	it->x = DLG_W/2 - 80; it->y = DLG_H - 44; it->cx = 70; it->cy = 24;
	it->id = IDOK;
	// class: BUTTON
	*((WORD*)p) = 0xFFFF; p += sizeof(WORD); *((WORD*)p) = 0x0080; p += sizeof(WORD); // 0x0080 = BUTTON
	// title
	const wchar_t* okText = L"确定";
	memcpy(p, okText, (wcslen(okText)+1)*sizeof(wchar_t)); p += (wcslen(okText)+1)*sizeof(wchar_t);
	*((WORD*)p) = 0; p += sizeof(WORD);

	// control 4: Cancel button id IDCANCEL
	align(4);
	it = (DLGITEMTEMPLATE*)p; p += sizeof(DLGITEMTEMPLATE);
	it->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	it->dwExtendedStyle = 0;
	it->x = DLG_W/2 + 10; it->y = DLG_H - 44; it->cx = 70; it->cy = 24;
	it->id = IDCANCEL;
	// class: BUTTON
	*((WORD*)p) = 0xFFFF; p += sizeof(WORD); *((WORD*)p) = 0x0080; p += sizeof(WORD);
	const wchar_t* cancelText = L"取消";
	memcpy(p, cancelText, (wcslen(cancelText)+1)*sizeof(wchar_t)); p += (wcslen(cancelText)+1)*sizeof(wchar_t);
	*((WORD*)p) = 0; p += sizeof(WORD);

	// prepare param
	InputDlgParam param = {}; param.owner = owner; param.title = title; param.prompt = prompt; param.init = init; param.ok = FALSE;

	BOOL rc = FALSE;
	INT_PTR res = DialogBoxIndirectParamW(AfxGetInstanceHandle(), (LPCDLGTEMPLATEW)buf.data(), owner, QieInputDlgProc, (LPARAM)&param);
	if (res == IDOK)
	{
		out = param.result;
		return true;
	}
	return false;
}


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

void CqieDlg::OnDeleteProgramSelected(UINT nID)
{
	int idx = nID - IDC_PROG_DELETE_BASE;
	LoadProgramsFromSettings();
	if (idx < 0 || idx >= (int)m_programs.size()) return;
	// confirm
	CString msg;
    // display friendly name if present
	CString stored = m_programs[idx];
	CString display = stored;
	int sep = stored.Find(L"||");
	if (sep >= 0)
		display = stored.Left(sep);
	else
	{
		// derive from path
		display = stored;
		if (!display.IsEmpty() && (display.Right(1) == L"\\" || display.Right(1) == L"/"))
			display = display.Left(display.GetLength() - 1);
		int pos = display.ReverseFind(L'\\');
		if (pos < 0) pos = display.ReverseFind(L'/');
		if (pos >= 0) display = display.Mid(pos + 1);
	}
	msg.Format(L"确定要删除 '%s' 吗？", (LPCWSTR)display);
    if (MessageBoxW(msg, L"移除程序", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
	m_programs.erase(m_programs.begin() + idx);
	SaveProgramsToSettings();
}

void CqieDlg::OnRenameProgramSelected(UINT nID)
{
	int idx = nID - IDC_PROG_RENAME_BASE;
	LoadProgramsFromSettings();
	if (idx < 0 || idx >= (int)m_programs.size()) return;
    CString stored = m_programs[idx];
	CString pathPart = stored;
	CString label;
	int sep = stored.Find(L"||");
	if (sep >= 0)
	{
		label = stored.Left(sep);
		pathPart = stored.Mid(sep + 2);
	}
    // prompt for new label
	CString prompt = L"输入新的昵称：";
	CString newLabel;
	TRACE("OnRenameProgramSelected called nID=%u, idx=%d, programs=%d\n", nID, idx, (int)m_programs.size());
	// Use NULL owner for the modal input dialog to avoid issues with layered/topmost
	// windows preventing the dialog from appearing in some cases.
	if (!PromptForText(NULL, L"修改昵称", prompt, label.IsEmpty() ? pathPart : label, newLabel)) { TRACE("PromptForText returned false\n"); return; }
	newLabel.Trim();
	if (newLabel.IsEmpty()) return;
	// store as label||path to preserve both
	CString combined = newLabel + L"||" + pathPart;
	m_programs[idx] = combined;
	SaveProgramsToSettings();
}

void CqieDlg::LoadProgramsFromSettings()
{
	m_programs.clear();
	if (m_settingsPath.IsEmpty()) return;
	WCHAR buf[4096];
	// programs are stored under [Programs] as indexed keys 0..N-1
	for (int i = 0; i < 100; ++i)
	{
		WCHAR key[32]; swprintf_s(key, L"%d", i);
		DWORD n = GetPrivateProfileStringW(L"Programs", key, L"", buf, _countof(buf), m_settingsPath);
		if (n == 0) break;
		m_programs.push_back(CString(buf));
	}
}

void CqieDlg::SaveProgramsToSettings()
{
	if (m_settingsPath.IsEmpty()) return;
	// clear section first
	WritePrivateProfileSectionW(L"Programs", L"", m_settingsPath);
	for (size_t i = 0; i < m_programs.size(); ++i)
	{
		WCHAR key[32]; swprintf_s(key, L"%d", (int)i);
		WritePrivateProfileStringW(L"Programs", key, m_programs[i], m_settingsPath);
	}
}

void CqieDlg::OnAddProgram()
{
    // Ask whether to add File or Folder
	int r = MessageBoxW(L"添加文件还是文件夹？\nYes = 文件, No = 文件夹", L"添加程序", MB_YESNOCANCEL | MB_ICONQUESTION);
	if (r == IDCANCEL) return;

	CString chosen;
	if (r == IDYES)
	{
		// file picker
		WCHAR file[MAX_PATH] = {0};
		OPENFILENAMEW ofn = {0};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = m_hWnd;
		ofn.lpstrFile = file;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
		ofn.lpstrFilter = L"All\0*.*\0Executables\0*.exe\0\0";
		if (!GetOpenFileNameW(&ofn)) return;
		chosen = file;
	}
	else
	{
		// folder picker using IFileDialog
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		bool coInit = SUCCEEDED(hr);
		IFileDialog *pfd = NULL;
		if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
		{
			DWORD opts;
			if (SUCCEEDED(pfd->GetOptions(&opts)))
			{
				pfd->SetOptions(opts | FOS_PICKFOLDERS);
			}
			if (SUCCEEDED(pfd->Show(m_hWnd)))
			{
				IShellItem *psi = NULL;
				if (SUCCEEDED(pfd->GetResult(&psi)) && psi)
				{
					PWSTR pszPath = NULL;
					if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath)
					{
						chosen = pszPath;
						CoTaskMemFree(pszPath);
					}
					psi->Release();
				}
			}
			pfd->Release();
		}
		if (coInit) CoUninitialize();
		if (chosen.IsEmpty()) return;
	}

	// normalize chosen path
	WCHAR full[MAX_PATH];
	if (GetFullPathNameW((LPCWSTR)chosen, MAX_PATH, full, NULL) == 0) return;
	CString finalPath(full);
	// remove trailing backslash for directories
	if (finalPath.GetLength() > 1 && (finalPath.Right(1) == L"\\" || finalPath.Right(1) == L"/"))
		finalPath = finalPath.Left(finalPath.GetLength() - 1);

	// check duplicates
	LoadProgramsFromSettings();
	for (auto &p : m_programs)
	{
		CString pp = p;
		// normalize stored path similarly
		WCHAR f2[MAX_PATH];
		if (GetFullPathNameW((LPCWSTR)pp, MAX_PATH, f2, NULL) != 0)
		{
			CString stored(f2);
			if (stored.GetLength() > 1 && (stored.Right(1) == L"\\" || stored.Right(1) == L"/"))
				stored = stored.Left(stored.GetLength() - 1);
			if (stored.CompareNoCase(finalPath) == 0) return; // duplicate
		}
		else if (pp.CompareNoCase(finalPath) == 0) return;
	}

	m_programs.push_back(finalPath);
	SaveProgramsToSettings();
}

void CqieDlg::OnOpenProgram(UINT nID)
{
	int idx = nID - IDC_PROG_OPEN_BASE;
	LoadProgramsFromSettings();
	if (idx < 0 || idx >= (int)m_programs.size()) return;
    CString stored = m_programs[idx];
	CString path = stored;
	int sep = stored.Find(L"||");
	if (sep >= 0)
		path = stored.Mid(sep + 2);
	// open file or directory
	SHELLEXECUTEINFOW sei = {0};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOASYNC;
	sei.hwnd = m_hWnd;
	sei.lpVerb = L"open";
	sei.lpFile = path;
	sei.nShow = SW_SHOWNORMAL;
	ShellExecuteExW(&sei);
}

void CqieDlg::LoadSettings()
{
	// default: nothing yet; create file with defaults if missing
	if (m_settingsPath.IsEmpty()) return;
	DWORD attrs = GetFileAttributesW(m_settingsPath);
	if (attrs == INVALID_FILE_ATTRIBUTES)
	{
		SaveDefaultSettings();
		return;
	}
	// example: read skins dir override
	WCHAR buf[MAX_PATH];
	DWORD n = GetPrivateProfileStringW(L"Paths", L"SkinsDir", L"", buf, MAX_PATH, m_settingsPath);
	if (n > 0)
	{
		m_skinsDir = CString(buf);
	}
}

void CqieDlg::SaveDefaultSettings()
{
	if (m_settingsPath.IsEmpty()) return;
	// write a minimal INI with current skins dir
	WritePrivateProfileStringW(L"Paths", L"SkinsDir", m_skinsDir.IsEmpty() ? NULL : (LPCWSTR)m_skinsDir, m_settingsPath);
	// add other defaults if needed
}

void CqieDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// simulate Ctrl+Alt+Space
	// send input sequence: keydown Ctrl, keydown Alt, keydown Space, keyup Space, keyup Alt, keyup Ctrl
	INPUT inputs[6];
	ZeroMemory(inputs, sizeof(inputs));
	// Ctrl down
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_CONTROL;
	// Alt down
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_MENU;
	// Space down
	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = VK_SPACE;
	// Space up
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = VK_SPACE;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	// Alt up
	inputs[4].type = INPUT_KEYBOARD;
	inputs[4].ki.wVk = VK_MENU;
	inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
	// Ctrl up
	inputs[5].type = INPUT_KEYBOARD;
	inputs[5].ki.wVk = VK_CONTROL;
	inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

	// send
	SendInput(_countof(inputs), inputs, sizeof(INPUT));

	CDialogEx::OnLButtonDblClk(nFlags, point);
}

void CqieDlg::SetAutoStart(bool enable)
{
	if (enable)
	{
		WCHAR exePath[MAX_PATH];
		if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
		{
			HKEY hk;
			if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hk) == ERROR_SUCCESS)
			{
				RegSetValueExW(hk, L"qi-e", 0, REG_SZ, (const BYTE*)exePath, (DWORD)((wcslen(exePath)+1)*sizeof(WCHAR)));
				RegCloseKey(hk);
			}
		}
	}
	else
	{
		HKEY hk;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hk) == ERROR_SUCCESS)
		{
			RegDeleteValueW(hk, L"qi-e");
			RegCloseKey(hk);
		}
	}
}

void CqieDlg::CheckForShake()
{
    // Improved shake detection tuned for human-like shakes:
	// - requires enough samples
	// - requires several rapid left/right reversals (frequency)
	// - requires sufficient average horizontal velocity (to ignore slow moves)
	// - allows some vertical movement

	const size_t MIN_SAMPLES = 6;
	const double VEL_THRESHOLD = 0.18; // px per ms (~180 px/s)
	const int MIN_DIRECTION_CHANGES = 4; // require several reversals
	const double MIN_FREQ_HZ = 1.8; // minimum oscillation frequency
	const int MAX_TOTAL_MOVE = 1000; // if user drags across huge distance, ignore

	if (m_moveHistory.size() < MIN_SAMPLES) return;

	// extract times and x positions
	std::vector<ULONGLONG> times;
	std::vector<int> xs;
	times.reserve(m_moveHistory.size()); xs.reserve(m_moveHistory.size());
	for (auto &p : m_moveHistory) { times.push_back(p.first); xs.push_back(p.second.x); }

	ULONGLONG t0 = times.front();
	ULONGLONG tn = times.back();
	double totalMs = (double)(tn - t0);
	if (totalMs < 50.0) return; // too short to analyze

	// compute per-sample horizontal deltas and velocities
	int dirChanges = 0;
	int prevSign = 0;
	double totalAbsDx = 0.0;
	double peakV = 0.0;
	for (size_t i = 1; i < xs.size(); ++i)
	{
		double dt = (double)(times[i] - times[i-1]);
		if (dt <= 0.0) continue;
		double dx = (double)(xs[i] - xs[i-1]);
		double vx = dx / dt; // px per ms
		totalAbsDx += fabs(dx);
		peakV = max(peakV, fabs(vx));
		int sign = (vx > VEL_THRESHOLD) ? 1 : ( (vx < -VEL_THRESHOLD) ? -1 : 0 );
		if (sign != 0)
		{
			if (prevSign == 0) prevSign = sign;
			else if (sign != prevSign)
			{
				++dirChanges;
				prevSign = sign;
			}
		}

	}

	double avgAbsV = (totalMs > 0.0) ? (totalAbsDx / totalMs) : 0.0; // px per ms

	// estimate oscillation frequency (half-cycles = dirChanges)
	double freqHz = (totalMs > 0.0) ? ( (double)dirChanges / (totalMs/1000.0) ) : 0.0;

	// ignore very large drags (user is moving window across screen)
	int netMove = abs(xs.back() - xs.front());
	if (netMove > MAX_TOTAL_MOVE) return;

	// criteria: enough direction changes, sufficient average velocity or peak velocity, and frequency
	if (dirChanges >= MIN_DIRECTION_CHANGES && (avgAbsV >= VEL_THRESHOLD || peakV >= VEL_THRESHOLD) && freqHz >= MIN_FREQ_HZ)
	{
		ULONGLONG now = GetTickCount64();
		if (now - m_lastShakeTime < 2000) return; // debounce 2s
		m_lastShakeTime = now;
		OnSkinRandom();
		m_moveHistory.clear();
	}
}

// OnTimer removed: timer-based shake animation and GWLP_USERDATA state are deleted

void CqieDlg::LoadSkinFromFile(const CString& path)
{
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

void CqieDlg::OnSkinRandom()
{
	if (m_skinFiles.empty()) return;
	// pick a random index between 1 and m_skinFiles.size() inclusive
	// assuming filenames are numeric, but we simply pick random file
	srand((unsigned)time(nullptr));
	int idx = rand() % (int)m_skinFiles.size();
	CString chosen = m_skinFiles[idx];
	LoadSkinFromFile(chosen);
}

// OnShakeRandom removed: UI-timer based shake animation and delayed load are deleted.

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
    m_lastShakeTime = 0;
	m_moveHistory.clear();
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
    ON_WM_LBUTTONDBLCLK()
    ON_MESSAGE(WM_TRAYICON, &CqieDlg::OnTrayIcon)
	ON_COMMAND(IDC_TRAY_RESTORE, &CqieDlg::OnMenuRestore)
	ON_COMMAND(IDC_TRAY_HIDE, &CqieDlg::OnMenuHideTray)
  ON_COMMAND(IDC_EXE_EXIT, &CqieDlg::OnMenuExit)
    ON_COMMAND(IDC_SKIN_RANDOM, &CqieDlg::OnSkinRandom)
    ON_COMMAND(IDC_STARTUP_TOGGLE, &CqieDlg::OnToggleStartup)
    ON_COMMAND(40100, &CqieDlg::OnAddProgram)
 ON_COMMAND_RANGE(IDC_PROG_OPEN_BASE, IDC_PROG_OPEN_BASE+1000, &CqieDlg::OnOpenProgram)
 ON_COMMAND_RANGE(IDC_PROG_DELETE_BASE, IDC_PROG_DELETE_BASE+1000, &CqieDlg::OnDeleteProgramSelected)
	ON_COMMAND_RANGE(IDC_PROG_RENAME_BASE, IDC_PROG_RENAME_BASE+1000, &CqieDlg::OnRenameProgramSelected)
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
         m_skinsDir = exeDir + L"\\skins";

		// settings.ini path
		m_settingsPath = exeDir + L"\\settings.ini";
		LoadSettings();
		WIN32_FIND_DATA fd;
        CString pattern = m_skinsDir + L"\\*.png";
		HANDLE h = FindFirstFile(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
                CString f = m_skinsDir + L"\\" + fd.cFileName;
				m_skinFiles.push_back(f);
			} while (FindNextFile(h, &fd));
			FindClose(h);
		}
	}

	// determine first run / default autostart: if Run key doesn't contain our entry, enable it and mark firstRun
	bool firstRun = false;
	HKEY hKeyTest = NULL;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKeyTest) == ERROR_SUCCESS)
	{
		WCHAR val[MAX_PATH]; DWORD cb = sizeof(val);
		if (RegQueryValueExW(hKeyTest, L"qi-e", NULL, NULL, (LPBYTE)val, &cb) != ERROR_SUCCESS)
		{
			firstRun = true;
		}
		RegCloseKey(hKeyTest);
	}
	else
	{
		firstRun = true;
	}
	if (firstRun)
	{
		SetAutoStart(true);
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
 ex |= WS_EX_LAYERED | WS_EX_TOOLWINDOW;
	SetWindowLong(m_hWnd, GWL_EXSTYLE, ex);

	// Remove caption and borders so layered window has only client area (no title bar)
	LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
	style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_BORDER);
	style |= WS_POPUP;
	SetWindowLong(m_hWnd, GWL_STYLE, style);
    SetWindowPos(NULL, 0,0,0,0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	// ensure window is topmost
	::SetWindowPos(m_hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

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

                        // Resize window to image size and position at right-middle of primary screen on first show
						int scrW = GetSystemMetrics(SM_CXSCREEN);
						int scrH = GetSystemMetrics(SM_CYSCREEN);
						const int RIGHT_MARGIN = 16;
						int posX = max(0, scrW - m_imgWidth - RIGHT_MARGIN);
						int posY = max(0, (scrH - m_imgHeight) / 2);
						SetWindowPos(nullptr, posX, posY, m_imgWidth, m_imgHeight, SWP_NOZORDER | SWP_NOACTIVATE);
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
	// after drag ends, record position and check for shake
	CheckForShake();
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
	// while dragging, log positions for shake detection
	if (m_dragging)
	{
		ULONGLONG now = GetTickCount64();
		CPoint tl;
		CRect rc; GetWindowRect(&rc); tl = rc.TopLeft();
		// push to history
		m_moveHistory.emplace_back(now, tl);
		// prune older than 800ms
		while (!m_moveHistory.empty() && now - m_moveHistory.front().first > 800)
			m_moveHistory.pop_front();

		// check for shake gesture while dragging (trigger immediate random skin)
		CheckForShake();
	}
}

void CqieDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
    RemoveTrayIcon();
	if (m_pBitmap) { delete m_pBitmap; m_pBitmap = nullptr; }
	m_alphaMask.clear();
    m_moveHistory.clear();
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
    // random skin entry
	if (!m_skinFiles.empty())
	{
		menu.AppendMenu(MF_STRING, IDC_SKIN_RANDOM, L"换肤");
	}

	// Programs submenu
	CMenu progMenu;
	progMenu.CreatePopupMenu();
	// add existing programs from settings
	LoadProgramsFromSettings();
  for (size_t i = 0; i < m_programs.size(); ++i)
	{
		CString stored = m_programs[i];
		CString label;
		CString pathPart = stored;
		int sep = stored.Find(L"||");
		if (sep >= 0)
		{
			label = stored.Left(sep);
			pathPart = stored.Mid(sep + 2);
		}
		if (label.IsEmpty())
		{
			// derive label from path
			CString display = pathPart;
			if (!display.IsEmpty() && (display.Right(1) == L"\\" || display.Right(1) == L"/"))
				display = display.Left(display.GetLength() - 1);
			int pos = display.ReverseFind(L'\\');
			if (pos < 0) pos = display.ReverseFind(L'/');
			if (pos >= 0)
				display = display.Mid(pos + 1);
			label = display;
		}

        // create submenu for this program: Open / Modify Nickname / Remove
		CMenu itemSub;
		itemSub.CreatePopupMenu();
        itemSub.AppendMenu(MF_STRING, IDC_PROG_OPEN_BASE + (UINT)i, L"打开");
		itemSub.AppendMenu(MF_STRING, IDC_PROG_RENAME_BASE + (UINT)i, L"修改昵称");
		itemSub.AppendMenu(MF_STRING, IDC_PROG_DELETE_BASE + (UINT)i, L"移除");
        // attach submenu under the program label (prefix index to avoid ambiguous first-item behavior)
       CString displayLabel = label;
		progMenu.AppendMenu(MF_POPUP, (UINT_PTR)itemSub.Detach(), displayLabel);
	}
	// add separator and Add... entry
	progMenu.AppendMenu(MF_SEPARATOR);
	progMenu.AppendMenu(MF_STRING, 40100, L"添加...");
	menu.AppendMenu(MF_POPUP, (UINT_PTR)progMenu.Detach(), L"打开");

	// startup toggle
	BOOL autoRun = FALSE;
	HKEY hKey = NULL;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		WCHAR exePath[MAX_PATH];
		if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
		{
			WCHAR val[MAX_PATH];
			DWORD cb = sizeof(val);
			if (RegQueryValueExW(hKey, L"qi-e", NULL, NULL, (LPBYTE)val, &cb) == ERROR_SUCCESS)
				autoRun = TRUE;
		}
		RegCloseKey(hKey);
	}
	menu.AppendMenu(autoRun ? MF_STRING : MF_STRING, 40010, autoRun ? L"取消开机自启动" : L"开机自启动");

    // skins listing removed from menu (logic retained)

	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, IDC_EXE_EXIT, L"退出");

	CPoint screenPt;
	GetCursorPos(&screenPt);
    SetForegroundWindow();
	// use TrackPopupMenu with TPM_RETURNCMD to get selected command directly
	HMENU hMenu = menu.GetSafeHmenu();
	UINT cmd = ::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, 0, m_hWnd, NULL);
	if (cmd != 0)
	{
		// dispatch known commands
		if (cmd == IDC_TRAY_HIDE) OnMenuHideTray();
		else if (cmd == IDC_SKIN_RANDOM) OnSkinRandom();
		else if (cmd >= IDC_PROG_OPEN_BASE && cmd < IDC_PROG_OPEN_BASE + 1000) OnOpenProgram(cmd);
		else if (cmd >= IDC_PROG_RENAME_BASE && cmd < IDC_PROG_RENAME_BASE + 1000) OnRenameProgramSelected(cmd);
		else if (cmd >= IDC_PROG_DELETE_BASE && cmd < IDC_PROG_DELETE_BASE + 1000) OnDeleteProgramSelected(cmd);
		else if (cmd == 40100) OnAddProgram();
		else if (cmd == 40010) OnToggleStartup();
		else if (cmd == IDC_EXE_EXIT) OnMenuExit();
		// other commands ignored
	}

	CDialogEx::OnRButtonUp(nFlags, point);
}

void CqieDlg::OnToggleStartup()
{
	// read current state
	HKEY hKey = NULL;
	bool enabled = false;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		WCHAR val[MAX_PATH]; DWORD cb = sizeof(val);
		if (RegQueryValueExW(hKey, L"qi-e", NULL, NULL, (LPBYTE)val, &cb) == ERROR_SUCCESS)
			enabled = true;
		RegCloseKey(hKey);
	}

	// toggle
	if (!enabled)
	{
		WCHAR exePath[MAX_PATH];
		if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
		{
			HKEY hk;
			if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hk) == ERROR_SUCCESS)
			{
				RegSetValueExW(hk, L"qi-e", 0, REG_SZ, (const BYTE*)exePath, (DWORD)((wcslen(exePath)+1)*sizeof(WCHAR)));
				RegCloseKey(hk);
			}
		}
	}
	else
	{
		HKEY hk;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hk) == ERROR_SUCCESS)
		{
			RegDeleteValueW(hk, L"qi-e");
			RegCloseKey(hk);
		}
	}
}

LRESULT CqieDlg::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
    if ((int)wParam == m_trayID)
	{
		// show tray menu only on right-click
		if (lParam == WM_RBUTTONUP)
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

