#include "StdAfx.h"
#include "MMService.h"

#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")

//////////////////////////////////////////////////////////////////////////////////

//状态信息
struct tagAstatInfo
{
	ADAPTER_STATUS					AdapterStatus;						//网卡状态
	NAME_BUFFER						NameBuff[16];						//名字缓冲
};

//////////////////////////////////////////////////////////////////////////////////

//构造函数
CMMService::CMMService()
{
}

//拷贝字符
bool CMMService::SetClipboardString(HWND hWnd, LPCTSTR pszString)
{
	//变量定义
	HANDLE hData = NULL;
	BOOL bOpenClopboard = FALSE;

	//执行逻辑
	__try
	{
		//打开拷贝
		bOpenClopboard = OpenClipboard(hWnd);
		if (bOpenClopboard == FALSE) __leave;

		//清空拷贝
		if (EmptyClipboard() == FALSE) __leave;

		//申请内存
		hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, CountStringBuffer(pszString));
		if (hData == NULL) __leave;

		//复制数据
		lstrcpy((LPTSTR)GlobalLock(hData), pszString);
		GlobalUnlock(hData);

		//设置数据
#ifndef _UNICODE
		::SetClipboardData(CF_TEXT, hData);
#else
		::SetClipboardData(CF_UNICODETEXT, hData);
#endif
	}

	//终止程序
	__finally
	{
		//关闭拷贝
		if (bOpenClopboard == TRUE) CloseClipboard();

		//错误判断
		if (AbnormalTermination() == TRUE)
		{
			assert(FALSE);
		}
	}

	return true;
}

bool CMMService::SetClipboardFiles(HWND hWndOwner, std::vector<CMMString> vecFiles, bool bCopy /*= true*/)
{
	if (vecFiles.empty()) return false;

	UINT uDropEffect = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

	//effect
	HGLOBAL hGblEffect = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DWORD));
	DWORD *pDropEffect = (DWORD*)GlobalLock(hGblEffect);
	*pDropEffect = bCopy ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
	GlobalUnlock(hGblEffect);

	//len
	UINT uFileListLen = 0;
	for (auto &strFile : vecFiles)
	{
		if (strFile.empty()) continue;

		uFileListLen += (strFile.length() + 1) * sizeof(strFile[0]);
	}

	UINT uGblLen = sizeof(DROPFILES) + (uFileListLen + sizeof(TCHAR));
	HGLOBAL hGblFiles = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, uGblLen);
	BYTE *pData = (BYTE*)GlobalLock(hGblFiles);

	//drop
	DROPFILES DropFiles = {};
	DropFiles.pFiles = sizeof(DROPFILES);
	DropFiles.pt.x = 0;
	DropFiles.pt.y = 0;
	DropFiles.fNC = FALSE;
	DropFiles.fWide = TRUE;
	memcpy(pData, (LPVOID)(&DropFiles), sizeof(DROPFILES));

	//files
	BYTE *pFileList = pData + sizeof(DROPFILES);
	for (auto &strFile : vecFiles)
	{
		if (strFile.empty()) continue;

		int nSize = (strFile.length() + 1) * sizeof(strFile[0]);
		memcpy(pFileList, (BYTE*)strFile.c_str(), nSize);
		pFileList += nSize;
	}

	GlobalUnlock(hGblFiles);

	//set
	if (OpenClipboard(hWndOwner))
	{
		EmptyClipboard();

		//可以设置剪切板内容为拖动文件
		SetClipboardData(CF_HDROP, hGblFiles);

		//可以设置剪切板内容为复制或者剪切标识
		SetClipboardData(uDropEffect, hGblEffect);

		CloseClipboard();

		return true;
	}

	::GlobalFree(hGblEffect);
	::GlobalFree(hGblFiles);

	return false;
}

