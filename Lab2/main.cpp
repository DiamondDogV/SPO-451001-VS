#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <sstream>
#include "gilb.hpp"

enum { Open = 101, Analyze = 102, Editor = 103, Output = 104 };
HWND fileLabel, editor, resultLabel;
HFONT uiFont, codeFont;

// Сообщения парсера хранятся в UTF-8, а Windows использует UTF-16.
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), int(text.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Не удалось прочитать текст UTF-8.");
    std::wstring value(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), int(text.size()), value.data(), size);
    return value;
}

// Текст редактора переводится из UTF-16 в UTF-8 для парсера.
std::string sourceText() {
    int length = GetWindowTextLengthW(editor);
    if (!length) return {};
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(editor, text.data(), length + 1);
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string source(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, source.data(), size, nullptr, nullptr);
    return source;
}

void chooseFile(HWND window) {
    wchar_t path[32768] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = path;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = L"Код Groovy (*.groovy)\0*.groovy\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        try {
            std::ifstream file(std::filesystem::path(path), std::ios::binary);
            if (!file) throw std::runtime_error("Не удалось открыть файл.");
            std::string source{std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>()};
            if (file.bad()) throw std::runtime_error("Ошибка чтения файла.");
            auto text = wide(source);
            if (!text.empty() && text[0] == 0xFEFF) text.erase(0, 1);
            std::wstring normalized;
            for (wchar_t ch : text) {
                if (ch == L'\r') continue;
                if (ch == L'\n') normalized += L'\r';
                normalized += ch;
            }
            SetWindowTextW(editor, normalized.c_str());
            SetWindowTextW(fileLabel, std::filesystem::path(path).filename().c_str());
        } catch (const std::exception& error) {
            SetWindowTextW(resultLabel, wide(error.what()).c_str());
        }
    }
}

void analyze() {
    try {
        auto result = gilb::analyze(sourceText());
        std::wostringstream text;
        text << L"Абсолютная сложность: " << result.absolute
             << L"\r\nЧисло операторов N: " << result.operators
             << L"\r\nОтносительная сложность: " << std::fixed
             << std::setprecision(6) << result.relative()
             << L"\r\nМаксимальный уровень вложенности: " << result.nesting;
        text << L"\r\n\r\nСтрока | Конструкция | Вложенность\r\n";
        for (const auto& branch : result.branches)
            text << branch.line << L" | " << wide(branch.kind) << L" | "
                 << branch.level << L"\r\n";
        SetWindowTextW(resultLabel, text.str().c_str());
    } catch (const std::exception& error) {
        SetWindowTextW(resultLabel, wide(error.what()).c_str());
    }
}

HWND control(HWND window, const wchar_t* type, const wchar_t* text,
             int x, int y, int width, int height, int id = 0, DWORD style = 0) {
    HWND child = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, window, reinterpret_cast<HMENU>(INT_PTR(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(child, WM_SETFONT, WPARAM(uiFont), TRUE);
    return child;
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE: {
        HDC dc = GetDC(window);
        int height = -MulDiv(11, GetDeviceCaps(dc, LOGPIXELSY), 72);
        ReleaseDC(window, dc);
        // Нулевая ширина сохраняет естественные пропорции символов.
        uiFont = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        codeFont = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        control(window, L"BUTTON", L"Выбрать файл", 20, 20, 150, 32, Open);
        control(window, L"BUTTON", L"Анализировать", 185, 20, 150, 32, Analyze);
        fileLabel = control(window, L"STATIC", L"Файл не выбран", 355, 26, 725, 25);
        control(window, L"STATIC", L"Исходный код Groovy", 20, 70, 550, 22);
        control(window, L"STATIC", L"Результаты анализа", 590, 70, 490, 22);
        editor = control(window, L"EDIT", L"", 20, 98, 550, 580, Editor,
            WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN);
        resultLabel = control(window, L"EDIT", L"", 590, 98, 490, 580, Output,
            WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY);
        SendMessageW(editor, EM_SETLIMITTEXT, 0, 0);
        SendMessageW(resultLabel, EM_SETLIMITTEXT, 0, 0);
        SendMessageW(editor, WM_SETFONT, WPARAM(codeFont), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == Open) chooseFile(window);
        else if (LOWORD(wp) == Analyze) analyze();
        return 0;
    case WM_DESTROY:
        DeleteObject(uiFont);
        DeleteObject(codeFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    WNDCLASSW cls = {};
    cls.lpfnWndProc = windowProcedure;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    cls.lpszClassName = L"GroovyGilbLab";
    if (!RegisterClassW(&cls)) return 1;
    HWND window = CreateWindowW(cls.lpszClassName, L"Метрика Джилба — Groovy",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 730, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return int(message.wParam);
}
