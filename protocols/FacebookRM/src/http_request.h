/*
Copyright (c) 2015-18 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

class HttpRequest : public NETLIBHTTPREQUEST, public MZeroedObject
{
	HttpRequest& operator=(const HttpRequest&); // to prevent copying;

	va_list formatArgs;
	CMStringA url;

protected:
	class HttpRequestUrl 
	{
		friend HttpRequest;

	private:
		HttpRequest & request;

		HttpRequestUrl(HttpRequest &request, const char *url) : request(request)
		{
			request.url = url;
			request.szUrl = request.url.GetBuffer();
		}

		HttpRequestUrl(HttpRequest &request, const char *urlFormat, va_list args) : request(request)
		{
			request.url.AppendFormatV(urlFormat, args);
			request.szUrl = request.url.GetBuffer();
		}

		HttpRequestUrl& operator=(const HttpRequestUrl&); // to prevent copying;

	public:
		HttpRequestUrl& operator<<(const char *param);
		HttpRequestUrl& operator<<(const BOOL_PARAM &param);
		HttpRequestUrl& operator<<(const INT_PARAM &param);
		HttpRequestUrl& operator<<(const INT64_PARAM &param);
		HttpRequestUrl& operator<<(const CHAR_PARAM &param);

		char* ToString()
		{
			return request.url.GetBuffer();
		}
	};

	class HttpRequestHeaders
	{
		HttpRequestHeaders& operator=(const HttpRequestHeaders&); // to prevent copying;

		HttpRequest &request;

		void Add(LPCSTR szName)
		{
			Add(szName, "");
		}

		void Add(LPCSTR szName, LPCSTR szValue)
		{
			request.headers = (NETLIBHTTPHEADER*)mir_realloc(
				request.headers, sizeof(NETLIBHTTPHEADER)* (request.headersCount + 1));
			request.headers[request.headersCount].szName = mir_strdup(szName);
			request.headers[request.headersCount].szValue = mir_strdup(szValue);
			request.headersCount++;
		}

	public:
		HttpRequestHeaders(HttpRequest &request) : request(request) {}

		HttpRequestHeaders& operator<<(const CHAR_PARAM &param)
		{
			Add(param.szName, param.szValue);
			return *this;
		}
	};

	class HttpRequestBody
	{
	private:
		CMStringA content;

		void AppendSeparator()
		{
			if (!content.IsEmpty())
				content.AppendChar('&');
		}

	public:
		HttpRequestBody() {}

		HttpRequestBody& operator<<(const char *str);
		HttpRequestBody& operator<<(const BOOL_PARAM &param);
		HttpRequestBody& operator<<(const INT_PARAM &param);
		HttpRequestBody& operator<<(const INT64_PARAM &param);
		HttpRequestBody& operator<<(const CHAR_PARAM &param);

		char* ToString()
		{
			return content.GetBuffer();
		}
	};

	void AddUrlParameter(const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		url += (url.Find('?') == -1) ? '?' : '&';
		url.AppendFormatV(fmt, args);
		va_end(args);
		szUrl = url.GetBuffer();
	}

public:
	HttpRequestUrl Url;
	HttpRequestHeaders Headers;
	HttpRequestBody Body;

	enum PersistentType { NONE, DEFAULT, CHANNEL, MESSAGES };

	bool NotifyErrors;
	PersistentType Persistent;

	HttpRequest(int type, LPCSTR url)
		: Url(*this, url), Headers(*this)
	{
		cbSize = sizeof(NETLIBHTTPREQUEST);
		flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_DUMPASTEXT;
		requestType = type;
		pData = nullptr;
		timeout = 20 * 1000;

		NotifyErrors = true;
		Persistent = DEFAULT;
	}

	HttpRequest(int type, CMStringDataFormat, LPCSTR urlFormat, ...)
		: Url(*this, urlFormat, (va_start(formatArgs, urlFormat), formatArgs)), Headers(*this)
	{
		cbSize = sizeof(NETLIBHTTPREQUEST);
		flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_DUMPASTEXT;
		requestType = type;
		va_end(formatArgs);
		pData = nullptr;
		timeout = 20 * 1000;

		NotifyErrors = true;
		Persistent = DEFAULT;
	}

	virtual ~HttpRequest()
	{
		for (int i = 0; i < headersCount; i++) {
			mir_free(headers[i].szName);
			mir_free(headers[i].szValue);
		}
		mir_free(headers);
	}

	virtual NETLIBHTTPREQUEST* Send(HNETLIBUSER nlu)
	{
		if (url.Find("://") == -1)
			url.Insert(0, ((flags & NLHRF_SSL) ? "https://" : "http://"));
		szUrl = url.GetBuffer();

		if (!pData) {
			pData = Body.ToString();
			dataLength = (int)mir_strlen(pData);
		}

		Netlib_Logf(nlu, "Send request to %s", szUrl);

		return Netlib_HttpTransaction(nlu, this);
	}
};

#endif //_HTTP_REQUEST_H_