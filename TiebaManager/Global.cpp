/*
Copyright (C) 2015  xfgryujk
http://tieba.baidu.com/f?kw=%D2%BB%B8%F6%BC%AB%C6%E4%D2%FE%C3%D8%D6%BB%D3%D0xfgryujk%D6%AA%B5%C0%B5%C4%B5%D8%B7%BD

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "stdafx.h"
#include "Global.h"
using std::regex_iterator;
extern CString g_cookie; //#include "Tieba.h"
#include <msxml2.h>
#import "msscript.ocx" no_namespace
#include <Dbghelp.h>



// 分割字符串
void SplitString(CStringArray& dst, const CString& src, LPCTSTR slipt)
{
	dst.RemoveAll();
	const int len = _tcslen(slipt);

	int start = 0, end = 0;
	while ((end = src.Find(slipt, end)) != -1)
	{
		dst.Add(src.Mid(start, end - start));
		start = end += len;
	}
	dst.Add(src.Right(src.GetLength() - start));
}

// 取字符串之间的字符串
CString GetStringBetween(const CString& src, const CString& left, LPCTSTR right, int startPos)
{
	int leftPos = src.Find(left, startPos);
	if (leftPos == -1)
		return _T("");
	leftPos += left.GetLength();
	int rightPos = src.Find(right, leftPos);
	if (rightPos == -1)
		return _T("");
	return src.Mid(leftPos, rightPos - leftPos);
}

// 取字符串之间的字符串，包括左右的字符串
CString GetStringBetween2(const CString& src, const CString& left, const CString& right, int startPos)
{
	int leftPos = src.Find(left, startPos);
	if (leftPos == -1)
		return _T("");
	int rightPos = src.Find(right, leftPos + left.GetLength());
	if (rightPos == -1)
		return _T("");
	rightPos += right.GetLength();
	return src.Mid(leftPos, rightPos - leftPos);
}

// 取字符串之前的字符串
CString GetStringBefore(const CString& src, LPCTSTR right, int startPos)
{
	int rightPos = src.Find(right, startPos);
	if (rightPos == -1)
		return _T("");
	return src.Left(rightPos);
}

// 写字符串到文件
BOOL WriteString(const CString& src, LPCTSTR path)
{
	CFile file;
	if (!file.Open(path, CFile::modeCreate | CFile::modeWrite))
		return FALSE;
#ifdef _UNICODE
	static const BYTE UNICODE_HEADER[] = { 0xFF, 0xFE };
	file.Write(UNICODE_HEADER, sizeof(UNICODE_HEADER));
#endif
	file.Write((LPCTSTR)src, src.GetLength() * sizeof(TCHAR));
	return TRUE;
}

// URL编码
CString EncodeURI(const CString& src)
{
	CComPtr<IScriptControl> script;
	if (FAILED(script.CoCreateInstance(__uuidof(ScriptControl))))
		return _T("");
	script->PutLanguage("JScript");
	_variant_t param = src;
	SAFEARRAY* params = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	LONG index = 0;
	SafeArrayPutElement(params, &index, &param);
	_variant_t result = script->Run(_bstr_t(_T("encodeURIComponent")), &params);
	SafeArrayDestroy(params);
	return (LPCTSTR)(_bstr_t)result;
}

// URL编码 GBK版
/*CString EncodeURI_GBK(const CString& _src)
{
	CString result, tmp;
	CStringA src(_src); // 有些电脑会转码失败？
	const int len = src.GetLength();
	for (int i = 0; i < len; i++)
	{
		tmp.Format(_T("%%%02X"), src[i] & 0xFF);
		result += tmp;
	}
	return result;
}*/

// HTML转义
CString HTMLEscape(const CString& src)
{
	CString result = src;
	result.Replace(_T("&"), _T("&amp;"));
	result.Replace(_T(" "), _T("&nbsp;"));
	result.Replace(_T("<"), _T("&lt;"));
	result.Replace(_T(">"), _T("&gt;"));
	return result;
}