CMMString CMMService::GetClipboardString()
{
	CMMString strText;

	do 
	{
		bool bText = IsClipboardFormatAvailable(CF_TEXT);
		bool bUnicodeText = IsClipboardFormatAvailable(CF_UNICODETEXT);
		if (false == bText && false == bUnicodeText) break;
		if (false == OpenClipboard(NULL)) break;

		HANDLE hData = bText ? GetClipboardData(CF_TEXT) : GetClipboardData(CF_UNICODETEXT);
		if (NULL == hData)
		{
			CloseClipboard();

			break;
		}
		if (bText)
		{
			strText = (LPCSTR)GlobalLock(hData);
		}
		else
		{
			strText = (LPCTSTR)GlobalLock(hData);
		}
		
		GlobalUnlock(hData);
		CloseClipboard();

	} while (false);

	return strText;
}

std::vector<CMMString> CMMService::GetClipboardFile()
{
	std::vector<CMMString> vecFile;

	do
	{
		bool bDrop = IsClipboardFormatAvailable(CF_HDROP);
		if (false == bDrop) break;
		if (false == OpenClipboard(NULL)) break;

		HDROP hDropInfo = (HDROP)GetClipboardData(CF_HDROP);
		if (NULL == hDropInfo)
		{
			CloseClipboard();

			break;
		}
		
		UINT nFilesCount = DragQueryFile(hDropInfo, INFINITE, NULL, 0);
		for (UINT i = 0; i < nFilesCount; i++)
		{
			TCHAR szFile[MAX_PATH + 1] = {};
			DragQueryFile(hDropInfo, i, szFile, MMCountString(szFile));
			if (false == PathFileExists(szFile)) continue;

			vecFile.push_back(szFile);
		}

		CloseClipboard();

	} while (false);

	return vecFile;
}

DROPEFFECT CMMService::GetClipboardDropEffect()
{
	DROPEFFECT DropEffect = 0;

	do
	{
		if (false == OpenClipboard(NULL)) break;

		auto ClipFormat = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
		HANDLE hData = GetClipboardData(ClipFormat);
		if (NULL == hData)
		{
			CloseClipboard();

			break;
		}

		DropEffect = *(DROPEFFECT*)GlobalLock(hData);

		GlobalUnlock(hData);
		CloseClipboard();

	} while (false);

	return DropEffect;
}

