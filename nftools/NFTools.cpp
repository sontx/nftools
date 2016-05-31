#include "NFTools.h"

#pragma comment(lib, "urlmon.lib");
#pragma comment(lib, "wininet.lib");

///********************************************************************
/// cpu type detector
///********************************************************************
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL isWindows_x64() {
    BOOL bIsWow64 = FALSE;
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    if(NULL != fnIsWow64Process)
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64)) return FALSE;
    return bIsWow64;
}
///********************************************************************
/// .net framework detector
///***************************************************``*****************
void strcpy(char* to, const char* from, int fromIndex, int toIndex) {
    for(int i = fromIndex; i <= toIndex; i++) {
        to[i - fromIndex] = from[i];
    }
    to[toIndex + 1] = '\0';
}
int getSubKeysCount(LPCSTR st_hkey) {
    HKEY hkey;
    DWORD ret = RegOpenKeyA(HKEY_LOCAL_MACHINE, st_hkey, &hkey);
    if(ret != ERROR_SUCCESS) return 0;

    TCHAR    achClass[MAX_PATH] = TEXT("");
    DWORD    cchClassName = MAX_PATH;
    DWORD    cSubKeys = 0;
    DWORD    cbMaxSubKey;
    DWORD    cchMaxClass;
    DWORD    cValues;
    DWORD    cchMaxValue;
    DWORD    cbMaxValueData;
    DWORD    cbSecurityDescriptor;
    FILETIME ftLastWriteTime;
    DWORD    retCode;

    retCode = RegQueryInfoKey(
                  hkey,
                  achClass,
                  &cchClassName,
                  NULL,
                  &cSubKeys,
                  &cbMaxSubKey,
                  &cchMaxClass,
                  &cValues,
                  &cchMaxValue,
                  &cbMaxValueData,
                  &cbSecurityDescriptor,
                  &ftLastWriteTime);
    RegCloseKey(hkey);
    return cSubKeys;
}
bool openNFKey(HKEY& hkey) {
    return RegOpenKey(
               HKEY_LOCAL_MACHINE,
               TEXT(HK_NET_FRAMEWORK_SETUP),
               &hkey) == ERROR_SUCCESS;
}
bool getNFVersion(std::vector<std::string>& lst) {
    HKEY hkey;
    if(!openNFKey(hkey)) return false;

    TCHAR    versionKeyName[MAX_KEY_LENGTH];
    DWORD    cbName;
    DWORD    cSubKeys;
    FILETIME ftLastWriteTime;
    DWORD    retCode;

    // lay .net framework tu 1.0 -> 4.0
    cSubKeys = getSubKeysCount(HK_NET_FRAMEWORK_SETUP);
    for(int i = 0; i < cSubKeys; i++) {
        cbName = MAX_KEY_LENGTH;
        retCode = RegEnumKeyEx(hkey,
                               i,
                               versionKeyName,
                               &cbName,
                               NULL,
                               NULL,
                               NULL,
                               &ftLastWriteTime);
        if(retCode != ERROR_SUCCESS) break;
        if(versionKeyName[0] == 'v' && strlen((const char *)versionKeyName) > 3) {
            if(cbName > 4) cbName = 4;
            char * name = new char[cbName];
            strcpy(name, (const char *)versionKeyName, 1, cbName - 1);
            lst.push_back(name);
			delete[] name;
        }
    }

    // lay .net framework tu 4.5 or later
    HKEY v45_hkey;
    retCode = RegOpenKey(hkey, TEXT("v4\\Full\\"), &v45_hkey);
    if(retCode == ERROR_SUCCESS) {
        DWORD v45_data;
        DWORD v45_cbData = 4;
        retCode = RegQueryValueEx(v45_hkey,
                                  TEXT("Release"),
                                  NULL,
                                  NULL,
                                  (LPBYTE)(&v45_data),
                                  &v45_cbData);
        if(retCode == ERROR_SUCCESS) {
            if(v45_data >= NET_VERSION_452)
                lst.push_back("4.5.2");
            else if(v45_data >= NET_VERSION_451)
                lst.push_back("4.5.1");
            else if(v45_data >= NET_VERSION_45)
                lst.push_back("4.5");
        }
        RegCloseKey(v45_hkey);
    }

    RegCloseKey(hkey);
    return cSubKeys > 0;
}
bool getMissingNF(vector<string>& lst) {
    string all[] = ALL_DOTNET_VERSION;
    vector<string> install;
    if(!getNFVersion(install)) return false;
    int i , j;
    bool installed;
    for(i = 0; i < ALL_DOTNET_COUNT; i++) {
        installed = false;
        for(j = 0; j < install.size(); j++) {
            if(install[j] == all[i]) {
                installed = true;
                break;
            }
        }
        if(!installed) lst.push_back(all[i]);
    }
    return true;
}
bool isNFInstalled(string dotNetVersion) {
    vector<string> lst;
    if(!getNFVersion(lst)) return false;
    for(int i = 0; i < lst.size(); i++) {
        if(lst[i] == dotNetVersion) return true;
    }
    return false;
}
bool isNFSupported(string dotNetVersion){
    string lst[] = ALL_DOTNET_VERSION;
    for(int i = 0; i < ALL_DOTNET_COUNT; i++){
        if(lst[i] == dotNetVersion) return true;
    }
    return false;
}
bool checkNF(string dotNetVersion){
    if(dotNetVersion.length() < 2){
        cerr << ".Net Framework version must x.x or x.x.x";
        exit(0x1);
    }
    vector<string> lst;
    if(!getNFVersion(lst)) return false;
    for(int i = 0; i < lst.size(); i++){
        if(lst[i].find(dotNetVersion) == 0) return true;
    }
    return false;
}
///********************************************************************
/// .net framework downloader
///********************************************************************
void progressBar(unsigned curr_val, unsigned max_val, unsigned bar_width = 73) {
    if((curr_val != max_val) && (curr_val % (max_val / 100) != 0))
        return;
    double   ratio   =  curr_val / (double)max_val;
    unsigned bar_now =  (unsigned)(ratio * bar_width);
    cout << "\r" << setw(3) << (unsigned)(ratio * 100.0) << "% ";
    for(unsigned curr_val = 0; curr_val < bar_now; ++curr_val)
        printf("%c", 219);
    for(unsigned curr_val = bar_now; curr_val < bar_width; ++curr_val)
        cout << " ";
    cout << "" << flush;
}
class CallbackHandler : public IBindStatusCallback {
private:
    int m_percentLast;
public:
    CallbackHandler() : m_percentLast(0) {
    }
    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
        if(IsEqualIID(IID_IBindStatusCallback, riid) || IsEqualIID(IID_IUnknown, riid) ) {
            *ppvObject = reinterpret_cast<void*>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        return 2UL;
    }