// HTML反转义
CString HTMLUnescape(const CString& src)
{
	CString result = src;
	//result.Replace(_T("<br>"), _T("\r\n")); // 不转换行符
	result.Replace(_T("&nbsp;"), _T(" "));
	result.Replace(_T("&quot;"), _T("\""));
	result.Replace(_T("&&#039;"), _T("'"));
	result.Replace(_T("&lt;"), _T("<"));
	result.Replace(_T("&gt;"), _T(">"));
	result.Replace(_T("&amp;"), _T("&"));
	return result;
}

// JS反转义，自行转义src里的双引号
CString JSUnescape(const CString& src)
{
	CComPtr<IScriptControl> script;
	if (FAILED(script.CoCreateInstance(__uuidof(ScriptControl))))
		return _T("");
	script->PutLanguage("JScript");
	_variant_t result;
	try
	{
		result = script->Eval((LPCTSTR)(_T("\"") + src + _T("\"")));
	}
	catch (_com_error&)
	{
		return _T("");
	}
	return (LPCTSTR)(_bstr_t)result;
}


// 从HTTP头提取Cookie并修改cookie
static void ReceiveCookie(LPCTSTR headers, CString& cookie)
{
	static const wregex cookieExp(_T("Set-Cookie: (.*?)=(.*?);"));
	for (regex_iterator<LPCTSTR> it(headers, headers + _tcslen(headers), cookieExp), end; it != end; it++)
	{
		CString name = (*it)[1].str().c_str();
		CString value = (*it)[2].str().c_str();
		int start = cookie.Find(name + _T("="));
		if (start == -1)
			cookie += name + _T("=") + value + _T(";");
		else
		{
			start += name.GetLength() + 1;
			int end = cookie.Find(_T(';'), start);
			cookie = cookie.Left(start) + value + cookie.Right(cookie.GetLength() - end);
		}
	}
}

// HTTP请求
static HTTPRequestResult HTTPRequestBase(BOOL postMethod, CComPtr<IServerXMLHTTPRequest>& xml, 
	LPCTSTR URL, LPCTSTR data, BOOL useCookie, volatile BOOL* stopFlag, CString* cookie)
{
	if (FAILED(xml.CoCreateInstance(__uuidof(ServerXMLHTTP))))
		return NET_FAILED_TO_CREATE_INSTANCE;
	if (cookie == NULL)
		cookie = &g_cookie;

	if (postMethod)
	{
		xml->open(_bstr_t(_T("POST")), _bstr_t(URL), _variant_t(true), _variant_t(), _variant_t());
		xml->setRequestHeader(_bstr_t(_T("Content-Type")), _bstr_t(_T("application/x-www-form-urlencoded")));
	}
	else
		xml->open(_bstr_t(_T("GET")), _bstr_t(URL), _variant_t(true), _variant_t(), _variant_t());
	if (useCookie)
		xml->setRequestHeader(_bstr_t(_T("Cookie")), _bstr_t(*cookie));
	xml->send(_variant_t(data));

	// 等待
	DWORD startTime = GetTickCount();
	for (long state = 0; state != 4; xml->get_readyState(&state))
	{
		Delay(1);
		if (stopFlag != NULL && *stopFlag)
		{
			xml->abort();
			return NET_STOP;
		}
		if (GetTickCount() - startTime > 10000)
		{
			xml->abort();
			return NET_TIMEOUT;
		}
	}

	// 接收Cookie
	if (useCookie)
	{
		_bstr_t headers;
		xml->getAllResponseHeaders(headers.GetAddress());
		ReceiveCookie((LPCTSTR)headers, *cookie);
	}

	// 重定向
	long status;
	xml->get_status(&status);
	if (status == 302)
	{
		_bstr_t location;
		xml->getResponseHeader(_bstr_t(_T("Location")), location.GetAddress());
		return HTTPRequestBase(postMethod, xml, URL, data, useCookie, stopFlag, cookie);
	}

	return NET_SUCCESS;
}