int CMMService::GetCompileDate()
{
	char sMonth[5] = "";
	int nMonth, nDay, nYear;
	char sMonthName[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	char sTime[20] = __DATE__;
	sscanf(sTime, "%s %d %d", sMonth, &nDay, &nYear);

	nMonth = (strstr(sMonthName, sMonth) - sMonthName) / 3 + 1;

	return  nYear * 10000 + nMonth * 100 + nDay;
}

//IP翻译
CMMString CMMService::IPAddrToString(DWORD dwIPAddr)
{
	struct in_addr in;
	memcpy((char*)&in, (char*)&dwIPAddr, 4);

	return CA2T(inet_ntoa(in));
}

DWORD CMMService::StringToIPAddr(CMMString strIPAddr)
{
	DWORD dwIPAddr = inet_addr((LPCSTR)CT2CA(strIPAddr));
	if (INADDR_NONE == dwIPAddr)
	{
		LPHOSTENT lpHost = gethostbyname((LPCSTR)CT2CA(strIPAddr));
		if (NULL == lpHost) return INADDR_NONE;

		dwIPAddr = ((LPIN_ADDR)lpHost->h_addr)->s_addr;
	}

	return dwIPAddr;
}

//机器标识
bool CMMService::GetMachineID(TCHAR szMachineID[Len_Machine_ID])
{
	GetImportIDEx(szMachineID);
	if (_T('0') != szMachineID[0]) return true;

	//网卡标识
	TCHAR szMACAddress[Len_Network_ID] = {};
	GetMACAddress(szMACAddress);

	//转换信息
	CMMString strMacAddr = CMMMD5Checksum::GetMD5((BYTE*)szMACAddress, sizeof(szMACAddress));
	lstrcpyn(szMachineID, strMacAddr, MMCountString(szMachineID));

	return true;
}

CMMString CMMService::GetMachineID()
{
	TCHAR szMachineID[Len_Machine_ID] = {};
	GetMachineID(szMachineID);

	return szMachineID;
}

//网卡地址
bool CMMService::GetMACAddress(TCHAR szMACAddress[Len_Network_ID])
{
	//变量定义
	HINSTANCE hInstance = NULL;

	//执行逻辑
	__try
	{
		//加载 DLL
		hInstance = LoadLibrary(_T("NetApi32.dll"));
		if (hInstance == NULL) __leave;

		//获取函数
		typedef BYTE __stdcall NetBiosProc(NCB * Ncb);
		NetBiosProc * pNetBiosProc = (NetBiosProc *)GetProcAddress(hInstance, "Netbios");
		if (pNetBiosProc == NULL) __leave;

		//变量定义
		NCB Ncb;
		LANA_ENUM LanaEnum;
		ZeroMemory(&Ncb, sizeof(Ncb));
		ZeroMemory(&LanaEnum, sizeof(LanaEnum));

		//枚举网卡
		Ncb.ncb_command = NCBENUM;
		Ncb.ncb_length = sizeof(LanaEnum);
		Ncb.ncb_buffer = (BYTE *)&LanaEnum;
		if ((pNetBiosProc(&Ncb) != NRC_GOODRET) || (LanaEnum.length == 0)) __leave;

		//获取地址
		if (LanaEnum.length > 0)
		{
			//变量定义
			tagAstatInfo Adapter;
			ZeroMemory(&Adapter, sizeof(Adapter));

			//重置网卡
			Ncb.ncb_command = NCBRESET;
			Ncb.ncb_lana_num = LanaEnum.lana[0];
			if (pNetBiosProc(&Ncb) != NRC_GOODRET) __leave;

			//获取状态
			Ncb.ncb_command = NCBASTAT;
			Ncb.ncb_length = sizeof(Adapter);
			Ncb.ncb_buffer = (BYTE *)&Adapter;
			Ncb.ncb_lana_num = LanaEnum.lana[0];
			strcpy((char *)Ncb.ncb_callname, "*");
			if (pNetBiosProc(&Ncb) != NRC_GOODRET) __leave;

			//获取地址
			for (INT i = 0; i < 6; i++)
			{
				assert((i * 2) < Len_Network_ID);
				_stprintf(&szMACAddress[i * 2], _T("%02X"), Adapter.AdapterStatus.adapter_address[i]);
			}
		}
	}

	//结束清理
	__finally
	{
		//释放资源
		if (hInstance != NULL)
		{
			FreeLibrary(hInstance);
			hInstance = NULL;
		}

		//错误断言
		if (AbnormalTermination() == TRUE)
		{
			assert(FALSE);
		}
	}

	return true;
}

//@szSystemInfo	len=4096		// 在程序执行完毕后，此处存储取得的系统特征码
//@uSystemInfoLen= 0;			// 在程序执行完毕后，此处存储取得的系统特征码的长度
//网卡 MAC 地址，注意: MAC 地址是可以在注册表中修改的
BOOL CMMService::GetMacAddr(BYTE* szSystemInfo, UINT uSystemInfoLen)
{
	UINT uErrorCode = 0;
	IP_ADAPTER_INFO iai;
	ULONG uSize = 0;
	DWORD dwResult = GetAdaptersInfo(&iai, &uSize);
	if (dwResult == ERROR_BUFFER_OVERFLOW)
	{
		IP_ADAPTER_INFO* piai = (IP_ADAPTER_INFO*)HeapAlloc(GetProcessHeap(), 0, uSize);
		if (piai != NULL)
		{
			dwResult = GetAdaptersInfo(piai, &uSize);
			if (ERROR_SUCCESS == dwResult)
			{
				IP_ADAPTER_INFO* piai2 = piai;
				while (piai2 != NULL && (uSystemInfoLen + piai2->AddressLength) < 4096U)
				{
					CopyMemory(szSystemInfo + uSystemInfoLen, piai2->Address, piai2->AddressLength);
					uSystemInfoLen += piai2->AddressLength;
					piai2 = piai2->Next;
				}
			}
			else
			{
				uErrorCode = 0xF0000000U + dwResult;
			}
			assert(HeapFree(GetProcessHeap(), 0, piai));
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		uErrorCode = 0xE0000000U + dwResult;
	}
	if (uErrorCode != 0U)
	{
		return FALSE;
	}
	return TRUE;
}

// 硬盘序列号，注意：有的硬盘没有序列号
BOOL CMMService::GetHdiskSerial(BYTE* szSystemInfo, UINT uSystemInfoLen)
{
	OSVERSIONINFOEX ovi = {};
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	CMMVersion::GetOSVersionInfo(&ovi);

	if (ovi.dwPlatformId != VER_PLATFORM_WIN32_NT)
	{
		// Only Windows 2000, Windows XP, Windows Server 2003...
		return FALSE;
	}
	else
	{
		if (!WinNTHDSerialNumAsPhysicalRead(szSystemInfo, &uSystemInfoLen, 1024))
		{
			WinNTHDSerialNumAsScsiRead(szSystemInfo, &uSystemInfoLen, 1024);
		}
	}
	return TRUE;
}

// CPU ID
void CMMService::GetCpuID(BYTE* szSystemInfo, UINT uSystemInfoLen)
// CPU ID
{
	BOOL bException = FALSE;
	BYTE szCpu[16] = {};
	UINT uCpuID = 0U;

	__try
	{
#ifndef _WIN64
		_asm
		{
			mov eax, 0
			cpuid
			mov dword ptr szCpu[0], ebx
			mov dword ptr szCpu[4], edx
			mov dword ptr szCpu[8], ecx
			mov eax, 1
			cpuid
			mov uCpuID, edx
		}
#endif
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		bException = TRUE;
	}

	if (!bException)
	{
		CopyMemory(szSystemInfo + uSystemInfoLen, &uCpuID, sizeof(UINT));
		uSystemInfoLen += sizeof(UINT);

		uCpuID = strlen((char*)szCpu);
		CopyMemory(szSystemInfo + uSystemInfoLen, szCpu, uCpuID);
		uSystemInfoLen += uCpuID;
	}
}

BOOL CMMService::DoIdentify(HANDLE hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
	PSENDCMDOUTPARAMS pSCOP, BYTE bIDCmd, BYTE bDriveNum,
	PDWORD lpcbBytesReturned)
{
	// Set up data structures for IDENTIFY command.
	pSCIP->cBufferSize = IDENTIFY_BUFFER_SIZE;
	pSCIP->irDriveRegs.bFeaturesReg = 0;
	pSCIP->irDriveRegs.bSectorCountReg = 1;
	pSCIP->irDriveRegs.bSectorNumberReg = 1;
	pSCIP->irDriveRegs.bCylLowReg = 0;
	pSCIP->irDriveRegs.bCylHighReg = 0;

	// calc the drive number.
	pSCIP->irDriveRegs.bDriveHeadReg = 0xA0 | ((bDriveNum & 1) << 4);

	// The command can either be IDE identify or ATAPI identify.
	pSCIP->irDriveRegs.bCommandReg = bIDCmd;
	pSCIP->bDriveNumber = bDriveNum;
	pSCIP->cBufferSize = IDENTIFY_BUFFER_SIZE;

	return DeviceIoControl(hPhysicalDriveIOCTL, DFP_RECEIVE_DRIVE_DATA,
		(LPVOID)pSCIP,
		sizeof(SENDCMDINPARAMS) - 1,
		(LPVOID)pSCOP,
		sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
		lpcbBytesReturned, NULL);
}

bool CMMService::RegisterHotKey(HWND hWnd, UINT uKeyID, WORD wHotKey)
{
	//变量定义
	BYTE cbModifiers = 0;
	if (HIBYTE(wHotKey)&HOTKEYF_ALT) cbModifiers |= MOD_ALT;
	if (HIBYTE(wHotKey)&HOTKEYF_SHIFT) cbModifiers |= MOD_SHIFT;
	if (HIBYTE(wHotKey)&HOTKEYF_CONTROL) cbModifiers |= MOD_CONTROL;

	//注册热键
	BOOL bSuccess = ::RegisterHotKey(hWnd, uKeyID, cbModifiers, LOBYTE(wHotKey));

	return (bSuccess == TRUE) ? true : false;
}

bool CMMService::UnRegisterHotKey(HWND hWnd, UINT uKeyID)
{
	//注销热键
	BOOL bSuccess = ::UnregisterHotKey(hWnd, uKeyID);

	return (bSuccess == TRUE) ? true : false;
}

//进程目录
bool CMMService::GetWorkDirectory(OUT TCHAR szWorkDirectory[], IN WORD wBufferCount)
{
	//模块路径
	TCHAR szModulePath[MAX_PATH] = _T("");
	GetModuleFileName(NULL, szModulePath, MMCountArray(szModulePath));

	//分析文件
	for (INT i = lstrlen(szModulePath); i >= 0; i--)
	{
		if (szModulePath[i] == _T('\\'))
		{
			szModulePath[i] = 0;
			break;
		}
	}

	//设置结果
	assert(szModulePath[0] != 0);
	lstrcpyn(szWorkDirectory, szModulePath, wBufferCount);

	return true;
}

CMMString CMMService::GetWorkDirectory()
{
	TCHAR szPath[MAX_PATH] = {};
	GetWorkDirectory(szPath, MMCountString(szPath));

	return szPath;
}

CMMString CMMService::GetCurrentPath()
{
	TCHAR szModulePath[MAX_PATH + 1] = _T("");
	::GetCurrentDirectory(MAX_PATH, szModulePath);

	return szModulePath;
}

CMMString CMMService::GetUserDataPath(CMMString strFolderName)
{
	TCHAR szPath[MAX_PATH + 1] = {};
	if (false == SHGetSpecialFolderPath(NULL, szPath, CSIDL_LOCAL_APPDATA, FALSE)
		&& false == SHGetSpecialFolderPath(NULL, szPath, CSIDL_APPDATA, FALSE)
		&& false == ::GetTempPath(MAX_PATH, szPath))
	{
		return CMMString();
	}

	if (false == strFolderName.empty())
	{
		PathAppend(szPath, strFolderName);
	}

	PathAddBackslash(szPath);
	SHCreateDirectory(NULL, szPath);

	return szPath;
}

CMMString CMMService::GetLocalTempPath()
{
	TCHAR szPath[MAX_PATH + 1] = {};
	GetTempPath(MAX_PATH, szPath);
	PathAddBackslash(szPath);
	return szPath;
}

CMMString CMMService::GetAppName()
{
	int nPos = -1;
	std::wstring sName;
	TCHAR szPath[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);

	sName = szPath;
	nPos = sName.rfind(L"\\");
	if (nPos > 0)
		sName = sName.substr(nPos + 1, sName.length() - nPos - 1);
	else
		sName = L"";

	return sName.c_str();
}

UINT CMMService::ReadFileValue(LPCTSTR lpszFileName, LPCTSTR pszKeyName, LPCTSTR pszItemName, UINT nDefault)
{
	//获取目录
	TCHAR szDirectory[MAX_PATH] = _T("");
	CMMService::GetWorkDirectory(szDirectory, MMCountArray(szDirectory));

	//构造路径
	TCHAR szServerInfoPath[MAX_PATH] = _T("");
	_sntprintf(szServerInfoPath, MMCountArray(szServerInfoPath), _T("%s\\%s"), szDirectory, lpszFileName);

	//读取数据
	UINT nValue = GetPrivateProfileInt(pszKeyName, pszItemName, nDefault, szServerInfoPath);

	return nValue;
}

//读取数据
VOID CMMService::ReadFileString(LPCTSTR lpszFileName, LPCTSTR pszKeyName, LPCTSTR pszItemName, TCHAR szResult[], WORD wMaxCount)
{
	//获取目录
	TCHAR szDirectory[MAX_PATH] = _T("");
	CMMService::GetWorkDirectory(szDirectory, MMCountArray(szDirectory));

	//构造路径
	TCHAR szServerInfoPath[MAX_PATH] = _T("");
	_sntprintf(szServerInfoPath, MMCountArray(szServerInfoPath), _T("%s\\%s"), szDirectory, lpszFileName);

	//读取数据
	GetPrivateProfileString(pszKeyName, pszItemName, _T(""), szResult, wMaxCount, szServerInfoPath);

	return;
}

//写入数值
VOID CMMService::WriteFileValue(LPCTSTR lpszFileName, LPCTSTR pszKeyName, LPCTSTR pszItemName, UINT nValue)
{
	//获取目录
	TCHAR szDirectory[MAX_PATH] = _T("");
	CMMService::GetWorkDirectory(szDirectory, MMCountArray(szDirectory));

	//构造路径
	TCHAR szServerInfoPath[MAX_PATH] = _T("");
	_sntprintf(szServerInfoPath, MMCountArray(szServerInfoPath), _T("%s\\%s"), szDirectory, lpszFileName);

	//构造数据
	TCHAR szString[16] = _T("");
	_sntprintf(szString, MMCountArray(szString), _T("%d"), nValue);

	//写入数据
	WritePrivateProfileString(pszKeyName, pszItemName, szString, szServerInfoPath);

	return;
}

//写入数据
VOID CMMService::WriteFileString(LPCTSTR lpszFileName, LPCTSTR pszKeyName, LPCTSTR pszItemName, LPCTSTR pszString)
{
	//获取目录
	TCHAR szDirectory[MAX_PATH] = _T("");
	CMMService::GetWorkDirectory(szDirectory, MMCountArray(szDirectory));

	//构造路径
	TCHAR szServerInfoPath[MAX_PATH] = _T("");
	_sntprintf(szServerInfoPath, MMCountArray(szServerInfoPath), _T("%s\\%s"), szDirectory, lpszFileName);

	WritePrivateProfileString(pszKeyName, pszItemName, pszString, szServerInfoPath);

	return;
}

//GUID
CMMString CMMService::ProductGUID()
{
	CMMString strGUID;
	GUID guid;
	CoCreateGuid(&guid);
	strGUID.Format(_T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
		guid.Data1, guid.Data2,
		guid.Data3, guid.Data4[0],
		guid.Data4[1], guid.Data4[2],
		guid.Data4[3], guid.Data4[4],
		guid.Data4[5], guid.Data4[6],
		guid.Data4[7]);

	return strGUID;
}

//随机值
int CMMService::RandValue(int nMin, int nMax)
{
	if (nMax <= nMin) return nMin;
	return nMin + rand() % (nMax - nMin + 1);
}

double CMMService::RandValue(double lfMin, double lfMax)
{
	if (lfMax <= lfMin) return lfMin;
	return lfMin + (lfMax - lfMin) * (rand() * 1.0 / RAND_MAX);
}

bool CMMService::IsZero(DWORD dwNum)
{
	return (abs((long long)dwNum) <= 1e-15);
}

std::string CMMService::EncryptBase64(unsigned char const* bytes_to_encode, unsigned int in_len)
{
	static const std::string base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--)
	{
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3)
		{
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];

			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';
	}

	return ret;
}

