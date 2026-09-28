#pragma once
#include "AvirA.hpp"

namespace AvirA
{
	struct S_HttpResult
	{
		int m_status = 0;
		std::string m_body;
		std::string m_error;
		bool m_ok = false;
	};

	struct S_UploadFile
	{
		std::string m_name;
		std::string m_path;
		std::string m_mime;
	};

	class C_Http
	{
	public:
		void SetToken(const std::string& token);
		void SetBase(const std::string& base);
		void SetSite(const std::string& referer, const std::string& title);
		void ClearToken();

		S_HttpResult Get(const std::string& path);
		S_HttpResult Delete(const std::string& path);
		S_HttpResult PutEmpty(const std::string& path);
		S_HttpResult PutJson(const std::string& path, const std::string& json);
		S_HttpResult PostEmpty(const std::string& path);
		S_HttpResult PostJson(const std::string& path, const std::string& json);
		S_HttpResult PostJsonFull(const std::string& url, const std::string& json);
		S_HttpResult PatchJson(const std::string& path, const std::string& json);
		S_HttpResult PatchJsonFull(const std::string& url, const std::string& json);
		S_HttpResult DeleteFull(const std::string& url);
		S_HttpResult PostMultipart(const std::string& path, const std::string& json, const std::vector<S_UploadFile>& files);
		S_HttpResult PostMultipartFull(const std::string& url, const std::string& json, const std::vector<S_UploadFile>& files);
		bool GetFull(const std::string& url, std::string& body, std::string& mime);

	private:
		S_HttpResult Request(const std::string& method, const std::string& url, const std::string& body, const std::string& content, const std::vector<S_UploadFile>& files, std::string* out_mime = nullptr);
		std::string BuildMultipart(const std::string& json, const std::vector<S_UploadFile>& files, const std::string& boundary);
		static std::string MimeFor(const std::string& path);

		std::string m_token;
		std::string m_base = AVIRA_DISCORD_API;
		std::string m_referer;
		std::string m_title;
	};
}
