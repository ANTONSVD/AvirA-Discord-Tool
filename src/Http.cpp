#include "Http.hpp"
#include <Windows.h>
#include <winhttp.h>

namespace AvirA
{
	void C_Http::SetToken(const std::string& token)
	{
		m_token = Trimmed(token);
	}

	void C_Http::SetBase(const std::string& base)
	{
		m_base = base;
	}

	void C_Http::ClearToken()
	{
		m_token.clear();
	}

	std::string C_Http::MimeFor(const std::string& path)
	{
		size_t dot = path.find_last_of('.');
		std::string ext;
		if (dot != std::string::npos)
			ext = path.substr(dot);
		for (size_t i = 0; i < ext.size(); i++)
			ext[i] = (char)tolower(ext[i]);
		if (ext == ".png")
			return "image/png";
		if (ext == ".jpg" || ext == ".jpeg")
			return "image/jpeg";
		if (ext == ".gif")
			return "image/gif";
		if (ext == ".webp")
			return "image/webp";
		if (ext == ".mp4")
			return "video/mp4";
		if (ext == ".mov")
			return "video/quicktime";
		if (ext == ".webm")
			return "video/webm";
		if (ext == ".mp3")
			return "audio/mpeg";
		if (ext == ".ogg")
			return "audio/ogg";
		if (ext == ".txt")
			return "text/plain";
		if (ext == ".zip")
			return "application/zip";
		return "application/octet-stream";
	}

	std::string C_Http::BuildMultipart(const std::string& json, const std::vector<S_UploadFile>& files, const std::string& boundary)
	{
		std::string out;
		out += "--" + boundary + "\r\n";
		out += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
		out += "Content-Type: application/json\r\n\r\n";
		out += json + "\r\n";
		for (size_t i = 0; i < files.size(); i++)
		{
			FILE* file = nullptr;
			if (fopen_s(&file, files[i].m_path.c_str(), "rb") != 0 || !file)
				continue;
			std::fseek(file, 0, SEEK_END);
			long size = std::ftell(file);
			std::fseek(file, 0, SEEK_SET);
			std::string data;
			if (size > 0 && size < 100 * 1024 * 1024)
			{
				data.resize((size_t)size);
				size_t got = std::fread(&data[0], 1, data.size(), file);
				data.resize(got);
			}
			std::fclose(file);
			if (data.empty())
				continue;
			std::string name = files[i].m_name.empty() ? files[i].m_path : files[i].m_name;
			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos)
				name = name.substr(slash + 1);
			out += "--" + boundary + "\r\n";
			out += "Content-Disposition: form-data; name=\"files[" + FormatU64(i) + "]\"; filename=\"" + name + "\"\r\n";
			std::string mime = files[i].m_mime.empty() ? MimeFor(name) : files[i].m_mime;
			out += "Content-Type: " + mime + "\r\n\r\n";
			out += data + "\r\n";
		}
		out += "--" + boundary + "--\r\n";
		return out;
	}

	S_HttpResult C_Http::Get(const std::string& path)
	{
		return Request("GET", m_base + path, "", "", {});
	}

	S_HttpResult C_Http::Delete(const std::string& path)
	{
		return Request("DELETE", m_base + path, "", "", {});
	}

	S_HttpResult C_Http::PutEmpty(const std::string& path)
	{
		return Request("PUT", m_base + path, "", "", {});
	}

	S_HttpResult C_Http::PostEmpty(const std::string& path)
	{
		return Request("POST", m_base + path, "", "", {});
	}

	S_HttpResult C_Http::PostJson(const std::string& path, const std::string& json)
	{
		return Request("POST", m_base + path, json, "application/json", {});
	}

	S_HttpResult C_Http::PostJsonFull(const std::string& url, const std::string& json)
	{
		return Request("POST", url, json, "application/json", {});
	}

	S_HttpResult C_Http::PatchJson(const std::string& path, const std::string& json)
	{
		return Request("PATCH", m_base + path, json, "application/json", {});
	}

	S_HttpResult C_Http::PostMultipart(const std::string& path, const std::string& json, const std::vector<S_UploadFile>& files)
	{
		std::string boundary = "AvirA" + FormatU64(NowSeconds()) + FormatU64(NowMillis() % 1000000);
		std::string body = BuildMultipart(json, files, boundary);
		return Request("POST", m_base + path, body, "multipart/form-data; boundary=" + boundary, {});
	}

	S_HttpResult C_Http::Request(const std::string& method, const std::string& url, const std::string& body, const std::string& content, const std::vector<S_UploadFile>& files)
	{
		S_HttpResult out;
		std::wstring wide(url.begin(), url.end());
		std::wstring wmethod(method.begin(), method.end());
		URL_COMPONENTSW parts = {};
		parts.dwStructSize = sizeof(parts);
		wchar_t host[256] = {};
		wchar_t path[2048] = {};
		parts.lpszHostName = host;
		parts.dwHostNameLength = 255;
		parts.lpszUrlPath = path;
		parts.dwUrlPathLength = 2047;
		if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts))
		{
			out.m_error = "Bad url";
			return out;
		}
		HINTERNET session = WinHttpOpen(L"AvirA-Discord-Tool/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
		if (!session)
		{
			out.m_error = "No session";
			return out;
		}
		HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
		if (!connect)
		{
			WinHttpCloseHandle(session);
			out.m_error = "No connect";
			return out;
		}
		DWORD flags = 0;
		if (parts.nScheme == INTERNET_SCHEME_HTTPS)
			flags |= WINHTTP_FLAG_SECURE;
		HINTERNET request = WinHttpOpenRequest(connect, wmethod.c_str(), path, nullptr, nullptr, nullptr, flags);
		if (!request)
		{
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			out.m_error = "No request";
			return out;
		}
		std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AvirA-Discord-Tool/1.0\r\n";
		if (!m_token.empty())
		{
			std::string auth = "Authorization: " + m_token + "\r\n";
			headers += std::wstring(auth.begin(), auth.end());
		}
		if (!content.empty())
		{
			std::string line = "Content-Type: " + content + "\r\n";
			headers += std::wstring(line.begin(), line.end());
		}
		WinHttpAddRequestHeaders(request, headers.c_str(), (DWORD)headers.size(), WINHTTP_ADDREQ_FLAG_ADD);
		BOOL sent = FALSE;
		if (body.empty())
			sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		else
			sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
		if (!sent)
		{
			out.m_error = "Send failed";
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return out;
		}
		if (!WinHttpReceiveResponse(request, nullptr))
		{
			out.m_error = "No response";
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return out;
		}
		DWORD status = 0;
		DWORD status_size = sizeof(status);
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &status_size, nullptr);
		out.m_status = (int)status;
		std::string collected;
		DWORD chunk = 0;
		do
		{
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request, &available))
				break;
			if (!available)
				break;
			std::string part;
			part.resize(available);
			DWORD got = 0;
			if (!WinHttpReadData(request, &part[0], available, &got))
				break;
			part.resize(got);
			collected += part;
			chunk++;
			if (chunk > 20000)
				break;
		} while (true);
		out.m_body = collected;
		out.m_ok = out.m_status >= 200 && out.m_status < 300;
		if (!out.m_ok && out.m_body.empty())
			out.m_error = "HTTP " + FormatI32(out.m_status);
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return out;
	}
}