BOOL CMMService::WinNTHDSerialNumAsScsiRead(BYTE* dwSerial, UINT* puSerialLen, UINT uMaxSerialLen)
{
	BOOL bInfoLoaded = FALSE;

	for (int iController = 0; iController < 2; ++iController)
	{
		HANDLE hScsiDriveIOCTL = 0;
		TCHAR   szDriveName[256];

		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		_snwprintf(szDriveName, sizeof(szDriveName), L"\\\\.\\Scsi%d:", iController);

		//  Windows NT, Windows 2000, any rights should do
		hScsiDriveIOCTL = CreateFile(szDriveName,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);

		// if (hScsiDriveIOCTL == INVALID_HANDLE_VALUE)
		//    printf ("Unable to open SCSI controller %d, error code: 0x%lX\n",
		//            controller, GetLastError ());

		if (hScsiDriveIOCTL != INVALID_HANDLE_VALUE)
		{
			int iDrive = 0;
			for (iDrive = 0; iDrive < 2; ++iDrive)
			{
				char szBuffer[sizeof(SRB_IO_CONTROL) + SENDIDLENGTH] = {};

				SRB_IO_CONTROL* p = (SRB_IO_CONTROL*)szBuffer;
				SENDCMDINPARAMS* pin = (SENDCMDINPARAMS*)(szBuffer + sizeof(SRB_IO_CONTROL));
				DWORD dwResult;

				p->HeaderLength = sizeof(SRB_IO_CONTROL);
				p->Timeout = 10000;
				p->Length = SENDIDLENGTH;
				p->ControlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
				strncpy((char*)p->Signature, "SCSIDISK", 8);

				pin->irDriveRegs.bCommandReg = IDE_ATA_IDENTIFY;
				pin->bDriveNumber = iDrive;

				if (DeviceIoControl(hScsiDriveIOCTL, IOCTL_SCSI_MINIPORT,
					szBuffer,
					sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS) - 1,
					szBuffer,
					sizeof(SRB_IO_CONTROL) + SENDIDLENGTH,
					&dwResult, NULL))
				{
					SENDCMDOUTPARAMS* pOut = (SENDCMDOUTPARAMS*)(szBuffer + sizeof(SRB_IO_CONTROL));
					IDSECTOR* pId = (IDSECTOR*)(pOut->bBuffer);
					if (pId->sModelNumber[0])
					{
						if (*puSerialLen + 20U <= uMaxSerialLen)
						{
							// 序列号
							CopyMemory(dwSerial + *puSerialLen, ((USHORT*)pId) + 10, 20);

							// Cut off the trailing blanks
							UINT i = 0;
							for (i = 20; i != 0U && ' ' == dwSerial[*puSerialLen + i - 1]; --i)
							{
							}
							*puSerialLen += i;

							// 型号
							CopyMemory(dwSerial + *puSerialLen, ((USHORT*)pId) + 27, 40);
							// Cut off the trailing blanks
							for (i = 40; i != 0U && ' ' == dwSerial[*puSerialLen + i - 1]; --i)
							{
							}
							*puSerialLen += i;

							bInfoLoaded = TRUE;
						}
						else
						{
							::CloseHandle(hScsiDriveIOCTL);
							return bInfoLoaded;
						}
					}
				}
			}
			::CloseHandle(hScsiDriveIOCTL);
		}
	}
	return bInfoLoaded;
}