// HTTP GET请求
CString HTTPGet(LPCTSTR URL, BOOL useCookie, volatile BOOL* stopFlag, CString* cookie)
{
	CComPtr<IServerXMLHTTPRequest> xml;
	HTTPRequestResult ret = HTTPRequestBase(FALSE, xml, URL, NULL, useCookie, stopFlag, cookie);
	if (ret != NET_SUCCESS)
		switch (ret)
		{
		case NET_FAILED_TO_CREATE_INSTANCE:
			return NET_FAILED_TO_CREATE_INSTANCE_TEXT;
		case NET_STOP:
			return NET_STOP_TEXT;
		case NET_TIMEOUT:
			return NET_TIMEOUT_TEXT;
		}

	_bstr_t result;
	xml->get_responseText(result.GetAddress());
	return (LPCTSTR)result;
}

// HTTP POST请求
CString HTTPPost(LPCTSTR URL, LPCTSTR data, BOOL useCookie, volatile BOOL* stopFlag, CString* cookie)
{
	CComPtr<IServerXMLHTTPRequest> xml;
	HTTPRequestResult ret = HTTPRequestBase(TRUE, xml, URL, data, useCookie, stopFlag, cookie);
	if (ret != NET_SUCCESS)
		switch (ret)
	{
		case NET_FAILED_TO_CREATE_INSTANCE:
			return NET_FAILED_TO_CREATE_INSTANCE_TEXT;
		case NET_STOP:
			return NET_STOP_TEXT;
		case NET_TIMEOUT:
			return NET_TIMEOUT_TEXT;
	}

	_bstr_t result;
	xml->get_responseText(result.GetAddress());
	return (LPCTSTR)result;
}

// HTTP GET请求，取得原始数据，注意自行delete buffer!!!
HTTPRequestResult HTTPGetRaw(LPCTSTR URL, BYTE** buffer, ULONG* size, BOOL useCookie, volatile BOOL* stopFlag, CString* cookie)
{
	if (buffer != NULL)
		*buffer = NULL;
	if (size != NULL)
		*size = 0;

	CComPtr<IServerXMLHTTPRequest> xml;
	HTTPRequestResult ret = HTTPRequestBase(FALSE, xml, URL, NULL, useCookie, stopFlag, cookie);
	if (ret != NET_SUCCESS)
		return ret;

	// 返回
	if (buffer != NULL && size != NULL)
	{
		_variant_t body;
		xml->get_responseBody(body.GetAddress());
		BYTE* p;
		if (SUCCEEDED(SafeArrayAccessData(body.parray, (void**)&p)))
		{
			*size = body.parray->rgsabound[0].cElements;
			*buffer = new BYTE[*size];
			memcpy(*buffer, p, *size);
			SafeArrayUnaccessData(body.parray);
		}
	}
	return NET_SUCCESS;
}


// 不阻塞消息的延迟
void Delay(DWORD time)
{
	DWORD startTime = GetTickCount();
	while (GetTickCount() - startTime < time)
	{
		DoEvents();
		Sleep(1); // 防止占用CPU
	}
}

// 处理消息
void DoEvents()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		DispatchMessage(&msg);
		TranslateMessage(&msg);
	}
}

// 异常处理
LONG WINAPI ExceptionHandler(_EXCEPTION_POINTERS* ExceptionInfo)
{
	CFile file;
	if (file.Open(_T("exception.dmp"), CFile::modeCreate | CFile::modeWrite))
	{
		MINIDUMP_EXCEPTION_INFORMATION einfo;
		einfo.ThreadId = GetCurrentThreadId();
		einfo.ExceptionPointers = ExceptionInfo;
		einfo.ClientPointers = FALSE;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithIndirectlyReferencedMemory,
			&einfo, NULL, NULL);
	}
	AfxMessageBox(_T("程序崩溃了，请把exception.dmp文件发到xfgryujk@126.com帮助调试"), MB_ICONERROR);
	return EXCEPTION_CONTINUE_SEARCH;
}
