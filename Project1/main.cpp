#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#include <TlHelp32.h>


#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// دالة تجيب HWID من Volume Serial


std::string getValue(const std::string& json, const std::string& key) {
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";

    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";

    pos++;

    // If the value starts with a quote → it's a string
    if (json[pos] == '\"') {
        size_t start = pos + 1;
        size_t end = json.find("\"", start);
        return json.substr(start, end - start);
    }

    // Otherwise: number or bool
    size_t end = json.find_first_of(",}", pos);
    return json.substr(pos, end - pos);
}
std::string GetHWID() {
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);

    char buffer[32];
    sprintf_s(buffer, "%08X", serial);
    return buffer;
}

// دالة POST JSON باستخدام WinHTTP
std::string HttpPostJson(const wchar_t* host, const wchar_t* path, const std::string& jsonBody)
{
    HINTERNET hSession = WinHttpOpen(
        L"WinHTTP License Checker",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0
    );

    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host, 80, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", path,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers = L"Content-Type: application/json";

    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        (DWORD)-1,
        (LPVOID)jsonBody.c_str(),
        (DWORD)jsonBody.size(),
        (DWORD)jsonBody.size(),
        0
    );

    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;
    DWORD size = 0;

    // قراءة كامل الرد (مش بس تشنك واحد)
    while (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
        std::string buffer(size, 0);
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, &buffer[0], size, &downloaded) || downloaded == 0)
            break;
        response.append(buffer, 0, downloaded);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

// التحقق من المفتاح
bool CheckLicense(const std::string& key)
{

    std::string hwid = GetHWID();


    std::string jsonBody =
        "{ \"key\": \"" + key + "\", \"hwid\": \"" + hwid + "\" }";

    //std::string response = HttpPostJson(L"project1-2p9w.onrender.com", L"/check", jsonBody);
    std::string response = HttpPostJson(L"127.0.0.1", L"/check", jsonBody);
    //std::cout << jsonBody;
    //std::cout << response;

    if (response.empty())
    {
        std::cout << "Error: Empty response from server\n";
        return false;
    }

    //std::cout << "Server Response: " << response << "\n";



    if (response.find("\"status\":\"ok\"") != std::string::npos ||
        response.find("\"status\": \"ok\"") != std::string::npos)
    {
        std::string days_left = getValue(response, "days_left");
        std::string expires_at = getValue(response, "expires_at");
        std::string status = getValue(response, "status");

        std::cout << "Days left: " << days_left << std::endl;
        std::cout << "Expires at: " << expires_at << std::endl;
        std::cout << "Status: " << status << std::endl;
        std::ofstream file("key.txt");

        if (file.is_open())
        {
            file << key;
            file.close();
            //std::cout << "Key saved\n";

        }


        return true;
    }

    return false;



}


// دالة التحميل
bool Download(const wchar_t* url, const wchar_t* savePath)
{
    URL_COMPONENTS uc = { sizeof(URL_COMPONENTS) };
    wchar_t host[256], urlPath[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(url, 0, 0, &uc))
        return false;

    HINTERNET session = WinHttpOpen(
        L"C++ Downloader",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        NULL, NULL, 0
    );
    if (!session)
        return false;

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET request = WinHttpOpenRequest(
        connect, L"GET", uc.lpszUrlPath,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    bool success = true;

    std::ofstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    DWORD size = 0;

    while (true)
    {
        if (!WinHttpQueryDataAvailable(request, &size)) {
            success = false;
            break;
        }

        if (size == 0)
            break;

        while (size > 0)
        {
            DWORD toRead = (size < BUFFER_SIZE) ? size : BUFFER_SIZE;

            DWORD bytesRead = 0;
            if (!WinHttpReadData(request, buffer, toRead, &bytesRead) || bytesRead == 0) {
                success = false;
                break;
            }

            file.write(buffer, bytesRead);
            size -= bytesRead;
        }

        if (!success)
            break;
    }

    file.close();
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    return success;
}



int main()
{
    std::wcout << L"Welcome to the Tmep Spoofer!\n";


        
    std::ifstream fileRead("key.txt");
    std::string key;

    if (!fileRead.is_open()) {
        std::cout << "Key not downloaded\n";
        std::cout << "Enter license key: ";
        std::cin >> key;
        std::cout << "Key :" << key << "\n";
    }

    else
    {
        
        std::string key1((std::istreambuf_iterator<char>(fileRead)),
        std::istreambuf_iterator<char>());
        std::cout << "Key :" << key1 << "\n";
        key = key1;
    }

   
    if (!CheckLicense(key))
    {
        std::cout << "License invalid. Exiting...\n";
        return 0;

     }

    
    
    //std::cout << "License OK! Running program...\n";
    
    std::wcout << L"Press 1 to Soopfer: ";

    int choice;
    std::wcin >> choice;

    if (choice != 1)
        return 0;

    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);

    std::wstring sysPath = std::wstring(temp) + L"bg9io8.sys";
    std::wstring exePath = std::wstring(temp) + L"Devmkh.exe";

    const wchar_t* urlSys = L"https://github.com/aldnoy-a11y/sp/releases/download/v1.0/2.sys";
    const wchar_t* urlExe = L"https://github.com/aldnoy-a11y/sp/releases/download/v1.0/1.exe";

    //std::wcout << L"\nDownloading files...\n";

    if (!Download(urlSys, sysPath.c_str()))
        std::wcout << L"Failed to download .sys\n";

    if (!Download(urlExe, exePath.c_str()))
        std::wcout << L"Failed to download .exe\n";

    std::wstring argument = L"\"" + sysPath + L"\"";

    ShellExecuteW(NULL, L"open", exePath.c_str(), argument.c_str(), temp, SW_SHOW);

    std::wcout << L"\nDone. Press Enter to exit...";
    std::wcin.ignore();
    std::wcin.get();

    return 0;
}