BOOL CMMService::WinNTHDSerialNumAsPhysicalRead(BYTE* dwSerial, UINT* puSerialLen, UINT uMaxSerialLen)
{
#define  DFP_GET_VERSION          0x00074080
	BOOL bInfoLoaded = FALSE;

	for (UINT uDrive = 0; uDrive < 4; ++uDrive)
	{
		HANDLE hPhysicalDriveIOCTL = 0;

		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		TCHAR szDriveName[256];
		_snwprintf(szDriveName, sizeof(szDriveName), L"\\\\.\\PhysicalDrive%d", uDrive);

		//  Windows NT, Windows 2000, must have admin rights
		hPhysicalDriveIOCTL = CreateFile(szDriveName,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);

		if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
		{
			GETVERSIONOUTPARAMS VersionParams = {};
			DWORD               cbBytesReturned = 0;

			// Get the version, etc of PhysicalDrive IOCTL
			if (DeviceIoControl(hPhysicalDriveIOCTL, DFP_GET_VERSION,
				NULL,
				0,
				&VersionParams,
				sizeof(GETVERSIONOUTPARAMS),
				&cbBytesReturned, NULL))
			{
				// If there is a IDE device at number "i" issue commands
				// to the device
				if (VersionParams.bIDEDeviceMap != 0)
				{
					BYTE             bIDCmd = 0;   // IDE or ATAPI IDENTIFY cmd
					SENDCMDINPARAMS  scip = {};

					// Now, get the ID sector for all IDE devices in the system.
					// If the device is ATAPI use the IDE_ATAPI_IDENTIFY command,
					// otherwise use the IDE_ATA_IDENTIFY command
					bIDCmd = (VersionParams.bIDEDeviceMap >> uDrive & 0x10) ? IDE_ATAPI_IDENTIFY : IDE_ATA_IDENTIFY;
					BYTE IdOutCmd[sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1] = {};

					if (DoIdentify(hPhysicalDriveIOCTL,
						&scip,
						(PSENDCMDOUTPARAMS)&IdOutCmd,
						(BYTE)bIDCmd,
						(BYTE)uDrive,
						&cbBytesReturned))
					{
						if (*puSerialLen + 20U <= uMaxSerialLen)
						{
							CopyMemory(dwSerial + *puSerialLen, ((USHORT*)(((PSENDCMDOUTPARAMS)IdOutCmd)->bBuffer)) + 10, 20);  // 序列号

																																// Cut off the trailing blanks
							UINT i = 0;
							for (i = 20; i != 0U && ' ' == dwSerial[*puSerialLen + i - 1]; --i) {}
							*puSerialLen += i;

							CopyMemory(dwSerial + *puSerialLen, ((USHORT*)(((PSENDCMDOUTPARAMS)IdOutCmd)->bBuffer)) + 27, 40); // 型号

																															   // Cut off the trailing blanks
							for (i = 40; i != 0U && ' ' == dwSerial[*puSerialLen + i - 1]; --i) {}
							*puSerialLen += i;

							bInfoLoaded = TRUE;
						}
						else
						{
							::CloseHandle(hPhysicalDriveIOCTL);
							return bInfoLoaded;
						}
					}
				}
			}
			CloseHandle(hPhysicalDriveIOCTL);
		}
	}
	return bInfoLoaded;
}