    ULONG STDMETHODCALLTYPE Release() {
        return 1UL;
    }

    // IBindStatusCallback
    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD, IBinding*) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetPriority(LONG*) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnLowResource(DWORD ) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR) {
        switch(ulStatusCode) {
        case BINDSTATUS_FINDINGRESOURCE:
            cout << "Finding resource..." << endl;
            break;
        case BINDSTATUS_CONNECTING:
            cout << "Connecting..." << endl;
            break;
        case BINDSTATUS_SENDINGREQUEST:
            cout << "Sending request..." << endl;
            break;
        case BINDSTATUS_MIMETYPEAVAILABLE:
            cout << "Mime type available" << endl;
            break;
        case BINDSTATUS_BEGINDOWNLOADDATA:
            cout << "Begin download" << endl;
            break;
        case BINDSTATUS_DOWNLOADINGDATA:
        case BINDSTATUS_ENDDOWNLOADDATA: {
            int percent = (int)(100.0 * static_cast<double>(ulProgress) / static_cast<double>(ulProgressMax));
            if(m_percentLast < percent) {
                progressBar(percent, 100);
                m_percentLast = percent;
            }
            if(ulStatusCode == BINDSTATUS_ENDDOWNLOADDATA) {
                cout << endl << "End download" << endl;
            }
        }
        break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT, LPCWSTR) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*, BINDINFO*) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID, IUnknown*) {
        return E_NOTIMPL;
    }
};
HANDLE execute(LPCSTR fileName, LPSTR args = NULL) {
    LPSTARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si->cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    CreateProcessA(
        fileName,
        args,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        si,
        &pi);
    return pi.hProcess;
}
void getRandomName(int len, char temp[]){
    temp[len] = '\0';
    srand(time(NULL));
    for(int i = 0; i < len; i++){
        if(rand() % 2)
            temp[i] = rand() % 10 + 48;
        else
            temp[i] = rand() % 26 + 65;
    }
}
bool download(LPCSTR fileLink, char tempFile[]) {
    int len = GetTempPathA(255, tempFile);
    char tempName[21];
    getRandomName(16, tempName);
    tempName[16] = '.';
    tempName[17] = 'e';
    tempName[18] = 'x';
    tempName[19] = 'e';
    tempName[20] = '\0';
    for(int i = 0; i < 21; i++){
        tempFile[i + len] = tempName[i];
    }

    // perform downloading
    cout << "Downloading: " << fileLink << endl;
    cout << "Temp file: " << tempFile << endl;

    DeleteUrlCacheEntryA(fileLink);
    CallbackHandler callbackHandler;
    IBindStatusCallback* pBindStatusCallback = NULL;
    callbackHandler.QueryInterface(IID_IBindStatusCallback, reinterpret_cast<void**>(&pBindStatusCallback));

    HRESULT ret = URLDownloadToFileA(NULL, fileLink, tempFile, 0, pBindStatusCallback);
    if(SUCCEEDED(ret)) {
        cout << "Downloaded OK!" << endl;
        return true;
    } else {
        cout << "An error occured: error code = 0x" << hex << ret << endl;
        return false;
    }
}
bool downloadAndExecute(LPCSTR fileLink, LPCSTR backupLink = NULL) {
    char tempFile[255];
    bool r = false;
    do {
        r = download(fileLink, tempFile);
        if(!r) {
            if(backupLink == NULL) {
                cout << "Press Y to try again, other keys to exit!" << endl;
                char key;
                scanf("%c%*c", &key);
                if(key == 'y' || key == 'Y')
                    continue;
                else
                    break;
            }
            else {
                cout << "Download with backup link..." << endl;
                fileLink = backupLink;
            }
        }
    } while(!r);
    if(r) {
        //HANDLE h = execute(tempFile, NULL);
        //WaitForSingleObject(h, INFINITE);
        system(tempFile);
        DeleteFileA(tempFile);
        return true;
    } else {
        cout << "Install fail!" << endl;
        return false;
    }
}
///********************************************************************
/// performance tasks
///********************************************************************
bool installNF(string dotNetVersion, bool isWx86 = true) {
    if(dotNetVersion == "1.1")
        return downloadAndExecute(LK_DOTNET_M_110,
                                  LK_DOTNET_B_110);
    if(dotNetVersion == "2.0")
        return downloadAndExecute(isWx86 ? LK_DOTNET_M_200_X86 : LK_DOTNET_M_200_X64,
                                  isWx86 ? LK_DOTNET_B_200_X86 : LK_DOTNET_B_200_X64);
    if(dotNetVersion == "3.0")
        return downloadAndExecute(isWx86 ? LK_DOTNET_M_300_X86 : LK_DOTNET_B_300_X86,
                                  isWx86 ? LK_DOTNET_M_300_X64 : LK_DOTNET_B_300_X64);
    if(dotNetVersion == "3.5")
        return downloadAndExecute(LK_DOTNET_M_350,
                                  LK_DOTNET_B_350);
    if(dotNetVersion == "4.0")
        return downloadAndExecute(LK_DOTNET_M_400,
                                  LK_DOTNET_B_400);
    if(dotNetVersion == "4.5")
        return downloadAndExecute(LK_DOTNET_M_450,
                                  LK_DOTNET_B_450);
    if(dotNetVersion == "4.5.1")
        return downloadAndExecute(LK_DOTNET_M_451,
                                  LK_DOTNET_B_451);
    if(dotNetVersion == "4.5.2")
        return downloadAndExecute(LK_DOTNET_M_452,
                                  LK_DOTNET_B_452);
    return false;
}
void installAllNFMissing() {
    vector<string> lst;
    bool isWx86 = !isWindows_x64();
    getMissingNF(lst);
    for(int i = 0; i < lst.size(); i++) {
        cout << "Installing v" + lst[i] << endl;
        if(installNF(lst[i], isWx86))
            cout << "Installed " + lst[i] << endl;
        else
            cout << "Could not install " + lst[i] << endl;
    }
}
void showMissing(){
    cout << "Missing .Net Framework version: ";
    vector<string> lst;
    getMissingNF(lst);
    for(int i = 0; i < lst.size(); i++){
        cout << lst[i] << " ";
    }
    cout << endl;
}
void showInstalled(){
    cout << "All .Net Framework version installed: ";
    vector<string> lst;
    getNFVersion(lst);
    for(int i = 0; i < lst.size(); i++){
        cout << lst[i] << " ";
    }
    cout << endl;
}
void showHelp() {
    cout << "Help: nftools [param] [options]" << endl;
    cout << "param list:" << endl;
    cout << "-a  install all missing .Net Framework" << endl;
    cout << "-i  install a .Net Framework version" << endl;
    cout << "-c  check and install .Net Framework version if it does not exist" << endl;
    cout << "-s  see .Net Framework version were installed or missing" << endl;
    cout << "exemple:" << endl;
    cout << "    nftools -a\n    nftools -i 4.5\n    nftools -c 4.5\n    nftools -s" << endl;
}
int main(int argc, char* argv[]) {
    cout << "NFTools v1.0" << endl;
    cout << "Author: Gin" << endl << endl;
    // arg dau tien la duong dan
    if(argc == 1 || argc > 3){
        showHelp();
    }
    else{
        if(argc == 2){
            if(strcmp(argv[1], "-a") == 0){
                installAllNFMissing();
            }
            else if(strcmp(argv[1], "-s") == 0){
                showMissing();
                showInstalled();
            }
        }
        else if(argc == 3){
            if(strcmp(argv[1], "-i") == 0){
                string dn = string(argv[2]);
                bool isWx86 = isWindows_x64();
                if(isNFSupported(dn)){
                    cout << "Installing .Net Framework v" + dn << endl;
                    if(installNF(dn, isWx86))
                        cout << "Installed .Net Framework v" + dn << "..." << endl;
                    else
                        cout << "Could not install " + dn << endl;
                }
                else{
                    cout << dn + " is not .Net Framework version"  << endl;
                }
            }
            else if(strcmp(argv[1], "-c") == 0){
                string dn = string(argv[2]);
                bool isWx86 = !isWindows_x64();
                if(isNFSupported(dn)){
                    cout << "Checking .Net Framework v" + dn << "..." << endl;
                    if(!checkNF(dn)){
                        cout << ".Net Framework v" + dn << " does not install" << endl;
                        cout << "Installing .Net Framework v" + dn << "..." << endl;
                        if(installNF(dn, isWx86))
                            cout << "Installed .Net Framework v" + dn << endl;
                        else
                            cout << "Could not install " + dn << endl;
                    }
                    else{
                        cout << ".Net Framework v" + dn << " already installed!" << endl;
                    }
                }
                else{
                    cout << dn + " is not .Net Framework version"  << endl;
                }
            }
            else{
                showHelp();
            }
        }
        else{
            showHelp();
        }
    }
    return 0x0;
}
