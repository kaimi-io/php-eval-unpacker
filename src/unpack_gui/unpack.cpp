#include <vector>
#include <string>
#include <algorithm>
#include <iterator>

#include <Windows.h>
#include <Shlwapi.h>
#include <process.h>
#include <tchar.h>

#include "resource.h"

#pragma comment (lib, "Shlwapi")


HWND ghWnd;
std::vector<std::wstring> eval_results;
HANDLE child_read = NULL, child_write = NULL;
const unsigned char signature[] = {0xDE, 0xAD, 0xBE, 0xEF};

LONG WINAPI SEH(struct _EXCEPTION_POINTERS *lpTopLevelExceptionFilter);
int DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
DWORD GetOpenName(TCHAR * outbuf, const TCHAR * filter, const TCHAR * title);
void enable_gui_controls(BOOL enable);


std::wstring str2wstr(const std::string& s)
{
    std::wstring result;
    size_t len, slength = s.length() + 1;

    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
    result.resize(len);

    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &result[0], len);

    return result;
}

unsigned __stdcall process_pipe(void * arg)
{
    BYTE buffer[1024];
    DWORD bytes_read = 0;
    std::wstring temporary;
    std::vector<unsigned char> data;
    std::vector<unsigned char>::iterator begin, end;
    std::vector<std::wstring>::const_iterator it;
    
    //tcb?
    for (;;) 
    {
        if(!PeekNamedPipe(child_read, NULL, 0, NULL, &bytes_read, NULL) && bytes_read == 0)
        {
            //Teh shitty
            begin = data.begin();

            while(1)
            {
                end = std::search(begin, data.end(), signature, signature + sizeof(signature));

                temporary = str2wstr(std::string(begin, end));
                eval_results.push_back(temporary);

                temporary = temporary.substr(0, 12) + L"...";
                SendDlgItemMessage(ghWnd, IDC_LIST, LB_ADDSTRING, NULL, reinterpret_cast<LPARAM>(temporary.c_str()));

                if(end == data.end())
                {
                    break;
                }
                begin = end + sizeof(signature);
            }

            enable_gui_controls(TRUE);

            break;
        }

        ReadFile(child_read, buffer, min(bytes_read, sizeof(buffer)), &bytes_read, NULL);
        if(bytes_read != 0)
        {
            data.insert(data.end(), buffer, buffer + bytes_read);
        }
    }

    CloseHandle(child_read);

    return 0;
}

int DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON ico;
    unsigned int selection_index;
    static TCHAR file_path[MAX_PATH], cmd[MAX_PATH * 2];
    TCHAR * tmp;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;

    ghWnd = hWnd;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            ico = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
            SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)ico);
        break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_LIST:
                    switch(HIWORD(wParam))
                    {
                        case LBN_DBLCLK:
                        case LBN_SELCHANGE:
                            selection_index = SendDlgItemMessage(hWnd, IDC_LIST, LB_GETCURSEL, 0, 0);
                            if(selection_index < eval_results.size())
                            {
                                SetDlgItemText(hWnd, IDC_DATA, eval_results[selection_index].c_str());
                            }
                        break;
                    }
                break;

                case IDC_CLEAR:
                    eval_results.clear();

                    SetDlgItemText(hWnd, IDC_DATA, L"");
                    SendDlgItemMessage(hWnd, IDC_LIST, LB_RESETCONTENT, 0, 0);
                break;

                case IDC_BROWSE:
                    if(GetOpenName(file_path, TEXT("PHP (*.php)\0*.php\0Все файлы (*.*)\0*.*\0\0"), TEXT("Открыть...")))
                    {
                        SetDlgItemText(hWnd, IDC_PATH, file_path);
                    }
                break;

                case IDC_UNPACK:
                    if(GetDlgItemText(hWnd, IDC_PATH, file_path, sizeof(file_path)))
                    {
                        enable_gui_controls(FALSE);

                        _stprintf_s
                        (
                            cmd,
                            sizeof(cmd)/sizeof(cmd[0]),
                            TEXT("-f \"%s\""),
                            file_path
                        );

                        memset(&si, 0, sizeof(STARTUPINFO));
                        memset(&pi, 0, sizeof(PROCESS_INFORMATION));
                        memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));

                        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                        sa.bInheritHandle = TRUE;
                        sa.lpSecurityDescriptor = NULL;

                        CreatePipe(&child_read, &child_write, &sa, 0);
                        SetHandleInformation(child_read, HANDLE_FLAG_INHERIT, 0);


                        si.cb = sizeof(STARTUPINFO);
                        si.wShowWindow = SW_HIDE;
                        si.hStdOutput = child_write;
                        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

                        GetModuleFileName(NULL, file_path, MAX_PATH);

                        tmp = StrRChr(file_path, NULL, L'\\');
                        if(tmp != 0)
                        {
                            *tmp = 0;
                        }
                        
                        _tcscat_s(file_path, MAX_PATH, TEXT("\\php-5.3.3\\php.exe"));
                        
                        if
                        (
                            CreateProcess
                            (
                                file_path,
                                cmd,
                                NULL,
                                NULL,
                                TRUE,
                                0,
                                NULL,
                                NULL,
                                &si,
                                &pi
                            ) == NULL
                        )
                        {
                            MessageBox(hWnd, TEXT("Ошибка создания процесса"), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
                            break;
                        }

                        
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        CloseHandle(child_write);

                        _beginthreadex(NULL, 0, &process_pipe, NULL, 0, NULL);
                    }
                    else
                    {
                        MessageBox(hWnd, TEXT("Укажите путь к файлу"), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
                    }
                break;
            }
        break;

        case WM_CLOSE:
            if(child_write)
            {
                CloseHandle(child_write);
            }
            
            DestroyIcon(ico);
            EndDialog(hWnd, 0);
        break;

        default:
            return 0;
    }

    return 1;
}

LONG WINAPI SEH(struct _EXCEPTION_POINTERS *lpTopLevelExceptionFilter)
{
    FatalAppExit(0, TEXT("Необрабатываемое исключение"));
    return 0L;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetUnhandledExceptionFilter(SEH);

    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, (DLGPROC) DlgProc, 0);

    return 0;
}

void enable_gui_controls(BOOL enable)
{
    EnableWindow(GetDlgItem(ghWnd, IDC_LIST), enable);
    EnableWindow(GetDlgItem(ghWnd, IDC_UNPACK), enable);
    EnableWindow(GetDlgItem(ghWnd, IDC_CLEAR), enable);
}

DWORD GetOpenName(TCHAR * outbuf, const TCHAR * filter, const TCHAR * title)
{
    OPENFILENAME ofn = {0};
    TCHAR buf[MAX_PATH + 2];

    GetModuleFileName(NULL, buf, MAX_PATH);

    TCHAR * tmp = StrRChr(buf, NULL, L'\\');
    if(tmp != 0)
    {
        *tmp = 0;
        ofn.lpstrInitialDir = buf;
    }

    ofn.hInstance = GetModuleHandle(NULL);
    ofn.hwndOwner = ghWnd;
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = outbuf;
    ofn.lpstrFile[0] = 0;
    ofn.lpstrFile[1] = 0;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NONETWORKBUTTON | OFN_PATHMUSTEXIST;

    return GetOpenFileName(&ofn);
}