//机器标识
bool CMMService::GetImportIDEx(TCHAR szMachineID[Len_Machine_ID], TCHAR szCPUID[20])
{
	TCHAR szMACAddress[20] = _T("");
	{
		unsigned long s1 = 0, s2 = 0;
		// 		unsigned char vendor_id[]="------------";
		// 		__asm{
		// 			xor eax,eax      //eax=0:取Vendor信息
		// 				cpuid    //取cpu id指令，可在Ring3级使用
		// 
		// 				mov dword ptr vendor_id,ebx
		// 				mov dword ptr vendor_id[+4],edx
		// 				mov dword ptr vendor_id[+8],ecx
		// 		}
		//	VernderID.Format(_T("%s-"),vendor_id);

#ifndef _WIN64
		__asm 
		{
			mov eax, 01h   //eax=1:取CPU序列号
			xor edx, edx
			cpuid
			mov s1, edx
			mov s2, eax

		}
#endif

		_sntprintf_s(szMACAddress, MMCountArray(szMACAddress), _T("%08X%08X"), s1, s2);
		if (szCPUID != NULL) lstrcpyn(szCPUID, szMACAddress, 20);
	}

	//硬盘标识
	WORD wMacSize = 1;
	DWORD *pbuf = (DWORD*)szMACAddress;
	WORD wIndex = (wMacSize + sizeof(DWORD) - 1) / sizeof(DWORD);
	LPCTSTR pszHardDisk[] = { _T("C:\\"),_T("D:\\"),_T("E:\\") };
	for (WORD i = wIndex; i < MMCountArray(pszHardDisk); i++)
	{
		assert(MMCountArray(pszHardDisk) > (i - wIndex));
		GetVolumeInformation(pszHardDisk[i - wIndex], NULL, 0, pbuf + i, NULL, NULL, 0, NULL);
	}

	//转换信息
	assert(Len_Machine_ID >= Len_MD5);
	CMMString strMacAddr = CMMMD5Checksum::GetMD5((BYTE*)szMACAddress, sizeof(szMACAddress));
	lstrcpyn(szMachineID, strMacAddr, Len_Machine_ID);

	return true;
}
//////////////////////////////////////////////////////////////////////////////////
