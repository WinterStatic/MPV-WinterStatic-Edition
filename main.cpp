// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 WinterStatic

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <iterator>
#include <memory>
#include <fstream>
#include <map>
#include <array>
#include <filesystem>

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

// -----------------------------------------------------------------------------
// Minimal libmpv ABI declarations.
// The EXE loads libmpv-2.dll dynamically, so MSVC does not need an import lib.
// -----------------------------------------------------------------------------
struct mpv_handle;
struct mpv_node_list;
struct mpv_byte_array;

enum mpv_format {
    MPV_FORMAT_NONE = 0,
    MPV_FORMAT_STRING = 1,
    MPV_FORMAT_OSD_STRING = 2,
    MPV_FORMAT_FLAG = 3,
    MPV_FORMAT_INT64 = 4,
    MPV_FORMAT_DOUBLE = 5,
    MPV_FORMAT_NODE = 6,
    MPV_FORMAT_NODE_ARRAY = 7,
    MPV_FORMAT_NODE_MAP = 8,
    MPV_FORMAT_BYTE_ARRAY = 9
};

struct mpv_node {
    union {
        char* string;
        int flag;
        int64_t int64;
        double double_;
        mpv_node_list* list;
        mpv_byte_array* ba;
    } u{};
    mpv_format format = MPV_FORMAT_NONE;
};

struct mpv_node_list {
    int num = 0;
    mpv_node* values = nullptr;
    char** keys = nullptr;
};


enum mpv_event_id {
    MPV_EVENT_NONE = 0,
    MPV_EVENT_FILE_LOADED = 8
};

struct mpv_event {
    mpv_event_id event_id = MPV_EVENT_NONE;
    int error = 0;
    uint64_t reply_userdata = 0;
    void* data = nullptr;
};

namespace {

constexpr wchar_t kMainClass[] = L"MPVMPCNativeMain";
constexpr wchar_t kVideoClass[] = L"MPVMPCNativeVideo";
constexpr wchar_t kPanelClass[] = L"MPVMPCNativePanel";
constexpr wchar_t kMenuClass[] = L"MPVMPCNativeMenu";
constexpr wchar_t kSeekClass[] = L"MPVMPCNativeSeek";
constexpr wchar_t kButtonClass[] = L"MPVMPCNativeButton";
constexpr wchar_t kVolumeClass[] = L"MPVMPCNativeVolume";
constexpr wchar_t kOverlayClass[] = L"MPVMPCNativeOverlay";

constexpr wchar_t kAppTitle[] = L"MPV WinterStatic Edition";
constexpr wchar_t kVersionText[] = L"Version 0.4.5";
constexpr wchar_t kProjectUrl[] = L"https://github.com/WinterStatic/MPV-WinterStatic-Edition";

constexpr int kMenuHeight = 24;
constexpr int kPanelHeight = 56;
constexpr int kSeekHeight = 17;
constexpr int kControlHeight = 26;
constexpr int kVolumeStep = 5;
constexpr int kDefaultVolume = 70;
constexpr int kSeekStepSeconds = 5;
constexpr int kSyncStepMs = 50;
constexpr int kRevealEdge = 8;
constexpr ULONGLONG kFullscreenCursorHideMs = 2000;
constexpr double kResumeMinSeconds = 5.0;
constexpr double kResumeEndMarginMaxSeconds = 10.0;

constexpr COLORREF C_BG = RGB(0, 0, 0);
constexpr COLORREF C_PANEL = RGB(27, 27, 27);
constexpr COLORREF C_PANEL_EDGE = RGB(38, 38, 38);
constexpr COLORREF C_BUTTON = RGB(35, 35, 35);
constexpr COLORREF C_BUTTON_DOWN = RGB(30, 34, 34);
constexpr COLORREF C_HOVER = RGB(138, 138, 138);
constexpr COLORREF C_TEXT = RGB(232, 232, 232);
constexpr COLORREF C_TEXT_DIM = RGB(175, 175, 175);
constexpr COLORREF C_AUTO_OFF_TEXT = RGB(105, 105, 105);
constexpr COLORREF C_SEEK_BG = RGB(22, 22, 22);
constexpr COLORREF C_SEEK_FILL = RGB(44, 44, 44);
constexpr COLORREF C_CHAPTER_MARKER = RGB(96, 96, 96);
constexpr COLORREF C_VOLUME_BG = RGB(18, 18, 18);
constexpr COLORREF C_VOLUME_FILL = RGB(112, 112, 112);
constexpr COLORREF C_BLACK = RGB(0, 0, 0);
constexpr COLORREF C_TEAL = RGB(0, 106, 109);

constexpr UINT_PTR TIMER_SINGLE_CLICK = 1;
constexpr UINT_PTR TIMER_FULLSCREEN_HOVER = 2;
constexpr UINT_PTR TIMER_MPV_POLL = 3;
constexpr UINT_PTR TIMER_PLAYLIST_OSD_RESTORE = 4;
constexpr int64_t kPlaylistOverlayId = 1;
constexpr UINT WM_APP_AUTO_NEXT_FILE = WM_APP + 1;

constexpr int IDC_PLAY = 1001;
constexpr int IDC_STOP = 1002;
constexpr int IDC_PREV = 1003;
constexpr int IDC_SEEK_BACK = 1004;
constexpr int IDC_SEEK_FORWARD = 1005;
constexpr int IDC_NEXT = 1006;
constexpr int IDC_SPEED_DOWN = 1007;
constexpr int IDC_SPEED_UP = 1008;
constexpr int IDC_AUDIO = 1009;
constexpr int IDC_SUBS = 1010;
constexpr int IDC_MUTE = 1011;
constexpr int IDC_VOLUME = 1012;
constexpr int IDC_SEEK = 1013;
constexpr int IDC_STATUS = 1014;
constexpr int IDC_TIME = 1015;
constexpr int IDC_VOLUME_TEXT = 1016;
constexpr int IDC_AUDIO_INFO = 1017;
constexpr int IDC_SUB_INFO = 1018;
constexpr int IDC_AUTO_NEXT = 1019;
constexpr int IDC_PLAYLIST_OSD = 1020;
constexpr int IDC_PLAYLIST_MENU = 1021;

constexpr int IDM_FILE_OPEN = 2001;
constexpr int IDM_FILE_EXIT = 2002;
constexpr int IDM_VIEW_FULLSCREEN = 2003;
constexpr int IDM_PLAY_TOGGLE = 2004;
constexpr int IDM_PLAY_STOP = 2005;
constexpr int IDM_PLAY_PREV = 2006;
constexpr int IDM_PLAY_BACK = 2007;
constexpr int IDM_PLAY_FORWARD = 2008;
constexpr int IDM_PLAY_NEXT = 2009;
constexpr int IDM_SPEED_DOWN = 2010;
constexpr int IDM_SPEED_UP = 2011;
constexpr int IDM_AUDIO_TRACKS = 2012;
constexpr int IDM_SUB_TRACKS = 2013;
constexpr int IDM_MUTE = 2014;
constexpr int IDM_HELP_ABOUT = 2015;
constexpr int IDM_VIEW_OPTIONS = 2016;
constexpr int IDM_VIEW_MEDIA_INFO = 2017;
constexpr int IDM_VIEW_OPEN_MPV_CONFIG = 2018;
constexpr int IDM_AUTO_NEXT_FILE = 2019;
constexpr int IDM_PLAYLIST_SHOW = 2020;
constexpr int IDM_PLAYLIST_PREV = 2021;
constexpr int IDM_PLAYLIST_NEXT = 2022;

// Dedicated ranges for the one-shot right-click cascading track menus.
// These are intentionally far away from control/menu IDs.
constexpr int IDM_CTX_AUDIO_TRACK_BASE = 30000;
constexpr int IDM_CTX_SUB_TRACK_OFF = 31000;
constexpr int IDM_CTX_SUB_TRACK_BASE = 31001;
constexpr int IDM_CTX_PLAYLIST_BASE = 32000;
constexpr int kMaxPlaylistMenuItems = 1000;

constexpr int IDC_OPT_DELAY = 3001;
constexpr int IDC_OPT_OSD = 3002;
constexpr int IDC_OPT_AUDIO_LANG = 3003;
constexpr int IDC_OPT_SUB_LANG = 3004;
constexpr int IDC_OPT_OSD_SIZE = 3005;
constexpr int IDC_OPT_SUB_SIZE = 3006;
constexpr int IDC_OPT_PLAYLIST_START_OSD = 3007;
constexpr int IDC_OPT_GPU_API = 3008;
constexpr int IDC_OPT_SHORTCUTS = 3009;
constexpr int IDC_OPT_EXIT_FULLSCREEN_END = 3010;

constexpr int IDC_SHORTCUT_PRIMARY_BASE = 4000;
constexpr int IDC_SHORTCUT_ALT_BASE = 4100;
constexpr int IDC_SHORTCUT_CLEAR = 4200;
constexpr int IDC_SHORTCUT_RESET = 4201;
constexpr int IDC_ABOUT_GITHUB = 4300;

HINSTANCE g_instance = nullptr;
HWND g_main = nullptr;
HWND g_video = nullptr;
HWND g_overlay = nullptr;
HWND g_menuBar = nullptr;
HWND g_panel = nullptr;
HWND g_seek = nullptr;
HWND g_play = nullptr;
HWND g_stop = nullptr;
HWND g_prev = nullptr;
HWND g_seekBack = nullptr;
HWND g_seekForward = nullptr;
HWND g_next = nullptr;
HWND g_speedDown = nullptr;
HWND g_speedUp = nullptr;
HWND g_autoNext = nullptr;
HWND g_playlistOsd = nullptr;
HWND g_playlistMenu = nullptr;
HWND g_audio = nullptr;
HWND g_subs = nullptr;
HWND g_mute = nullptr;
HWND g_volume = nullptr;
HWND g_status = nullptr;
HWND g_time = nullptr;
HWND g_volumeText = nullptr;
HWND g_audioInfo = nullptr;
HWND g_subInfo = nullptr;
HFONT g_font = nullptr;
HFONT g_smallFont = nullptr;
HBRUSH g_panelBrush = nullptr;
HBRUSH g_editBrush = nullptr;
HBRUSH g_seekBgBrush = nullptr;
HBRUSH g_seekFillBrush = nullptr;
HBRUSH g_chapterMarkerBrush = nullptr;
HBRUSH g_seekPlayheadBrush = nullptr;
HMENU g_fileMenu = nullptr;
HMENU g_viewMenu = nullptr;
HMENU g_playMenu = nullptr;
HMENU g_helpMenu = nullptr;
HHOOK g_menuHook = nullptr;
HICON g_playIconBig = nullptr;
HICON g_playIconSmall = nullptr;
HICON g_pauseIconBig = nullptr;
HICON g_pauseIconSmall = nullptr;

enum class OsdPosition {
    GoldenCenter = 0,
    TopLeft = 1
};

enum class PlaylistStartOsd {
    Nothing = 0,
    Title = 1,
    Playlist = 2
};

enum class GpuApi {
    Auto = 0,
    D3D11 = 1,
    Vulkan = 2,
    OpenGL = 3
};

enum class ShortcutAction : size_t {
    OpenFile = 0,
    PlayPause,
    Stop,
    PreviousChapter,
    NextChapter,
    SeekBackward,
    SeekForward,
    SpeedDown,
    SpeedUp,
    VolumeUp,
    VolumeDown,
    Mute,
    ToggleFullscreen,
    ExitFullscreen,
    ToggleAutoNext,
    TogglePlaylist,
    PreviousPlaylist,
    NextPlaylist,
    CycleAudio,
    CycleSubtitle,
    AudioDelayDown,
    AudioDelayUp,
    ResetAudioDelay,
    SubtitleDelayDown,
    SubtitleDelayUp,
    ResetSubtitleDelay,
    MediaInfo,
    Count
};

struct KeyBinding {
    UINT vk = 0;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
};

struct ShortcutActionInfo {
    const wchar_t* label;
    const wchar_t* settingKey;
};

constexpr size_t kShortcutSlotCount = 2;
constexpr size_t kShortcutActionCount =
    static_cast<size_t>(ShortcutAction::Count);

const std::array<ShortcutActionInfo, kShortcutActionCount> kShortcutActionInfo{{
    { L"Open file", L"OpenFile" },
    { L"Play / Pause", L"PlayPause" },
    { L"Stop", L"Stop" },
    { L"Previous chapter", L"PreviousChapter" },
    { L"Next chapter", L"NextChapter" },
    { L"Seek backward 5 seconds", L"SeekBackward" },
    { L"Seek forward 5 seconds", L"SeekForward" },
    { L"Decrease speed", L"SpeedDown" },
    { L"Increase speed", L"SpeedUp" },
    { L"Volume up", L"VolumeUp" },
    { L"Volume down", L"VolumeDown" },
    { L"Mute", L"Mute" },
    { L"Toggle fullscreen", L"ToggleFullscreen" },
    { L"Exit fullscreen", L"ExitFullscreen" },
    { L"Toggle AUTO next file", L"ToggleAutoNext" },
    { L"Toggle playlist OSD", L"TogglePlaylist" },
    { L"Previous playlist item", L"PreviousPlaylist" },
    { L"Next playlist item", L"NextPlaylist" },
    { L"Cycle audio track", L"CycleAudio" },
    { L"Cycle subtitle track", L"CycleSubtitle" },
    { L"Audio delay -50 ms", L"AudioDelayDown" },
    { L"Audio delay +50 ms", L"AudioDelayUp" },
    { L"Reset audio delay", L"ResetAudioDelay" },
    { L"Subtitle delay -50 ms", L"SubtitleDelayDown" },
    { L"Subtitle delay +50 ms", L"SubtitleDelayUp" },
    { L"Reset subtitle delay", L"ResetSubtitleDelay" },
    { L"Media Info", L"MediaInfo" }
}};

using ShortcutTable =
    std::array<std::array<KeyBinding, kShortcutSlotCount>, kShortcutActionCount>;

ShortcutTable g_shortcuts{};
ShortcutTable g_shortcutDialogBindings{};
WNDPROC g_shortcutEditOriginalProc = nullptr;
HWND g_lastShortcutEdit = nullptr;

enum class MediaShortcutSource {
    KeyDown,
    AppCommand
};

UINT g_lastHandledMediaVk = 0;
ULONGLONG g_lastHandledMediaTick = 0;
MediaShortcutSource g_lastHandledMediaSource = MediaShortcutSource::KeyDown;
constexpr ULONGLONG kMediaShortcutDedupMs = 80;

UINT g_singleClickDelayMs = 64;
int g_osdFontSize = 72;
int g_subtitleFontSize = 55;
OsdPosition g_osdPosition = OsdPosition::TopLeft;
PlaylistStartOsd g_playlistStartOsd = PlaylistStartOsd::Nothing;
GpuApi g_gpuApi = GpuApi::Auto;
bool g_exitFullscreenOnPlaybackEnd = true;
std::wstring g_preferredAudioLanguage;
std::wstring g_preferredSubtitleLanguage;

// These are updated only by deliberate user track changes (A/S or track menus).
// Automatic selection on file load never overwrites them.
bool g_haveRememberedAudio = false;
std::wstring g_rememberedAudioLanguage;
std::wstring g_rememberedAudioTitle;

bool g_haveRememberedSubtitle = false;
bool g_rememberedSubtitleOff = false;
std::wstring g_rememberedSubtitleLanguage;
std::wstring g_rememberedSubtitleTitle;

bool g_startMaximized = true;
bool g_haveSavedWindowRect = false;
bool g_mediaInfoVisible = false;
bool g_autoPlayNextFile = false;
bool g_lastEofReached = false;
bool g_seenPlayingSinceLoad = false;
bool g_suppressNextEofFullscreenExit = false;
bool g_fileLoadPending = false;
bool g_audioDelayTouched = false;
bool g_subtitleDelayTouched = false;
bool g_playlistOsdVisible = false;
bool g_haveLoadedPlaylistItem = false;
int64_t g_lastLoadedPlaylistPos = -1;
RECT g_savedWindowRect{};

bool g_fullscreen = false;
bool g_controlsVisible = true;
bool g_pendingSingleClick = false;
bool g_singleClickApplied = false;
bool g_ignoreNextVideoButtonUp = false;
bool g_seekPointerDown = false;
bool g_seekDragging = false;
POINT g_seekMouseDownPoint{};
bool g_volumeDragging = false;
bool g_playing = false;
bool g_muted = false;
bool g_mpvReady = false;
bool g_initializingPlaybackEngine = true;
int g_volumePercent = kDefaultVolume;
int g_wheelRemainder = 0;
int g_menuHover = -1;
int g_activeTopMenu = -1;
int g_menuSwitchRequest = -1;
int g_pollCounter = 0;
double g_timePos = 0.0;
double g_duration = 0.0;
ULONGLONG g_lastControlsHover = 0;
ULONGLONG g_lastFullscreenMouseActivity = 0;
bool g_fullscreenCursorHidden = false;
DWORD g_windowedStyle = 0;
DWORD g_windowedExStyle = 0;
RECT g_windowedRect{};
WINDOWPLACEMENT g_windowedPlacement{ sizeof(WINDOWPLACEMENT) };
std::wstring g_statusText = L"Stopped";
std::wstring g_audioInfoText = L"--";
std::wstring g_subInfoText = L"--";
std::wstring g_currentMediaPath;
std::wstring g_pendingAutoNextPath;
std::map<std::wstring, double> g_resumePositions;
bool g_resumePositionsDirty = false;
std::vector<double> g_chapterTimes;

struct MpvApi {
    HMODULE dll = nullptr;
    mpv_handle* (__cdecl* create)() = nullptr;
    int (__cdecl* initialize)(mpv_handle*) = nullptr;
    void (__cdecl* terminate_destroy)(mpv_handle*) = nullptr;
    int (__cdecl* command)(mpv_handle*, const char* const[]) = nullptr;
    int (__cdecl* command_node)(mpv_handle*, mpv_node*, mpv_node*) = nullptr;
    int (__cdecl* set_option)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (__cdecl* set_option_string)(mpv_handle*, const char*, const char*) = nullptr;
    int (__cdecl* load_config_file)(mpv_handle*, const char*) = nullptr;
    int (__cdecl* get_property)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (__cdecl* set_property)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (__cdecl* set_property_string)(mpv_handle*, const char*, const char*) = nullptr;
    mpv_event* (__cdecl* wait_event)(mpv_handle*, double) = nullptr;
    void (__cdecl* free_fn)(void*) = nullptr;
    void (__cdecl* free_node_contents)(mpv_node*) = nullptr;
    const char* (__cdecl* error_string)(int) = nullptr;
} g_mpvApi;

mpv_handle* g_mpv = nullptr;
DLL_DIRECTORY_COOKIE g_libMpvDllDirectoryCookie = nullptr;

struct TrackInfo {
    int64_t id = 0;
    std::string type;
    std::string lang;
    std::string title;
    bool selected = false;
};

struct PlaylistEntry {
    std::string filename;
    std::string title;
    bool current = false;
    bool playing = false;
};

const char* GpuApiOptionValue(GpuApi api) {
    switch (api) {
    case GpuApi::D3D11: return "d3d11";
    case GpuApi::Vulkan: return "vulkan";
    case GpuApi::OpenGL: return "opengl";
    case GpuApi::Auto:
    default: return "auto";
    }
}

void FillSolid(HDC dc, const RECT& r, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    const int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) {
        const int fallback = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (fallback <= 0) return L"";
        std::wstring out(static_cast<size_t>(fallback), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), fallback);
        return out;
    }
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH * 4]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (n == 0 || n >= std::size(path)) return L".";
    std::wstring p(path, n);
    const size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}


bool DirectoryIsWritable(const std::wstring& directory) {
    const std::wstring testPath = directory + L"\\.mpv-winterstatic-write-test.tmp";
    HANDLE file = CreateFileW(
        testPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    DeleteFileW(testPath.c_str());
    return true;
}

std::wstring GetLocalSettingsPath() {
    wchar_t localAppData[MAX_PATH * 4]{};
    const DWORD n = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData,
        static_cast<DWORD>(std::size(localAppData)));

    std::wstring base;
    if (n > 0 && n < std::size(localAppData)) {
        base.assign(localAppData, n);
    } else {
        base = GetExeDirectory();
    }

    const std::wstring directory = base + L"\\MPV WinterStatic Edition";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\settings.ini";
}

std::wstring GetSettingsPath() {
    static std::wstring path;
    if (!path.empty()) return path;

    const std::wstring exeDir = GetExeDirectory();
    path = DirectoryIsWritable(exeDir)
        ? exeDir + L"\\settings.ini"
        : GetLocalSettingsPath();

    return path;
}

std::wstring GetResumePath() {
    const std::filesystem::path settingsPath(GetSettingsPath());
    return (settingsPath.parent_path() / L"resume.ini").wstring();
}


std::wstring GetMpvConfigPath() {
    return GetExeDirectory() + L"\\mpv.conf";
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool EnsureMpvConfigExists() {
    const std::wstring path = GetMpvConfigPath();
    if (FileExists(path)) return true;

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) return false;

    const char initialText[] =
        "# MPV WinterStatic Edition - custom libmpv configuration\r\n"
        "# Add normal mpv options here, one per line.\r\n"
        "# Restart MPV WinterStatic Edition after saving changes.\r\n"
        "#\r\n"
        "# GPU API is controlled by Options and overrides gpu-api here.\r\n";

    DWORD written = 0;
    const BOOL writeOk = WriteFile(
        file,
        initialText,
        static_cast<DWORD>(sizeof(initialText) - 1),
        &written,
        nullptr);

    if (writeOk) FlushFileBuffers(file);
    CloseHandle(file);

    return writeOk && written == static_cast<DWORD>(sizeof(initialText) - 1);
}

void OpenMpvConfig() {
    if (!EnsureMpvConfigExists()) {
        MessageBoxW(
            g_main,
            L"Could not create mpv.conf beside the player executable.",
            kAppTitle,
            MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring path = GetMpvConfigPath();
    const std::wstring parameters = L"\"" + path + L"\"";
    const HINSTANCE result = ShellExecuteW(
        g_main,
        L"open",
        L"notepad.exe",
        parameters.c_str(),
        GetExeDirectory().c_str(),
        SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(
            g_main,
            L"Could not open mpv.conf in Notepad.",
            kAppTitle,
            MB_OK | MB_ICONERROR);
    }
}

using IniSection = std::map<std::string, std::string>;
using IniData = std::map<std::string, IniSection>;

std::string TrimIniText(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

IniData ReadIniDataFromPath(const std::wstring& path) {
    IniData data;
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) return data;

    std::string section;
    std::string line;
    bool firstLine = true;

    while (std::getline(in, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }

        line = TrimIniText(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = TrimIniText(line.substr(1, line.size() - 2));
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos || section.empty()) continue;

        const std::string key = TrimIniText(line.substr(0, equals));
        const std::string value = TrimIniText(line.substr(equals + 1));
        if (!key.empty()) data[section][key] = value;
    }

    return data;
}

IniData ReadIniData() {
    return ReadIniDataFromPath(GetSettingsPath());
}

bool WriteIniDataToPath(const IniData& data, const std::wstring& path) {
    std::string bytes;
    bool firstSection = true;

    for (const auto& sectionPair : data) {
        if (!firstSection) bytes += "\r\n";
        firstSection = false;

        bytes += "[";
        bytes += sectionPair.first;
        bytes += "]\r\n";

        for (const auto& keyValue : sectionPair.second) {
            bytes += keyValue.first;
            bytes += "=";
            bytes += keyValue.second;
            bytes += "\r\n";
        }
    }

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    size_t offset = 0;

    while (offset < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(bytes.size() - offset, 1024 * 1024));
        DWORD written = 0;

        if (!WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) ||
            written == 0) {
            ok = false;
            break;
        }

        offset += written;
    }

    if (ok && !FlushFileBuffers(file)) {
        ok = false;
    }

    CloseHandle(file);
    return ok;
}

bool WriteIniData(const IniData& data) {
    return WriteIniDataToPath(data, GetSettingsPath());
}

std::wstring ReadSettingString(const wchar_t* section, const wchar_t* key,
                               const wchar_t* fallback = L"") {
    const IniData data = ReadIniData();
    const std::string sectionUtf8 = WideToUtf8(section ? std::wstring(section) : L"");
    const std::string keyUtf8 = WideToUtf8(key ? std::wstring(key) : L"");

    const auto sectionIt = data.find(sectionUtf8);
    if (sectionIt == data.end()) return fallback ? fallback : L"";

    const auto valueIt = sectionIt->second.find(keyUtf8);
    if (valueIt == sectionIt->second.end()) return fallback ? fallback : L"";

    return Utf8ToWide(valueIt->second);
}

int ReadSettingInt(const wchar_t* section, const wchar_t* key, int fallback) {
    const std::wstring value = ReadSettingString(section, key, L"");
    if (value.empty()) return fallback;

    wchar_t* end = nullptr;
    const long parsed = wcstol(value.c_str(), &end, 10);
    if (!end || end == value.c_str()) return fallback;
    return static_cast<int>(parsed);
}

void WriteSettingString(const wchar_t* section, const wchar_t* key,
                        const std::wstring& value) {
    IniData data = ReadIniData();
    const std::string sectionUtf8 = WideToUtf8(section ? std::wstring(section) : L"");
    const std::string keyUtf8 = WideToUtf8(key ? std::wstring(key) : L"");
    data[sectionUtf8][keyUtf8] = WideToUtf8(value);
    WriteIniData(data);
}

void WriteSettingInt(const wchar_t* section, const wchar_t* key, int value) {
    WriteSettingString(section, key, std::to_wstring(value));
}

KeyBinding MakeKeyBinding(UINT vk, bool ctrl = false,
                          bool alt = false, bool shift = false) {
    KeyBinding binding{};
    binding.vk = vk;
    binding.ctrl = ctrl;
    binding.alt = alt;
    binding.shift = shift;
    return binding;
}

bool SameKeyBinding(const KeyBinding& a, const KeyBinding& b) {
    return a.vk == b.vk &&
           a.ctrl == b.ctrl &&
           a.alt == b.alt &&
           a.shift == b.shift;
}

ShortcutTable DefaultShortcutBindings() {
    ShortcutTable bindings{};

    auto set = [&](ShortcutAction action, size_t slot, const KeyBinding& binding) {
        bindings[static_cast<size_t>(action)][slot] = binding;
    };

    set(ShortcutAction::OpenFile, 0, MakeKeyBinding('O', true));
    set(ShortcutAction::PlayPause, 0, MakeKeyBinding(VK_SPACE));
    set(ShortcutAction::PreviousChapter, 0, MakeKeyBinding(VK_LEFT, true));
    set(ShortcutAction::PreviousChapter, 1, MakeKeyBinding(VK_MEDIA_PREV_TRACK));
    set(ShortcutAction::NextChapter, 0, MakeKeyBinding(VK_RIGHT, true));
    set(ShortcutAction::NextChapter, 1, MakeKeyBinding(VK_MEDIA_NEXT_TRACK));
    set(ShortcutAction::SeekBackward, 0, MakeKeyBinding(VK_LEFT));
    set(ShortcutAction::SeekForward, 0, MakeKeyBinding(VK_RIGHT));
    set(ShortcutAction::VolumeUp, 0, MakeKeyBinding(VK_UP));
    set(ShortcutAction::VolumeDown, 0, MakeKeyBinding(VK_DOWN));
    set(ShortcutAction::Mute, 0, MakeKeyBinding('M'));
    set(ShortcutAction::ToggleFullscreen, 0, MakeKeyBinding('F'));
    set(ShortcutAction::ToggleFullscreen, 1, MakeKeyBinding(VK_RETURN));
    set(ShortcutAction::ExitFullscreen, 0, MakeKeyBinding(VK_ESCAPE));
    set(ShortcutAction::TogglePlaylist, 0, MakeKeyBinding('P'));
    set(ShortcutAction::PreviousPlaylist, 0, MakeKeyBinding(VK_PRIOR));
    set(ShortcutAction::PreviousPlaylist, 1, MakeKeyBinding(VK_UP, true));
    set(ShortcutAction::NextPlaylist, 0, MakeKeyBinding(VK_NEXT));
    set(ShortcutAction::NextPlaylist, 1, MakeKeyBinding(VK_DOWN, true));
    set(ShortcutAction::CycleAudio, 0, MakeKeyBinding('A'));
    set(ShortcutAction::CycleSubtitle, 0, MakeKeyBinding('S'));
    set(ShortcutAction::MediaInfo, 0, MakeKeyBinding('I'));

    return bindings;
}

int EncodeKeyBinding(const KeyBinding& binding) {
    if (binding.vk == 0) return 0;

    int encoded = static_cast<int>(binding.vk & 0xFFFFu);
    if (binding.ctrl) encoded |= (1 << 16);
    if (binding.alt) encoded |= (1 << 17);
    if (binding.shift) encoded |= (1 << 18);
    return encoded;
}

KeyBinding DecodeKeyBinding(int encoded) {
    if (encoded <= 0) return {};

    KeyBinding binding{};
    binding.vk = static_cast<UINT>(encoded & 0xFFFF);
    binding.ctrl = (encoded & (1 << 16)) != 0;
    binding.alt = (encoded & (1 << 17)) != 0;
    binding.shift = (encoded & (1 << 18)) != 0;
    return binding;
}

std::wstring ShortcutSettingName(size_t actionIndex, size_t slot) {
    std::wstring key = kShortcutActionInfo[actionIndex].settingKey;
    key += slot == 0 ? L"Primary" : L"Alternate";
    return key;
}

void LoadShortcutSettings() {
    g_shortcuts = DefaultShortcutBindings();

    const IniData data = ReadIniData();
    const auto sectionIt = data.find("Shortcuts");
    if (sectionIt == data.end()) return;

    for (size_t action = 0; action < kShortcutActionCount; ++action) {
        for (size_t slot = 0; slot < kShortcutSlotCount; ++slot) {
            const std::wstring keyWide = ShortcutSettingName(action, slot);
            const std::string key = WideToUtf8(keyWide);
            const auto valueIt = sectionIt->second.find(key);
            if (valueIt == sectionIt->second.end()) continue;

            try {
                const int encoded = std::stoi(valueIt->second);
                if (encoded >= 0) {
                    g_shortcuts[action][slot] = DecodeKeyBinding(encoded);
                }
            } catch (...) {
                // Keep the default for malformed hand-edited values.
            }
        }
    }

    // 0.2.49 shipped Media Previous/Next as the sole chapter bindings.
    // Migrate that exact untouched default pair to the 0.3.1 defaults while
    // preserving any user-customized chapter shortcuts.
    const size_t prev = static_cast<size_t>(ShortcutAction::PreviousChapter);
    const size_t next = static_cast<size_t>(ShortcutAction::NextChapter);
    if (SameKeyBinding(g_shortcuts[prev][0], MakeKeyBinding(VK_MEDIA_PREV_TRACK)) &&
        g_shortcuts[prev][1].vk == 0) {
        g_shortcuts[prev][0] = MakeKeyBinding(VK_LEFT, true);
        g_shortcuts[prev][1] = MakeKeyBinding(VK_MEDIA_PREV_TRACK);
    }
    if (SameKeyBinding(g_shortcuts[next][0], MakeKeyBinding(VK_MEDIA_NEXT_TRACK)) &&
        g_shortcuts[next][1].vk == 0) {
        g_shortcuts[next][0] = MakeKeyBinding(VK_RIGHT, true);
        g_shortcuts[next][1] = MakeKeyBinding(VK_MEDIA_NEXT_TRACK);
    }
}

void SaveShortcutSettings() {
    IniData data = ReadIniData();
    IniSection& section = data["Shortcuts"];

    for (size_t action = 0; action < kShortcutActionCount; ++action) {
        for (size_t slot = 0; slot < kShortcutSlotCount; ++slot) {
            const std::wstring keyWide = ShortcutSettingName(action, slot);
            section[WideToUtf8(keyWide)] =
                std::to_string(EncodeKeyBinding(g_shortcuts[action][slot]));
        }
    }

    WriteIniData(data);
}

std::wstring ShortcutKeyName(UINT vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }

    switch (vk) {
    case VK_SPACE: return L"Space";
    case VK_RETURN: return L"Enter";
    case VK_ESCAPE: return L"Esc";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_BACK: return L"Backspace";
    case VK_TAB: return L"Tab";
    case VK_MEDIA_PREV_TRACK: return L"Media Previous";
    case VK_MEDIA_NEXT_TRACK: return L"Media Next";
    case VK_MEDIA_PLAY_PAUSE: return L"Media Play/Pause";
    case VK_MEDIA_STOP: return L"Media Stop";
    case VK_VOLUME_MUTE: return L"Volume Mute";
    case VK_VOLUME_DOWN: return L"Volume Down";
    case VK_VOLUME_UP: return L"Volume Up";
    }

    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scan != 0) {
        LONG keyData = static_cast<LONG>(scan << 16);
        switch (vk) {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
            keyData |= (1 << 24);
            break;
        }
        wchar_t name[64]{};
        if (GetKeyNameTextW(keyData, name, static_cast<int>(std::size(name))) > 0) {
            return name;
        }
    }

    wchar_t fallback[24]{};
    swprintf_s(fallback, L"Key 0x%02X", static_cast<unsigned int>(vk));
    return fallback;
}

std::wstring KeyBindingText(const KeyBinding& binding) {
    if (binding.vk == 0) return L"None";

    std::wstring text;
    if (binding.ctrl) text += L"Ctrl+";
    if (binding.alt) text += L"Alt+";
    if (binding.shift) text += L"Shift+";
    text += ShortcutKeyName(binding.vk);
    return text;
}

std::wstring ShortcutSummary(ShortcutAction action) {
    const auto& pair = g_shortcuts[static_cast<size_t>(action)];
    std::wstring text;

    for (size_t slot = 0; slot < kShortcutSlotCount; ++slot) {
        if (pair[slot].vk == 0) continue;
        if (!text.empty()) text += L" / ";
        text += KeyBindingText(pair[slot]);
    }
    return text;
}

std::wstring ShortcutMenuLabel(const wchar_t* label, ShortcutAction action) {
    std::wstring text = label ? label : L"";
    const std::wstring shortcuts = ShortcutSummary(action);
    if (!shortcuts.empty()) {
        text += L"\t";
        text += shortcuts;
    }
    return text;
}

void LoadResumePositions() {
    g_resumePositions.clear();
    g_resumePositionsDirty = false;

    const IniData data = ReadIniDataFromPath(GetResumePath());
    for (const auto& sectionPair : data) {
        const auto pathIt = sectionPair.second.find("Path");
        const auto positionIt = sectionPair.second.find("PositionMs");
        if (pathIt == sectionPair.second.end() ||
            positionIt == sectionPair.second.end()) {
            continue;
        }

        const std::wstring path = Utf8ToWide(pathIt->second);
        if (path.empty()) continue;

        try {
            const long long positionMs = std::stoll(positionIt->second);
            if (positionMs >= 0) {
                g_resumePositions[path] =
                    static_cast<double>(positionMs) / 1000.0;
            }
        } catch (...) {
            // Ignore malformed entries rather than making resume state fatal.
        }
    }
}

bool FlushResumePositions() {
    if (!g_resumePositionsDirty) return true;

    IniData data;
    size_t index = 0;
    for (const auto& entry : g_resumePositions) {
        const std::string section = "File" + std::to_string(++index);
        data[section]["Path"] = WideToUtf8(entry.first);
        const long long positionMs =
            static_cast<long long>(std::llround(entry.second * 1000.0));
        data[section]["PositionMs"] = std::to_string(positionMs);
    }

    if (!WriteIniDataToPath(data, GetResumePath())) return false;
    g_resumePositionsDirty = false;
    return true;
}


bool RectTouchesAnyMonitor(const RECT& r) {
    return MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != nullptr;
}

void LoadSettings() {
    g_singleClickDelayMs = static_cast<UINT>(
        std::clamp(ReadSettingInt(L"General", L"ClickDelayMs", 64), 20, 500));
    g_volumePercent =
        std::clamp(ReadSettingInt(L"General", L"Volume", kDefaultVolume), 0, 100);
    g_muted = ReadSettingInt(L"General", L"Muted", 0) != 0;
    g_autoPlayNextFile =
        ReadSettingInt(L"General", L"AutoPlayNextFile", 0) != 0;
    g_exitFullscreenOnPlaybackEnd =
        ReadSettingInt(L"General", L"ExitFullscreenAtEnd", 1) != 0;
    g_osdFontSize =
        std::clamp(ReadSettingInt(L"General", L"OsdFontSize", 72), 24, 180);
    g_subtitleFontSize =
        std::clamp(ReadSettingInt(L"Subtitles", L"FontSize", 55), 20, 120);

    const int osd = ReadSettingInt(L"General", L"OsdPosition", 1);
    g_osdPosition = osd == 1 ? OsdPosition::TopLeft : OsdPosition::GoldenCenter;

    const int playlistStartOsd =
        std::clamp(ReadSettingInt(L"General", L"PlaylistStartOsd", 0), 0, 2);
    g_playlistStartOsd = static_cast<PlaylistStartOsd>(playlistStartOsd);

    const int gpuApi =
        std::clamp(ReadSettingInt(L"Video", L"GpuApi", 0), 0, 3);
    g_gpuApi = static_cast<GpuApi>(gpuApi);

    LoadShortcutSettings();

    // Preferred languages are optional fallbacks. A remembered manual
    // selection takes priority when a matching track exists.
    g_preferredAudioLanguage =
        ReadSettingString(L"Tracks", L"AudioLanguage", L"");
    g_preferredSubtitleLanguage =
        ReadSettingString(L"Tracks", L"SubtitleLanguage", L"");

    g_haveRememberedAudio =
        ReadSettingInt(L"Tracks", L"LastAudioValid", 0) != 0;
    g_rememberedAudioLanguage =
        ReadSettingString(L"Tracks", L"LastAudioLanguage", L"");
    g_rememberedAudioTitle =
        ReadSettingString(L"Tracks", L"LastAudioTitle", L"");

    g_haveRememberedSubtitle =
        ReadSettingInt(L"Tracks", L"LastSubtitleValid", 0) != 0;
    g_rememberedSubtitleOff =
        ReadSettingInt(L"Tracks", L"LastSubtitleOff", 0) != 0;
    g_rememberedSubtitleLanguage =
        ReadSettingString(L"Tracks", L"LastSubtitleLanguage", L"");
    g_rememberedSubtitleTitle =
        ReadSettingString(L"Tracks", L"LastSubtitleTitle", L"");

    g_startMaximized = ReadSettingInt(L"Window", L"Maximized", 1) != 0;

    const int x = ReadSettingInt(L"Window", L"X", INT_MIN);
    const int y = ReadSettingInt(L"Window", L"Y", INT_MIN);
    const int w = ReadSettingInt(L"Window", L"Width", 0);
    const int h = ReadSettingInt(L"Window", L"Height", 0);

    if (x != INT_MIN && y != INT_MIN && w >= 650 && h >= 400) {
        RECT r{ x, y, x + w, y + h };
        if (RectTouchesAnyMonitor(r)) {
            g_savedWindowRect = r;
            g_haveSavedWindowRect = true;
        }
    }

}

void SaveAudioState() {
    WriteSettingInt(L"General", L"Volume", g_volumePercent);
    WriteSettingInt(L"General", L"Muted", g_muted ? 1 : 0);
}

void SaveSettings() {
    WriteSettingInt(L"General", L"ClickDelayMs",
                    static_cast<int>(g_singleClickDelayMs));
    SaveAudioState();
    WriteSettingInt(L"General", L"AutoPlayNextFile",
                    g_autoPlayNextFile ? 1 : 0);
    WriteSettingInt(L"General", L"ExitFullscreenAtEnd",
                    g_exitFullscreenOnPlaybackEnd ? 1 : 0);
    WriteSettingInt(L"General", L"OsdFontSize", g_osdFontSize);
    WriteSettingInt(L"Subtitles", L"FontSize", g_subtitleFontSize);
    WriteSettingInt(L"General", L"OsdPosition",
                    g_osdPosition == OsdPosition::TopLeft ? 1 : 0);
    WriteSettingInt(L"General", L"PlaylistStartOsd",
                    static_cast<int>(g_playlistStartOsd));
    WriteSettingInt(L"Video", L"GpuApi", static_cast<int>(g_gpuApi));
    WriteSettingString(L"Tracks", L"AudioLanguage", g_preferredAudioLanguage);
    WriteSettingString(L"Tracks", L"SubtitleLanguage", g_preferredSubtitleLanguage);

    WriteSettingInt(L"Tracks", L"LastAudioValid", g_haveRememberedAudio ? 1 : 0);
    WriteSettingString(L"Tracks", L"LastAudioLanguage", g_rememberedAudioLanguage);
    WriteSettingString(L"Tracks", L"LastAudioTitle", g_rememberedAudioTitle);

    WriteSettingInt(L"Tracks", L"LastSubtitleValid",
                    g_haveRememberedSubtitle ? 1 : 0);
    WriteSettingInt(L"Tracks", L"LastSubtitleOff",
                    g_rememberedSubtitleOff ? 1 : 0);
    WriteSettingString(L"Tracks", L"LastSubtitleLanguage",
                       g_rememberedSubtitleLanguage);
    WriteSettingString(L"Tracks", L"LastSubtitleTitle",
                       g_rememberedSubtitleTitle);

    WINDOWPLACEMENT wp{ sizeof(WINDOWPLACEMENT) };
    if (g_fullscreen && g_windowedPlacement.length == sizeof(WINDOWPLACEMENT)) {
        wp = g_windowedPlacement;
    } else if (g_main) {
        GetWindowPlacement(g_main, &wp);
    }

    const bool maximized =
        wp.showCmd == SW_SHOWMAXIMIZED ||
        (wp.showCmd == SW_SHOWMINIMIZED && (wp.flags & WPF_RESTORETOMAXIMIZED));
    WriteSettingInt(L"Window", L"Maximized", maximized ? 1 : 0);

    const RECT r = wp.rcNormalPosition;
    const int width = static_cast<int>(r.right - r.left);
    const int height = static_cast<int>(r.bottom - r.top);
    if (width >= 650 && height >= 400) {
        WriteSettingInt(L"Window", L"X", static_cast<int>(r.left));
        WriteSettingInt(L"Window", L"Y", static_cast<int>(r.top));
        WriteSettingInt(L"Window", L"Width", width);
        WriteSettingInt(L"Window", L"Height", height);
    }

}

std::wstring FormatTime(double seconds) {
    if (!(seconds >= 0.0)) seconds = 0.0;
    const int whole = static_cast<int>(seconds + 0.5);
    const int hours = whole / 3600;
    const int minutes = (whole % 3600) / 60;
    const int secs = whole % 60;
    wchar_t buf[32]{};
    if (hours > 0) swprintf_s(buf, L"%d:%02d:%02d", hours, minutes, secs);
    else swprintf_s(buf, L"%02d:%02d", minutes, secs);
    return buf;
}

std::string UpperAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return s;
}

void EnableDarkModeForApp() {
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
    using SetPreferredAppModeFn = PreferredAppMode (WINAPI*)(PreferredAppMode);
    using FlushMenuThemesFn = void (WINAPI*)();
    auto setPreferred = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(ux, MAKEINTRESOURCEA(135)));
    auto flushMenus = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(ux, MAKEINTRESOURCEA(136)));
    if (setPreferred) setPreferred(PreferredAppMode::ForceDark);
    if (flushMenus) flushMenus();
    FreeLibrary(ux);
}

void ApplyDarkTheme(HWND hwnd) {
    if (!hwnd) return;
    BOOL enabled = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled)))) {
        DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
    }
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (ux) {
        using AllowDarkModeForWindowFn = BOOL (WINAPI*)(HWND, BOOL);
        auto allowDark = reinterpret_cast<AllowDarkModeForWindowFn>(GetProcAddress(ux, MAKEINTRESOURCEA(133)));
        if (allowDark) allowDark(hwnd, TRUE);
        FreeLibrary(ux);
    }
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

void ApplyFont(HWND hwnd, bool useSmallFont = false) {
    if (hwnd) {
        HFONT font = useSmallFont ? g_smallFont : g_font;
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

template <typename T>
bool LoadProc(T& target, const char* name) {
    target = reinterpret_cast<T>(GetProcAddress(g_mpvApi.dll, name));
    return target != nullptr;
}

std::wstring MpvError(int code) {
    if (g_mpvApi.error_string) {
        const char* s = g_mpvApi.error_string(code);
        if (s) return Utf8ToWide(s);
    }
    wchar_t buf[64]{};
    swprintf_s(buf, L"libmpv error %d", code);
    return buf;
}

bool MpvGetFlag(const char* name, bool fallback = false) {
    if (!g_mpvReady || !g_mpvApi.get_property) return fallback;
    int value = fallback ? 1 : 0;
    if (g_mpvApi.get_property(g_mpv, name, MPV_FORMAT_FLAG, &value) < 0) return fallback;
    return value != 0;
}

double MpvGetDouble(const char* name, double fallback = 0.0) {
    if (!g_mpvReady || !g_mpvApi.get_property) return fallback;
    double value = fallback;
    if (g_mpvApi.get_property(g_mpv, name, MPV_FORMAT_DOUBLE, &value) < 0) return fallback;
    return value;
}

int64_t MpvGetInt64(const char* name, int64_t fallback = 0) {
    if (!g_mpvReady || !g_mpvApi.get_property) return fallback;
    int64_t value = fallback;
    if (g_mpvApi.get_property(g_mpv, name, MPV_FORMAT_INT64, &value) < 0) return fallback;
    return value;
}

std::string MpvGetString(const char* name) {
    if (!g_mpvReady || !g_mpvApi.get_property || !g_mpvApi.free_fn) return {};
    char* value = nullptr;
    if (g_mpvApi.get_property(g_mpv, name, MPV_FORMAT_STRING, &value) < 0 || !value) return {};
    std::string result(value);
    g_mpvApi.free_fn(value);
    return result;
}

bool MpvSetFlag(const char* name, bool value) {
    if (!g_mpvReady) return false;
    int v = value ? 1 : 0;
    return g_mpvApi.set_property(g_mpv, name, MPV_FORMAT_FLAG, &v) >= 0;
}

bool MpvSetDouble(const char* name, double value) {
    if (!g_mpvReady) return false;
    return g_mpvApi.set_property(g_mpv, name, MPV_FORMAT_DOUBLE, &value) >= 0;
}

bool ResumePositionShouldBeKept(double position, double duration) {
    if (!std::isfinite(position) || position < kResumeMinSeconds) return false;
    if (duration <= 0.0 || !std::isfinite(duration)) return true;

    const double endMargin = std::min(
        kResumeEndMarginMaxSeconds,
        std::max(2.0, duration * 0.05));
    return position < duration - endMargin;
}

void SaveCurrentResumePosition() {
    if (g_currentMediaPath.empty()) return;

    bool changed = false;
    const bool keep = ResumePositionShouldBeKept(g_timePos, g_duration);
    const auto existing = g_resumePositions.find(g_currentMediaPath);

    if (keep) {
        if (existing == g_resumePositions.end() ||
            std::abs(existing->second - g_timePos) >= 0.05) {
            g_resumePositions[g_currentMediaPath] = g_timePos;
            changed = true;
        }
    } else if (existing != g_resumePositions.end()) {
        g_resumePositions.erase(existing);
        changed = true;
    }

    if (changed) {
        g_resumePositionsDirty = true;
        FlushResumePositions();
    }
}

void RestoreCurrentResumePosition() {
    if (!g_mpvReady || g_currentMediaPath.empty()) return;

    const auto saved = g_resumePositions.find(g_currentMediaPath);
    if (saved == g_resumePositions.end()) return;

    const double duration = std::max(0.0, MpvGetDouble("duration", 0.0));
    if (!ResumePositionShouldBeKept(saved->second, duration)) {
        g_resumePositions.erase(saved);
        g_resumePositionsDirty = true;
        FlushResumePositions();
        return;
    }

    double position = saved->second;
    if (duration > 0.0) position = std::clamp(position, 0.0, duration);

    if (MpvSetDouble("time-pos", position)) {
        g_timePos = position;
    }
}


bool MpvSetString(const char* name, const std::wstring& value) {
    if (!g_mpvReady || !g_mpvApi.set_property_string) return false;
    const std::string utf8 = WideToUtf8(value);
    return g_mpvApi.set_property_string(g_mpv, name, utf8.c_str()) >= 0;
}

bool MpvCommand(const std::vector<std::string>& args) {
    if (!g_mpvReady || args.empty()) return false;
    std::vector<const char*> p;
    p.reserve(args.size() + 1);
    for (const auto& s : args) p.push_back(s.c_str());
    p.push_back(nullptr);
    return g_mpvApi.command(g_mpv, p.data()) >= 0;
}


std::string MpvEscapeAssFallback(std::string text) {
    // Only used if libmpv's escape-ass command is unavailable. Preserve normal
    // filename text while neutralising the ASS control characters that could
    // otherwise turn a media title into formatting instructions.
    for (char& ch : text) {
        if (ch == '{') ch = '(';
        else if (ch == '}') ch = ')';
        else if (ch == '\\') ch = '/';
        else if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    return text;
}

std::string MpvEscapeAss(const std::string& text) {
    if (!g_mpvReady || !g_mpvApi.command_node || !g_mpvApi.free_node_contents) {
        return MpvEscapeAssFallback(text);
    }

    mpv_node values[2]{};
    values[0].format = MPV_FORMAT_STRING;
    values[0].u.string = const_cast<char*>("escape-ass");
    values[1].format = MPV_FORMAT_STRING;
    values[1].u.string = const_cast<char*>(text.c_str());

    mpv_node_list list{};
    list.num = 2;
    list.values = values;

    mpv_node args{};
    args.format = MPV_FORMAT_NODE_ARRAY;
    args.u.list = &list;

    mpv_node result{};
    if (g_mpvApi.command_node(g_mpv, &args, &result) < 0) {
        return MpvEscapeAssFallback(text);
    }

    std::string escaped = MpvEscapeAssFallback(text);
    if (result.format == MPV_FORMAT_STRING && result.u.string) {
        escaped = result.u.string;
    }
    g_mpvApi.free_node_contents(&result);
    return escaped;
}

int MpvSetTextOverlay(const std::string& format, const std::string& data) {
    if (!g_mpvReady || !g_mpvApi.command) return -12; // MPV_ERROR_COMMAND

    // The documented osd-overlay interface prefers named arguments, but the
    // bundled libmpv rejects our MPV_FORMAT_NODE_MAP call with
    // MPV_ERROR_INVALID_PARAMETER. Use the ordinary array API here instead.
    // Array arguments are already split and passed directly to mpv's argument
    // parsers, so DATA can contain spaces, newlines, and ASS backslashes without
    // input.conf quoting/escaping. Keep every positional field explicit so this
    // matches the command signature used by the bundled libmpv.
    const std::string id = std::to_string(kPlaylistOverlayId);
    const char* args[] = {
        "osd-overlay",
        id.c_str(),
        format.c_str(),
        data.c_str(),
        "0",       // res_x: derive from res_y / display aspect
        "720",     // res_y: documented default PlayResY
        "0",       // z
        "no",      // hidden
        "no",      // compute_bounds
        nullptr
    };
    return g_mpvApi.command(g_mpv, args);
}

std::wstring MpvErrorText(int code) {
    if (code >= 0) return {};
    if (g_mpvApi.error_string) {
        const char* text = g_mpvApi.error_string(code);
        if (text && *text) return Utf8ToWide(text);
    }
    return L"mpv error " + std::to_wstring(code);
}

void RemovePlaylistOverlay() {
    if (!g_mpvReady) return;
    MpvSetTextOverlay("none", "");
}

void UpdateOsdPosition() {
    if (!g_mpvReady || !g_video) return;

    RECT r{};
    GetClientRect(g_video, &r);
    const int height = std::max(0, static_cast<int>(r.bottom - r.top));
    if (height <= 0) return;

    auto SetOsdOption = [](const char* name, const std::string& value) {
        MpvCommand({"set", name, value});
    };

    const UINT dpi = GetDpiForWindow(g_video);
    const int scaleDpi = static_cast<int>(dpi ? dpi : 96);
    const int fontPx = std::max(24, MulDiv(g_osdFontSize, scaleDpi, 96));

    // libass leaves visible space above a top-aligned glyph. The previous
    // compensation was far too small; the 0.2.11 screenshot showed roughly
    // 21 px of extra apparent top spacing with a 72 px OSD at 96 DPI.
    const int glyphTopPadding =
        std::max(4, static_cast<int>(static_cast<double>(fontPx) * 0.29 + 0.5));

    if (g_mediaInfoVisible) {
        // The stats overlay is a large multi-line block, so don't inherit the
        // normal Golden Centre position. Give Media Info its own compact,
        // DPI-scaled top-left safe area so the full block has room below it.
        const int statsInset = std::max(12, MulDiv(18, scaleDpi, 96));

        SetOsdOption("osd-align-x", "left");
        SetOsdOption("osd-align-y", "top");
        SetOsdOption("osd-margin-x", std::to_string(statsInset));
        SetOsdOption("osd-margin-y", std::to_string(statsInset));
    } else if (g_osdPosition == OsdPosition::TopLeft) {
        // Aim for the visible text to be equally inset from the top and left.
        const int visibleInset = std::max(18, MulDiv(28, scaleDpi, 96));
        const int rawTopMargin = std::max(2, visibleInset - glyphTopPadding);

        SetOsdOption("osd-align-x", "left");
        SetOsdOption("osd-align-y", "top");
        SetOsdOption("osd-margin-x", std::to_string(visibleInset));
        SetOsdOption("osd-margin-y", std::to_string(rawTopMargin));
    } else {
        SetOsdOption("osd-align-x", "center");
        SetOsdOption("osd-align-y", "top");
        SetOsdOption("osd-margin-x", "0");

        // Place the *visible* OSD at the golden-ratio conjugate:
        // 38.2% down from the top, visually around one-third of the screen.
        const int visibleGoldenY =
            std::max(24, static_cast<int>(static_cast<double>(height) * 0.382 + 0.5));
        const int rawGoldenMargin =
            std::max(2, visibleGoldenY - glyphTopPadding);

        SetOsdOption("osd-margin-y", std::to_string(rawGoldenMargin));
    }
}

void ShowOsdText(const std::wstring& text, int durationMs = 750) {
    if (!g_mpvReady || text.empty()) return;

    // Custom playlist overlays sit above mpv's builtin OSD. Temporarily remove
    // a persistent playlist so volume/seek/speed messages remain readable,
    // then let the restore timer rebuild it after the transient message ends.
    if (g_playlistOsdVisible) RemovePlaylistOverlay();
    UpdateOsdPosition();

    MpvCommand({"show-text", WideToUtf8(text), std::to_string(durationMs)});

    if (g_playlistOsdVisible && g_main) {
        KillTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE);
        SetTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE,
                 static_cast<UINT>(std::max(50, durationMs + 60)), nullptr);
    }
}

const mpv_node* MapValue(const mpv_node& node, const char* key) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list || !node.u.list->keys) return nullptr;
    for (int i = 0; i < node.u.list->num; ++i) {
        if (node.u.list->keys[i] && strcmp(node.u.list->keys[i], key) == 0) {
            return &node.u.list->values[i];
        }
    }
    return nullptr;
}

std::string NodeString(const mpv_node* n) {
    return (n && n->format == MPV_FORMAT_STRING && n->u.string) ? std::string(n->u.string) : std::string();
}

int64_t NodeInt(const mpv_node* n, int64_t fallback = 0) {
    return (n && n->format == MPV_FORMAT_INT64) ? n->u.int64 : fallback;
}

double NodeDouble(const mpv_node* n, double fallback = 0.0) {
    if (!n) return fallback;
    if (n->format == MPV_FORMAT_DOUBLE) return n->u.double_;
    if (n->format == MPV_FORMAT_INT64) return static_cast<double>(n->u.int64);
    return fallback;
}

bool NodeFlag(const mpv_node* n, bool fallback = false) {
    return (n && n->format == MPV_FORMAT_FLAG) ? n->u.flag != 0 : fallback;
}

bool IsStatsOverlayAvailable() {
    if (!g_mpvReady || !g_mpvApi.free_node_contents) return false;

    mpv_node root{};
    if (g_mpvApi.get_property(
            g_mpv, "input-bindings", MPV_FORMAT_NODE, &root) < 0) {
        // Older/unusual libmpv builds may not expose input-bindings. In that
        // case, don't reject Media Info solely on the failed capability check.
        return true;
    }

    bool found = false;
    if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
        for (int i = 0; i < root.u.list->num; ++i) {
            const mpv_node& item = root.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP) continue;

            const std::string owner = NodeString(MapValue(item, "owner"));
            const std::string cmd = NodeString(MapValue(item, "cmd"));

            if (owner == "stats" ||
                cmd.find("script-binding stats/") != std::string::npos ||
                cmd.find("script-message-to stats ") != std::string::npos) {
                found = true;
                break;
            }
        }
    }

    g_mpvApi.free_node_contents(&root);
    return found;
}

std::vector<TrackInfo> GetTracks(const char* typeFilter = nullptr) {
    std::vector<TrackInfo> tracks;
    if (!g_mpvReady || !g_mpvApi.free_node_contents) return tracks;
    mpv_node root{};
    if (g_mpvApi.get_property(g_mpv, "track-list", MPV_FORMAT_NODE, &root) < 0) return tracks;
    if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
        for (int i = 0; i < root.u.list->num; ++i) {
            const mpv_node& item = root.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP) continue;
            TrackInfo t;
            t.type = NodeString(MapValue(item, "type"));
            if (typeFilter && t.type != typeFilter) continue;
            t.id = NodeInt(MapValue(item, "id"), -1);
            if (t.id < 0) continue;
            t.lang = NodeString(MapValue(item, "lang"));
            t.title = NodeString(MapValue(item, "title"));
            t.selected = NodeFlag(MapValue(item, "selected"), false);
            tracks.push_back(std::move(t));
        }
    }
    g_mpvApi.free_node_contents(&root);
    return tracks;
}

std::vector<PlaylistEntry> GetPlaylistEntries() {
    std::vector<PlaylistEntry> entries;
    if (!g_mpvReady || !g_mpvApi.free_node_contents) return entries;

    mpv_node root{};
    if (g_mpvApi.get_property(g_mpv, "playlist", MPV_FORMAT_NODE, &root) < 0) {
        return entries;
    }

    if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
        entries.reserve(static_cast<size_t>(root.u.list->num));
        for (int i = 0; i < root.u.list->num; ++i) {
            const mpv_node& item = root.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP) continue;

            PlaylistEntry entry;
            entry.filename = NodeString(MapValue(item, "filename"));
            entry.title = NodeString(MapValue(item, "title"));
            entry.current = NodeFlag(MapValue(item, "current"), false);
            entry.playing = NodeFlag(MapValue(item, "playing"), false);
            entries.push_back(std::move(entry));
        }
    }

    g_mpvApi.free_node_contents(&root);
    return entries;
}

std::wstring PlaylistEntryLabel(const PlaylistEntry& entry, size_t index) {
    std::wstring label;
    if (!entry.title.empty()) {
        label = Utf8ToWide(entry.title);
    } else {
        const std::wstring full = Utf8ToWide(entry.filename);
        if (!full.empty()) {
            const std::filesystem::path path(full);
            label = path.filename().wstring();
            if (label.empty()) label = full;
        }
    }
    if (label.empty()) label = L"Untitled";
    if (label.size() > 100) {
        label.resize(97);
        label += L"...";
    }

    // Win32 menus treat '&' as a mnemonic marker. Escape literal ampersands.
    size_t pos = 0;
    while ((pos = label.find(L'&', pos)) != std::wstring::npos) {
        label.insert(pos, 1, L'&');
        pos += 2;
    }

    return std::to_wstring(index + 1) + L". " + label;
}

std::vector<double> GetChapterTimes() {
    std::vector<double> chapters;
    if (!g_mpvReady || !g_mpvApi.free_node_contents) return chapters;

    mpv_node root{};
    if (g_mpvApi.get_property(g_mpv, "chapter-list", MPV_FORMAT_NODE, &root) < 0) {
        return chapters;
    }

    if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
        chapters.reserve(static_cast<size_t>(root.u.list->num));
        for (int i = 0; i < root.u.list->num; ++i) {
            const mpv_node& item = root.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP) continue;

            const double time = NodeDouble(MapValue(item, "time"), -1.0);
            if (time >= 0.0) chapters.push_back(time);
        }
    }

    g_mpvApi.free_node_contents(&root);
    std::sort(chapters.begin(), chapters.end());
    return chapters;
}

void RefreshChapterMarkers() {
    const std::vector<double> chapters = GetChapterTimes();

    bool changed = chapters.size() != g_chapterTimes.size();
    if (!changed) {
        for (size_t i = 0; i < chapters.size(); ++i) {
            if (std::abs(chapters[i] - g_chapterTimes[i]) > 0.001) {
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        g_chapterTimes = chapters;
        if (g_seek) InvalidateRect(g_seek, nullptr, FALSE);
    }
}

std::wstring TrackShortLabel(const TrackInfo& t) {
    if (!t.lang.empty()) return Utf8ToWide(UpperAscii(t.lang));
    if (!t.title.empty()) return Utf8ToWide(t.title);
    wchar_t buf[32]{};
    swprintf_s(buf, L"#%lld", static_cast<long long>(t.id));
    return buf;
}

std::wstring TrackMenuLabel(const TrackInfo& t) {
    std::wstring lang = t.lang.empty() ? L"" : Utf8ToWide(UpperAscii(t.lang));
    std::wstring title = t.title.empty() ? L"" : Utf8ToWide(t.title);
    wchar_t idbuf[32]{};
    swprintf_s(idbuf, L"Track %lld", static_cast<long long>(t.id));
    if (!lang.empty() && !title.empty()) return lang + L" - " + title;
    if (!title.empty()) return title;
    if (!lang.empty()) return lang;
    return idbuf;
}

bool AsciiEqualInsensitive(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ac = static_cast<unsigned char>(a[i]);
        unsigned char bc = static_cast<unsigned char>(b[i]);
        if (ac >= 'A' && ac <= 'Z') ac = static_cast<unsigned char>(ac - 'A' + 'a');
        if (bc >= 'A' && bc <= 'Z') bc = static_cast<unsigned char>(bc - 'A' + 'a');
        if (ac != bc) return false;
    }
    return true;
}

void SaveRememberedTrackSettings() {
    WriteSettingInt(L"Tracks", L"LastAudioValid", g_haveRememberedAudio ? 1 : 0);
    WriteSettingString(L"Tracks", L"LastAudioLanguage", g_rememberedAudioLanguage);
    WriteSettingString(L"Tracks", L"LastAudioTitle", g_rememberedAudioTitle);

    WriteSettingInt(L"Tracks", L"LastSubtitleValid",
                    g_haveRememberedSubtitle ? 1 : 0);
    WriteSettingInt(L"Tracks", L"LastSubtitleOff",
                    g_rememberedSubtitleOff ? 1 : 0);
    WriteSettingString(L"Tracks", L"LastSubtitleLanguage",
                       g_rememberedSubtitleLanguage);
    WriteSettingString(L"Tracks", L"LastSubtitleTitle",
                       g_rememberedSubtitleTitle);
}

void RememberAudioTrack(const TrackInfo& track) {
    g_haveRememberedAudio = true;
    g_rememberedAudioLanguage = Utf8ToWide(track.lang);
    g_rememberedAudioTitle = Utf8ToWide(track.title);
    SaveRememberedTrackSettings();
}

void RememberSubtitleTrack(const TrackInfo& track) {
    g_haveRememberedSubtitle = true;
    g_rememberedSubtitleOff = false;
    g_rememberedSubtitleLanguage = Utf8ToWide(track.lang);
    g_rememberedSubtitleTitle = Utf8ToWide(track.title);
    SaveRememberedTrackSettings();
}

void RememberSubtitlesOff() {
    g_haveRememberedSubtitle = true;
    g_rememberedSubtitleOff = true;
    g_rememberedSubtitleLanguage.clear();
    g_rememberedSubtitleTitle.clear();
    SaveRememberedTrackSettings();
}

int FindRememberedTrackIndex(const std::vector<TrackInfo>& tracks,
                             const std::wstring& rememberedLanguage,
                             const std::wstring& rememberedTitle) {
    const std::string language = WideToUtf8(rememberedLanguage);
    const std::string title = WideToUtf8(rememberedTitle);

    // Strongest match: title plus language (when both were available).
    if (!title.empty()) {
        for (size_t i = 0; i < tracks.size(); ++i) {
            const bool titleMatches = tracks[i].title == title;
            const bool languageMatches =
                language.empty() || AsciiEqualInsensitive(tracks[i].lang, language);
            if (titleMatches && languageMatches) {
                return static_cast<int>(i);
            }
        }

        // Some files omit or change the language tag while keeping the same
        // descriptive title, so title-only is the next safest fallback.
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].title == title) {
                return static_cast<int>(i);
            }
        }
    }

    // Finally fall back to the remembered language. If even that does not
    // exist, leave mpv's preferred-language/default choice untouched.
    if (!language.empty()) {
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (AsciiEqualInsensitive(tracks[i].lang, language)) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

void ApplyRememberedTrackSelections() {
    if (!g_mpvReady) return;

    if (g_haveRememberedAudio) {
        const auto audioTracks = GetTracks("audio");
        const int index = FindRememberedTrackIndex(
            audioTracks, g_rememberedAudioLanguage, g_rememberedAudioTitle);

        if (index >= 0) {
            int64_t id = audioTracks[static_cast<size_t>(index)].id;
            g_mpvApi.set_property(g_mpv, "aid", MPV_FORMAT_INT64, &id);
        }
    }

    if (g_haveRememberedSubtitle) {
        if (g_rememberedSubtitleOff) {
            g_mpvApi.set_property_string(g_mpv, "sid", "no");
        } else {
            const auto subtitleTracks = GetTracks("sub");
            const int index = FindRememberedTrackIndex(
                subtitleTracks,
                g_rememberedSubtitleLanguage,
                g_rememberedSubtitleTitle);

            if (index >= 0) {
                int64_t id = subtitleTracks[static_cast<size_t>(index)].id;
                g_mpvApi.set_property(g_mpv, "sid", MPV_FORMAT_INT64, &id);
            }
        }
    }
}

void LayoutChildren(HWND hwnd);
void ShowPlaylistOsd();
void TogglePlaylistOsd();
void FlashPlaylistOsd(int durationMs = 2500);
void ShowPlaylistPopup(HWND anchor);
void SetMediaWindowTitle(const std::wstring& path);
std::wstring CurrentMediaTitleForOsd();
std::wstring AbsoluteLocalPath(const std::wstring& path);
std::wstring FindNextMediaFileInFolder(const std::wstring& currentPath);
void ExitFullscreen();

void UpdateTimeLabel() {
    if (!g_time) return;
    const std::wstring text = FormatTime(g_timePos) + L" / " + FormatTime(g_duration);
    SetWindowTextW(g_time, text.c_str());
}

void UpdateVolumeLabel() {
    if (!g_volumeText) return;
    wchar_t text[16]{};
    swprintf_s(text, L"%d%%", g_volumePercent);
    SetWindowTextW(g_volumeText, text);
}

void UpdateStatusText() {
    std::wstring status;
    std::wstring audioText = L"--";
    std::wstring subText = L"--";

    if (!g_mpvReady) {
        status = L"libmpv not loaded";
    } else {
        const bool idle = MpvGetFlag("idle-active", true);
        const bool cachePause = MpvGetFlag("paused-for-cache", false);
        const bool paused = MpvGetFlag("pause", true);

        if (idle) status = L"Stopped";
        else if (cachePause) status = L"Buffering";
        else if (paused) status = L"Paused";
        else status = L"Playing";

        mpv_node vp{};
        if (g_mpvApi.get_property(g_mpv, "video-params", MPV_FORMAT_NODE, &vp) >= 0) {
            if (vp.format == MPV_FORMAT_NODE_MAP) {
                const int64_t w = NodeInt(MapValue(vp, "w"), 0);
                const int64_t h = NodeInt(MapValue(vp, "h"), 0);
                if (w > 0 && h > 0) {
                    wchar_t res[64]{};
                    swprintf_s(res, L"  %lldx%lld",
                               static_cast<long long>(w), static_cast<long long>(h));
                    status += res;
                }
            }
            g_mpvApi.free_node_contents(&vp);
        }

        const auto allTracks = GetTracks(nullptr);
        bool haveSubTrack = false;

        for (const auto& t : allTracks) {
            if (t.type == "audio" && t.selected) {
                audioText = TrackShortLabel(t);
            } else if (t.type == "sub") {
                haveSubTrack = true;
                if (t.selected) {
                    subText = TrackShortLabel(t);
                }
            }
        }

        if (haveSubTrack && subText == L"--") {
            subText = L"OFF";
        }
    }

    const bool layoutChanged =
        status != g_statusText || audioText != g_audioInfoText || subText != g_subInfoText;

    g_statusText = status;
    g_audioInfoText = audioText;
    g_subInfoText = subText;

    if (g_status) SetWindowTextW(g_status, g_statusText.c_str());
    if (g_audioInfo) SetWindowTextW(g_audioInfo, g_audioInfoText.c_str());
    if (g_subInfo) SetWindowTextW(g_subInfo, g_subInfoText.c_str());

    // Track metadata often arrives just after the first layout. Re-layout as soon
    // as the real strings are known so they don't remain squeezed until a resize.
    if (layoutChanged && g_main) LayoutChildren(g_main);
}

void LoadStateIcons() {
    const int bigW = GetSystemMetrics(SM_CXICON);
    const int bigH = GetSystemMetrics(SM_CYICON);
    const int smallW = GetSystemMetrics(SM_CXSMICON);
    const int smallH = GetSystemMetrics(SM_CYSMICON);

    g_playIconBig = static_cast<HICON>(LoadImageW(
        g_instance, MAKEINTRESOURCEW(101), IMAGE_ICON, bigW, bigH, LR_SHARED));
    g_playIconSmall = static_cast<HICON>(LoadImageW(
        g_instance, MAKEINTRESOURCEW(101), IMAGE_ICON, smallW, smallH, LR_SHARED));
    g_pauseIconBig = static_cast<HICON>(LoadImageW(
        g_instance, MAKEINTRESOURCEW(102), IMAGE_ICON, bigW, bigH, LR_SHARED));
    g_pauseIconSmall = static_cast<HICON>(LoadImageW(
        g_instance, MAKEINTRESOURCEW(102), IMAGE_ICON, smallW, smallH, LR_SHARED));
}

void UpdateWindowStateIcon(bool paused) {
    if (!g_main) return;
    if (!g_playIconBig || !g_pauseIconBig) LoadStateIcons();

    HICON big = paused && g_pauseIconBig ? g_pauseIconBig : g_playIconBig;
    HICON smallIcon = paused && g_pauseIconSmall ? g_pauseIconSmall : g_playIconSmall;
    if (big) SendMessageW(g_main, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    if (smallIcon) SendMessageW(g_main, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
}

void PollMpv() {
    if (!g_mpvReady) return;

    bool fileLoaded = false;

    // libmpv requires clients to drain the event queue even when most events
    // are not otherwise consumed. FILE_LOADED is useful here because it is the
    // point at which the new file's track-list is ready for remembered choices.
    if (g_mpvApi.wait_event) {
        for (;;) {
            mpv_event* event = g_mpvApi.wait_event(g_mpv, 0.0);
            if (!event || event->event_id == MPV_EVENT_NONE) break;
            if (event->event_id == MPV_EVENT_FILE_LOADED) {
                fileLoaded = true;
            }
        }
    }

    if (fileLoaded) {
        g_fileLoadPending = false;

        // FILE_LOADED can be caused by mpv's own playlist auto-advance, which
        // bypasses LoadPlaylist(). Save the cached position for the file we are
        // leaving before replacing g_currentMediaPath with the new one.
        SaveCurrentResumePosition();

        const std::string loadedPathUtf8 = MpvGetString("path");
        if (!loadedPathUtf8.empty()) {
            g_currentMediaPath = AbsoluteLocalPath(Utf8ToWide(loadedPathUtf8));
            SetMediaWindowTitle(g_currentMediaPath);
        }

        // Sync offsets are intentionally per-file. If the user adjusted one on
        // the previous file, return it to neutral when a new file is loaded.
        if (g_audioDelayTouched) {
            MpvSetDouble("audio-delay", 0.0);
            g_audioDelayTouched = false;
        }
        if (g_subtitleDelayTouched) {
            MpvSetDouble("sub-delay", 0.0);
            g_subtitleDelayTouched = false;
        }

        g_lastEofReached = false;
        g_seenPlayingSinceLoad = false;
        g_suppressNextEofFullscreenExit = false;
        g_chapterTimes.clear();
        if (g_seek) InvalidateRect(g_seek, nullptr, FALSE);

        ApplyRememberedTrackSelections();
        RefreshChapterMarkers();
        RestoreCurrentResumePosition();
        g_pollCounter = 3;

        const int64_t playlistCount = MpvGetInt64("playlist-count", 0);
        const int64_t playlistPos = MpvGetInt64("playlist-pos", -1);
        const bool playlistTransition =
            playlistCount > 1 && playlistPos >= 0 &&
            g_haveLoadedPlaylistItem && playlistPos != g_lastLoadedPlaylistPos;

        if (playlistPos >= 0) {
            g_lastLoadedPlaylistPos = playlistPos;
            g_haveLoadedPlaylistItem = true;
        }

        if (g_playlistOsdVisible) {
            // Refresh the selected/gold entry after mpv advances internally.
            ShowPlaylistOsd();
        } else if (playlistTransition) {
            // Optional, automatic feedback when playback moves to another item.
            // The persistent P-toggle remains independent of this preference.
            switch (g_playlistStartOsd) {
            case PlaylistStartOsd::Title:
                ShowOsdText(CurrentMediaTitleForOsd(), 2000);
                break;
            case PlaylistStartOsd::Playlist:
                FlashPlaylistOsd();
                break;
            case PlaylistStartOsd::Nothing:
            default:
                break;
            }
        }
    }

    const double oldTime = g_timePos;
    const double oldDuration = g_duration;
    const int oldVolume = g_volumePercent;
    const bool oldMuted = g_muted;
    const bool oldPlaying = g_playing;

    g_timePos = std::max(0.0, MpvGetDouble("time-pos", 0.0));
    g_duration = std::max(0.0, MpvGetDouble("duration", 0.0));
    g_volumePercent = std::clamp(static_cast<int>(MpvGetDouble("volume", g_volumePercent) + 0.5), 0, 100);
    g_muted = MpvGetFlag("mute", false);

    const bool idle = MpvGetFlag("idle-active", true);
    const bool paused = MpvGetFlag("pause", true);
    const bool eofReached = MpvGetFlag("eof-reached", false);
    g_playing = !idle && !paused;

    if (g_playing) {
        g_seenPlayingSinceLoad = true;
    }

    if (g_suppressNextEofFullscreenExit &&
        g_duration > 0.0 &&
        g_timePos < g_duration - 0.25) {
        // A deliberate seek onto EOF may take a poll or two before mpv reports
        // eof-reached. Keep only the fullscreen-exit suppression armed until
        // the playhead moves materially back into the file.
        g_suppressNextEofFullscreenExit = false;
    }

    const bool eofJustReached =
        eofReached &&
        !g_lastEofReached &&
        g_seenPlayingSinceLoad;

    if (eofJustReached) {
        const int64_t playlistCount = MpvGetInt64("playlist-count", 0);
        const int64_t playlistPos = MpvGetInt64("playlist-pos", -1);
        const bool playlistHasNext =
            playlistPos >= 0 &&
            playlistCount > 0 &&
            playlistPos < playlistCount - 1;

        // AUTO is deliberately suppressed while an explicit multi-item mpv
        // playlist exists, so mirror that same priority rule here. Resolve the
        // next folder item once and reuse that exact path for AUTO advance.
        const bool autoCanAdvance =
            g_autoPlayNextFile &&
            !g_fileLoadPending &&
            playlistCount <= 1 &&
            !g_currentMediaPath.empty();

        std::wstring nextAutoPath;
        if (autoCanAdvance) {
            nextAutoPath = FindNextMediaFileInFolder(g_currentMediaPath);
        }

        const bool nextThingExists =
            playlistHasNext || !nextAutoPath.empty();
        // AUTO should advance whether EOF was reached naturally or by a
        // deliberate seek. Only the optional fullscreen-exit behavior needs
        // to distinguish those two cases.
        if (g_exitFullscreenOnPlaybackEnd &&
            g_fullscreen &&
            !g_suppressNextEofFullscreenExit &&
            !nextThingExists) {
            ExitFullscreen();
        }

        if (!nextAutoPath.empty()) {
            g_pendingAutoNextPath = nextAutoPath;
            if (!PostMessageW(g_main, WM_APP_AUTO_NEXT_FILE, 0, 0)) {
                g_pendingAutoNextPath.clear();
            }
        }
    }
    g_lastEofReached = eofReached;

    if (oldTime != g_timePos || oldDuration != g_duration) {
        UpdateTimeLabel();
        if (g_seek) InvalidateRect(g_seek, nullptr, FALSE);
    }
    if (oldVolume != g_volumePercent) {
        if (g_volume) InvalidateRect(g_volume, nullptr, FALSE);
        UpdateVolumeLabel();
    }
    if (oldMuted != g_muted && g_mute) InvalidateRect(g_mute, nullptr, FALSE);
    if (oldPlaying != g_playing && g_play) InvalidateRect(g_play, nullptr, FALSE);

    const bool pausedForIcon = !idle && paused;
    UpdateWindowStateIcon(pausedForIcon);

    if (g_overlay) SetWindowPos(g_overlay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (++g_pollCounter >= 4) {
        g_pollCounter = 0;
        UpdateStatusText();
        RefreshChapterMarkers();
    }
}

bool InitMpv() {
    const std::wstring exeDir = GetExeDirectory();
    const std::wstring libMpvDir = exeDir + L"\\libmpv";
    const std::wstring dllPath = libMpvDir + L"\\libmpv-2.dll";

    // Detect a missing engine before configuring the DLL search path so the
    // user gets a useful error instead of a generic loader failure. A future
    // GitHub release can attach an automatic download action to this path.
    if (!FileExists(dllPath)) {
        g_statusText = L"libmpv runtime missing";
        if (g_status) SetWindowTextW(g_status, g_statusText.c_str());
        MessageBoxW(
            g_main,
            L"The playback engine is missing.\n\n"
            L"Expected: libmpv\\libmpv-2.dll\n\n"
            L"Reinstall the Full Portable package, or place a compatible "
            L"libmpv runtime in the libmpv folder.\n\n"
            L"Project: https://github.com/WinterStatic/MPV-WinterStatic-Edition",
            kAppTitle, MB_OK | MB_ICONWARNING);
        return false;
    }

    // Keep the playback engine isolated from the frontend. Register libmpv\
    // as a dedicated DLL directory for the process lifetime so both normal
    // imports and libraries that libmpv loads dynamically (Vulkan/EGL/etc.)
    // can resolve from the engine folder without placing runtime DLLs beside
    // the frontend EXE.
    if (!SetDefaultDllDirectories(
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS)) {
        g_statusText = L"DLL search setup failed";
        if (g_status) SetWindowTextW(g_status, g_statusText.c_str());
        MessageBoxW(g_main,
                    L"Windows could not configure the isolated libmpv DLL search path.",
                    kAppTitle, MB_OK | MB_ICONERROR);
        return false;
    }

    g_libMpvDllDirectoryCookie = AddDllDirectory(libMpvDir.c_str());
    if (!g_libMpvDllDirectoryCookie) {
        g_statusText = L"libmpv folder unavailable";
        if (g_status) SetWindowTextW(g_status, g_statusText.c_str());
        MessageBoxW(g_main,
                    L"Windows could not register the libmpv runtime folder.\n\n"
                    L"Expected folder: libmpv\\",
                    kAppTitle, MB_OK | MB_ICONERROR);
        return false;
    }

    g_mpvApi.dll = LoadLibraryExW(
        dllPath.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
            LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (!g_mpvApi.dll) {
        g_statusText = L"libmpv runtime load failed";
        if (g_status) SetWindowTextW(g_status, g_statusText.c_str());
        MessageBoxW(g_main,
                    L"The libmpv runtime was found but could not be loaded.\n\n"
                    L"Use the runtime supplied with MPV WinterStatic Edition, or replace the entire libmpv folder with a compatible runtime.",
                    kAppTitle, MB_OK | MB_ICONWARNING);
        RemoveDllDirectory(g_libMpvDllDirectoryCookie);
        g_libMpvDllDirectoryCookie = nullptr;
        return false;
    }

    bool ok = true;
    ok &= LoadProc(g_mpvApi.create, "mpv_create");
    ok &= LoadProc(g_mpvApi.initialize, "mpv_initialize");
    ok &= LoadProc(g_mpvApi.terminate_destroy, "mpv_terminate_destroy");
    ok &= LoadProc(g_mpvApi.command, "mpv_command");
    ok &= LoadProc(g_mpvApi.command_node, "mpv_command_node");
    ok &= LoadProc(g_mpvApi.set_option, "mpv_set_option");
    ok &= LoadProc(g_mpvApi.set_option_string, "mpv_set_option_string");
    ok &= LoadProc(g_mpvApi.load_config_file, "mpv_load_config_file");
    ok &= LoadProc(g_mpvApi.get_property, "mpv_get_property");
    ok &= LoadProc(g_mpvApi.set_property, "mpv_set_property");
    ok &= LoadProc(g_mpvApi.set_property_string, "mpv_set_property_string");
    ok &= LoadProc(g_mpvApi.wait_event, "mpv_wait_event");
    ok &= LoadProc(g_mpvApi.free_fn, "mpv_free");
    ok &= LoadProc(g_mpvApi.free_node_contents, "mpv_free_node_contents");
    LoadProc(g_mpvApi.error_string, "mpv_error_string");
    if (!ok) {
        MessageBoxW(g_main, L"The installed libmpv-2.dll is missing one or more required client API exports.", kAppTitle, MB_OK | MB_ICONERROR);
        return false;
    }

    g_mpv = g_mpvApi.create();
    if (!g_mpv) {
        MessageBoxW(g_main, L"mpv_create() failed.", kAppTitle, MB_OK | MB_ICONERROR);
        return false;
    }

    // Ignore standalone mpv config locations. This frontend loads only the
    // frontend-owned mpv.conf beside MPV-WinterStatic-Edition.exe.
    g_mpvApi.set_option_string(g_mpv, "config", "no");

    // Baseline playback defaults. Local mpv.conf is loaded afterwards so it
    // can override playback choices such as hwdec, vo, gpu-api, scaling, etc.
    g_mpvApi.set_option_string(g_mpv, "keep-open", "yes");
    g_mpvApi.set_option_string(g_mpv, "hwdec", "auto-safe");

    const std::wstring mpvConfigPath = GetMpvConfigPath();
    if (FileExists(mpvConfigPath)) {
        const std::string configUtf8 = WideToUtf8(mpvConfigPath);
        const int configResult =
            g_mpvApi.load_config_file(g_mpv, configUtf8.c_str());

        if (configResult < 0) {
            const std::wstring msg =
                L"mpv.conf could not be loaded:\n\n" + MpvError(configResult);
            MessageBoxW(g_main, msg.c_str(), kAppTitle, MB_OK | MB_ICONWARNING);
        }
    }

    // The Options-dialog GPU API selector is authoritative and is applied
    // after mpv.conf. Changing it requires restarting the player.
    g_mpvApi.set_option_string(g_mpv, "gpu-api", GpuApiOptionValue(g_gpuApi));

    // Frontend-owned options are applied after mpv.conf so custom playback
    // tuning cannot accidentally re-enable mpv's OSC or keyboard input.
    // Reassert config=no so no separate standalone-mpv config is imported.
    g_mpvApi.set_option_string(g_mpv, "config", "no");
    g_mpvApi.set_option_string(g_mpv, "terminal", "no");
    g_mpvApi.set_option_string(g_mpv, "osc", "no");
    g_mpvApi.set_option_string(g_mpv, "load-stats-overlay", "yes");
    g_mpvApi.set_option_string(g_mpv, "input-default-bindings", "no");
    g_mpvApi.set_option_string(g_mpv, "input-vo-keyboard", "no");
    g_mpvApi.set_option_string(g_mpv, "volume-max", "100");
    if (!g_preferredAudioLanguage.empty()) {
        const std::string alang = WideToUtf8(g_preferredAudioLanguage);
        g_mpvApi.set_option_string(g_mpv, "alang", alang.c_str());
    }
    if (!g_preferredSubtitleLanguage.empty()) {
        const std::string slang = WideToUtf8(g_preferredSubtitleLanguage);
        g_mpvApi.set_option_string(g_mpv, "slang", slang.c_str());
    }
    const std::string subtitleSize = std::to_string(g_subtitleFontSize);
    g_mpvApi.set_option_string(g_mpv, "sub-font-size", subtitleSize.c_str());

    g_mpvApi.set_option_string(g_mpv, "osd-align-x", "center");
    g_mpvApi.set_option_string(g_mpv, "osd-align-y", "top");
    g_mpvApi.set_option_string(g_mpv, "osd-margin-y", "180");
    g_mpvApi.set_option_string(g_mpv, "osd-font", "Segoe UI Semibold");
    const std::string osdSize = std::to_string(g_osdFontSize);
    g_mpvApi.set_option_string(g_mpv, "osd-font-size", osdSize.c_str());
    g_mpvApi.set_option_string(g_mpv, "osd-bold", "yes");
    g_mpvApi.set_option_string(g_mpv, "osd-border-size", "1.2");
    g_mpvApi.set_option_string(g_mpv, "osd-shadow-offset", "1.5");

    // On Windows, --wid expects the HWND value as an unsigned 32-bit value.
    const uint32_t rawWid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_video));
    int64_t wid = static_cast<int64_t>(rawWid);
    g_mpvApi.set_option(g_mpv, "wid", MPV_FORMAT_INT64, &wid);

    const int init = g_mpvApi.initialize(g_mpv);
    if (init < 0) {
        const std::wstring msg = L"libmpv failed to initialize:\n\n" + MpvError(init);
        MessageBoxW(g_main, msg.c_str(), kAppTitle, MB_OK | MB_ICONERROR);
        g_mpvApi.terminate_destroy(g_mpv);
        g_mpv = nullptr;
        return false;
    }

    g_mpvReady = true;
    MpvSetDouble("volume", static_cast<double>(g_volumePercent));
    MpvSetFlag("mute", g_muted);
    UpdateStatusText();
    return true;
}

void ShutdownMpv() {
    g_mpvReady = false;
    if (g_mpv && g_mpvApi.terminate_destroy) {
        g_mpvApi.terminate_destroy(g_mpv);
        g_mpv = nullptr;
    }
    if (g_mpvApi.dll) {
        FreeLibrary(g_mpvApi.dll);
        g_mpvApi.dll = nullptr;
    }
    if (g_libMpvDllDirectoryCookie) {
        RemoveDllDirectory(g_libMpvDllDirectoryCookie);
        g_libMpvDllDirectoryCookie = nullptr;
    }
}

std::wstring DisplayNameFromPath(const std::wstring& path) {
    if (path.empty()) return kAppTitle;
    const size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    return name.empty() ? kAppTitle : name;
}

void SetMediaWindowTitle(const std::wstring& path) {
    if (!g_main) return;
    const std::wstring fileName = DisplayNameFromPath(path);
    const std::wstring title = fileName.empty()
        ? std::wstring(kAppTitle)
        : fileName + L" - " + kAppTitle;
    SetWindowTextW(g_main, title.c_str());
}

std::wstring CurrentMediaTitleForOsd() {
    const std::string mpvTitle = MpvGetString("media-title");
    if (!mpvTitle.empty()) return Utf8ToWide(mpvTitle);
    return DisplayNameFromPath(g_currentMediaPath);
}

bool IsSupportedMediaFile(const std::filesystem::path& path) {
    const std::wstring ext = path.extension().wstring();
    static constexpr const wchar_t* kExtensions[] = {
        L".mkv", L".mp4", L".avi", L".mov", L".wmv", L".webm",
        L".m4v", L".mpg", L".mpeg", L".ts", L".m2ts",
        L".mp3", L".flac", L".wav", L".m4a", L".aac", L".ogg", L".opus"
    };

    for (const wchar_t* supported : kExtensions) {
        if (_wcsicmp(ext.c_str(), supported) == 0) return true;
    }
    return false;
}

std::wstring AbsoluteLocalPath(const std::wstring& path) {
    std::error_code ec;
    const std::filesystem::path absolute =
        std::filesystem::absolute(std::filesystem::path(path), ec);
    return ec ? path : absolute.lexically_normal().wstring();
}

std::wstring FindNextMediaFileInFolder(const std::wstring& currentPath) {
    if (currentPath.empty()) return L"";

    const std::filesystem::path current(currentPath);
    std::filesystem::path directory = current.parent_path();
    if (directory.empty()) directory = L".";

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(directory, ec), end;
         !ec && it != end;
         it.increment(ec)) {
        std::error_code typeError;
        if (!it->is_regular_file(typeError) || typeError) continue;
        if (IsSupportedMediaFile(it->path())) {
            files.push_back(it->path());
        }
    }
    if (ec || files.empty()) return L"";

    std::sort(files.begin(), files.end(),
              [](const std::filesystem::path& a,
                 const std::filesystem::path& b) {
        const std::wstring an = a.filename().wstring();
        const std::wstring bn = b.filename().wstring();
        const int logical = StrCmpLogicalW(an.c_str(), bn.c_str());
        if (logical != 0) return logical < 0;
        return _wcsicmp(a.wstring().c_str(), b.wstring().c_str()) < 0;
    });

    const std::wstring currentName = current.filename().wstring();
    for (size_t i = 0; i < files.size(); ++i) {
        if (_wcsicmp(files[i].filename().wstring().c_str(),
                     currentName.c_str()) == 0) {
            if (i + 1 < files.size()) {
                return files[i + 1].wstring();
            }
            break;
        }
    }

    return L"";
}

void LoadPlaylist(const std::vector<std::wstring>& paths) {
    if (!g_mpvReady || paths.empty()) return;

    // Any explicit/replacement load supersedes a queued AUTO transition.
    g_pendingAutoNextPath.clear();

    // Preserve the current file before a replace load. Natural mpv playlist
    // advances are covered separately by FILE_LOADED in PollMpv().
    SaveCurrentResumePosition();

    if (!g_preferredAudioLanguage.empty()) {
        MpvSetString("alang", g_preferredAudioLanguage);
    }
    if (!g_preferredSubtitleLanguage.empty()) {
        MpvSetString("slang", g_preferredSubtitleLanguage);
    }

    g_fileLoadPending = true;
    // A newly opened/replaced playlist starts a fresh transition history.
    // The first file is not treated as "the next video" for automatic OSD.
    g_haveLoadedPlaylistItem = false;
    g_lastLoadedPlaylistPos = -1;

    bool first = true;
    for (const auto& rawPath : paths) {
        if (rawPath.empty()) continue;
        const std::wstring path = AbsoluteLocalPath(rawPath);
        const std::string utf8 = WideToUtf8(path);
        if (utf8.empty()) continue;

        if (first) {
            if (!MpvCommand({"loadfile", utf8, "replace"})) {
                g_fileLoadPending = false;
                return;
            }
            first = false;
        } else {
            MpvCommand({"loadfile", utf8, "append"});
        }
    }

    if (first) {
        g_fileLoadPending = false;
        return;
    }

    g_pollCounter = 3;
}

void LoadFile(const std::wstring& path) {
    if (path.empty()) return;
    LoadPlaylist({ path });
}

void PlayPendingAutoNextFile() {
    // The EOF transition already resolved the next file. Consume that cached
    // path here instead of enumerating/sorting the same folder a second time.
    const std::wstring next = g_pendingAutoNextPath;
    g_pendingAutoNextPath.clear();

    if (!g_autoPlayNextFile || next.empty()) return;
    if (MpvGetInt64("playlist-count", 0) > 1) return;

    LoadFile(next);

    // keep-open=yes leaves mpv paused on EOF. That pause state survives a
    // replace load unless we deliberately clear it for AUTO advance.
    MpvSetFlag("pause", false);
}

void ToggleAutoPlayNextFile() {
    g_autoPlayNextFile = !g_autoPlayNextFile;
    WriteSettingInt(L"General", L"AutoPlayNextFile",
                    g_autoPlayNextFile ? 1 : 0);
    if (g_autoNext) InvalidateRect(g_autoNext, nullptr, FALSE);
}

void OpenFileDialog() {
    // Explorer-style multi-select returns either one full path, or a directory
    // followed by one or more file names, terminated by a second NUL.
    std::vector<wchar_t> buffer(65536, L'\0');
    OPENFILENAMEW ofn{ sizeof(ofn) };
    ofn.hwndOwner = g_main;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.lpstrFilter =
        L"Media files\0*.mkv;*.mp4;*.avi;*.mov;*.wmv;*.webm;*.m4v;*.mpg;*.mpeg;*.ts;*.m2ts;*.mp3;*.flac;*.wav;*.m4a;*.aac;*.ogg;*.opus\0"
        L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<std::wstring> paths;
    const wchar_t* first = buffer.data();
    const wchar_t* next = first + wcslen(first) + 1;

    if (*next == L'\0') {
        paths.emplace_back(first);
    } else {
        const std::filesystem::path directory(first);
        for (const wchar_t* name = next; *name; name += wcslen(name) + 1) {
            paths.push_back((directory / name).wstring());
        }
    }

    LoadPlaylist(paths);
}

void TogglePlay() {
    if (!g_mpvReady) return;
    const bool paused = MpvGetFlag("pause", false);
    const bool idle = MpvGetFlag("idle-active", true);
    if (idle) return;
    MpvSetFlag("pause", !paused);
    PollMpv();
}

void StopPlayback() {
    MpvCommand({"stop"});
    PollMpv();
}

void SeekAbsolute(double seconds);

void SeekBy(int seconds) {
    if (!g_mpvReady || g_duration <= 0.0) return;
    SeekAbsolute(g_timePos + static_cast<double>(seconds));
}

void SeekAbsolute(double seconds) {
    if (!g_mpvReady || g_duration <= 0.0) return;
    seconds = std::clamp(seconds, 0.0, g_duration);

    // A deliberate seek onto the very end may still advance AUTO, but should
    // not be treated as a natural finish for automatic fullscreen exit.
    if (seconds >= g_duration - 0.05) {
        g_suppressNextEofFullscreenExit = true;
    }

    MpvSetDouble("time-pos", seconds);
    g_timePos = seconds;
    UpdateTimeLabel();
    if (g_seek) InvalidateRect(g_seek, nullptr, FALSE);
    ShowOsdText(FormatTime(g_timePos) + L" / " + FormatTime(g_duration));
}

void ChapterStep(bool next) {
    if (!g_mpvReady) return;
    MpvCommand({"add", "chapter", next ? "1" : "-1"});
    g_pollCounter = 3;
    PollMpv();
}

void AdjustSpeed(double delta) {
    if (!g_mpvReady) return;

    double speed = MpvGetDouble("speed", 1.0);
    speed = std::clamp(speed + delta, 0.25, 4.0);

    // Snap to hundredths so repeated +/- presses don't accumulate ugly
    // floating-point tails.
    speed = std::round(speed * 100.0) / 100.0;
    MpvSetDouble("speed", speed);

    wchar_t osd[32]{};
    swprintf_s(osd, L"%.2f\u00D7", speed);
    ShowOsdText(osd, 850);
}

std::wstring DelayOsdText(const wchar_t* label, int milliseconds) {
    wchar_t value[64]{};
    if (milliseconds > 0) {
        swprintf_s(value, L"%ls: +%d ms", label, milliseconds);
    } else {
        swprintf_s(value, L"%ls: %d ms", label, milliseconds);
    }
    return value;
}

void SetSyncDelayMs(const char* property, const wchar_t* label,
                    int milliseconds, bool& touched) {
    if (!g_mpvReady || MpvGetFlag("idle-active", true)) {
        ShowOsdText(L"No media loaded", 1000);
        return;
    }

    if (MpvSetDouble(property, static_cast<double>(milliseconds) / 1000.0)) {
        touched = milliseconds != 0;
        ShowOsdText(DelayOsdText(label, milliseconds), 1000);
    } else {
        ShowOsdText(std::wstring(label) + L" adjustment unavailable", 1400);
    }
}

void AdjustSyncDelayMs(const char* property, const wchar_t* label,
                       int deltaMs, bool& touched) {
    if (!g_mpvReady || MpvGetFlag("idle-active", true)) {
        ShowOsdText(L"No media loaded", 1000);
        return;
    }

    const int currentMs = static_cast<int>(
        std::llround(MpvGetDouble(property, 0.0) * 1000.0));
    SetSyncDelayMs(property, label, currentMs + deltaMs, touched);
}

void SetVolume(int percent) {
    g_volumePercent = std::clamp(percent, 0, 100);
    if (g_mpvReady) MpvSetDouble("volume", static_cast<double>(g_volumePercent));
    if (g_volume) InvalidateRect(g_volume, nullptr, FALSE);
    UpdateVolumeLabel();
    SaveAudioState();
    wchar_t osd[32]{};
    swprintf_s(osd, L"%d%%", g_volumePercent);
    ShowOsdText(osd);
}

void AdjustVolume(int delta) {
    SetVolume(g_volumePercent + delta);
}

int PlaylistOsdFontSize() {
    // Keep the couch-readable size proven in 0.2.40/0.2.41. The bounded
    // overlay fixes the long-list problem without shrinking the glyphs again.
    return std::clamp(static_cast<int>(std::lround(g_osdFontSize * 0.56)), 30, 44);
}

std::wstring PlaylistOverlayDisplayLabel(const PlaylistEntry& entry) {
    std::wstring label;
    if (!entry.title.empty()) {
        label = Utf8ToWide(entry.title);
    } else {
        const std::wstring full = Utf8ToWide(entry.filename);
        if (!full.empty()) {
            const std::filesystem::path path(full);
            label = path.filename().wstring();
            if (label.empty()) label = full;
        }
    }
    if (label.empty()) label = L"Untitled";

    for (wchar_t& ch : label) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    }
    while (label.find(L"  ") != std::wstring::npos) {
        label.replace(label.find(L"  "), 2, L" ");
    }
    return label;
}

size_t PlaylistOverlayMaxLabelChars() {
    // Keep rows single-line. Scale the character budget with the actual video
    // surface width; 96 characters is a conservative 1080p-width target for
    // the proven ~40 OSD size and Segoe UI Semibold.
    RECT r{};
    if (!g_video || !GetClientRect(g_video, &r)) return 72;
    const int width = std::max(1, static_cast<int>(r.right - r.left));
    const double scale = static_cast<double>(width) / 1920.0;
    return static_cast<size_t>(std::clamp(static_cast<int>(std::lround(96.0 * scale)), 48, 104));
}

std::string AssColorFromMpvHex(const std::string& value, const char* fallbackRgb) {
    std::string rgb = value;
    if (rgb.size() == 9 && rgb[0] == '#') rgb = "#" + rgb.substr(3); // #AARRGGBB
    if (rgb.size() != 7 || rgb[0] != '#') rgb = fallbackRgb;
    if (rgb.size() != 7 || rgb[0] != '#') rgb = "#FFFFFF";

    const std::string rr = rgb.substr(1, 2);
    const std::string gg = rgb.substr(3, 2);
    const std::string bb = rgb.substr(5, 2);
    return "&H" + bb + gg + rr + "&";
}

bool RenderPlaylistOverlay(std::wstring* errorText = nullptr) {
    if (!g_mpvReady) return false;

    const auto entries = GetPlaylistEntries();
    if (entries.empty()) return false;

    int64_t pos = MpvGetInt64("playlist-pos", -1);
    if (pos < 0 || pos >= static_cast<int64_t>(entries.size())) {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].playing || entries[i].current) {
                pos = static_cast<int64_t>(i);
                break;
            }
        }
    }
    if (pos < 0 || pos >= static_cast<int64_t>(entries.size())) pos = 0;

    constexpr size_t kVisibleEntries = 9;
    const size_t count = entries.size();
    const size_t current = static_cast<size_t>(pos);
    const size_t half = kVisibleEntries / 2;
    size_t start = current > half ? current - half : 0;
    if (start + kVisibleEntries > count) {
        start = count > kVisibleEntries ? count - kVisibleEntries : 0;
    }
    const size_t end = std::min(count, start + kVisibleEntries);
    const size_t maxLabelChars = PlaylistOverlayMaxLabelChars();

    const int fontSize = PlaylistOsdFontSize();
    const int lineHeight = std::max(fontSize + 6, static_cast<int>(std::lround(fontSize * 1.18)));
    const int left = 10;
    int y = 10;

    const std::string normalColor = AssColorFromMpvHex(MpvGetString("osd-color"), "#FFFFFF");
    const std::string selectedColor = AssColorFromMpvHex(MpvGetString("osd-selected-color"), "#FFCC00");

    std::string data;
    auto appendLine = [&](const std::wstring& raw, bool selected) {
        std::wstring line = raw;
        if (line.size() > maxLabelChars) {
            const size_t keep = maxLabelChars > 3 ? maxLabelChars - 3 : maxLabelChars;
            line.resize(keep);
            line += L"...";
        }

        // ass-events splits DATA on real newline characters and turns every
        // line into its own Dialogue event. Give every row its own absolute
        // position rather than relying on embedded \\N line breaks.
        const std::string color = selected ? selectedColor : normalColor;
        data += "{\\an7\\pos(" + std::to_string(left) + "," + std::to_string(y) + ")"
                "\\q2\\fs" + std::to_string(fontSize) + "\\fsp-0.75\\1c" + color + "}";
        data += MpvEscapeAss(WideToUtf8(line));
        data += "\n";
        y += lineHeight;
    };

    wchar_t header[64]{};
    swprintf_s(header, L"Playlist [%lld/%llu]:",
               static_cast<long long>(current + 1),
               static_cast<unsigned long long>(count));
    appendLine(header, false);
    if (start > 0) appendLine(L"...", false);

    for (size_t i = start; i < end; ++i) {
        std::wstring label = PlaylistOverlayDisplayLabel(entries[i]);
        wchar_t prefix[32]{};
        if (i == current) {
            swprintf_s(prefix, L"\u25B6 %llu. ", static_cast<unsigned long long>(i + 1));
        } else {
            swprintf_s(prefix, L"   %llu. ", static_cast<unsigned long long>(i + 1));
        }
        appendLine(std::wstring(prefix) + label, i == current);
    }
    if (end < count) appendLine(L"...", false);

    if (!data.empty() && data.back() == '\n') data.pop_back();

    const int rc = MpvSetTextOverlay("ass-events", data);
    if (rc < 0) {
        if (errorText) *errorText = MpvErrorText(rc);
        return false;
    }
    return true;
}

void ShowPlaylistOsd() {
    if (!g_mpvReady || MpvGetInt64("playlist-count", 0) <= 0) {
        g_playlistOsdVisible = false;
        RemovePlaylistOverlay();
        ShowOsdText(L"No playlist", 1200);
        return;
    }

    if (g_main) KillTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE);
    std::wstring overlayError;
    if (RenderPlaylistOverlay(&overlayError)) {
        g_playlistOsdVisible = true;
    } else {
        g_playlistOsdVisible = false;
        const std::wstring message = overlayError.empty()
            ? L"Playlist overlay could not be rendered"
            : L"Playlist overlay failed: " + overlayError;
        ShowOsdText(message, 3000);
    }
}

void HidePlaylistOsd() {
    if (g_main) KillTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE);
    RemovePlaylistOverlay();
    g_playlistOsdVisible = false;
}

void TogglePlaylistOsd() {
    if (g_playlistOsdVisible) HidePlaylistOsd();
    else ShowPlaylistOsd();
}

void FlashPlaylistOsd(int durationMs) {
    if (!g_mpvReady || MpvGetInt64("playlist-count", 0) <= 0) {
        ShowOsdText(L"No playlist", 1200);
        return;
    }

    if (!RenderPlaylistOverlay()) return;

    if (!g_playlistOsdVisible && g_main) {
        KillTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE);
        SetTimer(g_main, TIMER_PLAYLIST_OSD_RESTORE,
                 static_cast<UINT>(std::max(50, durationMs)), nullptr);
    }
}

void PlaylistStep(bool next) {
    if (!g_mpvReady) return;

    const int64_t count = MpvGetInt64("playlist-count", 0);
    const int64_t pos = MpvGetInt64("playlist-pos", -1);
    if (count <= 0 || pos < 0) {
        if (!g_playlistOsdVisible) FlashPlaylistOsd();
        return;
    }

    const int64_t target = next ? pos + 1 : pos - 1;
    if (target < 0 || target >= count) {
        if (!g_playlistOsdVisible) FlashPlaylistOsd();
        return;
    }

    MpvCommand({next ? "playlist-next" : "playlist-prev", "weak"});
}

void PlayPlaylistIndex(size_t index) {
    if (!g_mpvReady) return;

    const int64_t count = MpvGetInt64("playlist-count", 0);
    if (index >= static_cast<size_t>(std::max<int64_t>(0, count))) return;

    const int64_t current = MpvGetInt64("playlist-pos", -1);
    if (current == static_cast<int64_t>(index)) {
        if (!g_playlistOsdVisible) FlashPlaylistOsd();
        return;
    }

    MpvCommand({"playlist-play-index", std::to_string(index)});
}

bool HandlePlaylistMenuChoice(int chosen) {
    if (chosen == IDM_PLAYLIST_SHOW) {
        TogglePlaylistOsd();
        return true;
    }
    if (chosen == IDM_PLAYLIST_PREV) {
        PlaylistStep(false);
        return true;
    }
    if (chosen == IDM_PLAYLIST_NEXT) {
        PlaylistStep(true);
        return true;
    }
    if (chosen >= IDM_CTX_PLAYLIST_BASE &&
        chosen < IDM_CTX_PLAYLIST_BASE + kMaxPlaylistMenuItems) {
        PlayPlaylistIndex(static_cast<size_t>(chosen - IDM_CTX_PLAYLIST_BASE));
        return true;
    }
    return false;
}

void ToggleMute() {
    g_muted = !MpvGetFlag("mute", false);
    if (g_mpvReady) MpvSetFlag("mute", g_muted);
    if (g_mute) InvalidateRect(g_mute, nullptr, FALSE);
    SaveAudioState();
}

void AdjustVolumeFromWheel(WPARAM wParam) {
    g_wheelRemainder += GET_WHEEL_DELTA_WPARAM(wParam);
    while (g_wheelRemainder >= WHEEL_DELTA) {
        AdjustVolume(kVolumeStep);
        g_wheelRemainder -= WHEEL_DELTA;
    }
    while (g_wheelRemainder <= -WHEEL_DELTA) {
        AdjustVolume(-kVolumeStep);
        g_wheelRemainder += WHEEL_DELTA;
    }
}

void ShowTrackMenu(HWND anchor, bool audio) {
    if (!g_mpvReady || !anchor) return;
    const auto tracks = GetTracks(audio ? "audio" : "sub");
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    int command = 1;
    if (!audio) {
        bool anySelected = false;
        for (const auto& t : tracks) anySelected = anySelected || t.selected;
        AppendMenuW(menu, MF_STRING | (anySelected ? 0 : MF_CHECKED), command++, L"Off");
        if (!tracks.empty()) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    for (const auto& t : tracks) {
        const std::wstring label = TrackMenuLabel(t);
        AppendMenuW(menu, MF_STRING | (t.selected ? MF_CHECKED : 0), command++, label.c_str());
    }

    if (tracks.empty()) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 999, audio ? L"No audio tracks" : L"No subtitle tracks");
    }

    RECT r{};
    GetWindowRect(anchor, &r);
    const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN,
                                      r.left, r.top, 0, g_main, nullptr);
    if (chosen > 0 && chosen != 999) {
        if (!audio && chosen == 1) {
            if (g_mpvApi.set_property_string(g_mpv, "sid", "no") >= 0) {
                RememberSubtitlesOff();
            }
        } else {
            const int index = chosen - 1 - (audio ? 0 : 1);
            if (index >= 0 && index < static_cast<int>(tracks.size())) {
                const TrackInfo& track = tracks[static_cast<size_t>(index)];
                int64_t id = track.id;
                if (g_mpvApi.set_property(
                        g_mpv,
                        audio ? "aid" : "sid",
                        MPV_FORMAT_INT64,
                        &id) >= 0) {
                    if (audio) RememberAudioTrack(track);
                    else RememberSubtitleTrack(track);
                }
            }
        }
        g_pollCounter = 3;
        PollMpv();
    }
    DestroyMenu(menu);
}

void CycleAudioTrack() {
    if (!g_mpvReady) return;

    const auto tracks = GetTracks("audio");
    if (tracks.empty()) {
        ShowOsdText(L"No audio tracks", 1000);
        return;
    }

    int selected = -1;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].selected) {
            selected = static_cast<int>(i);
            break;
        }
    }

    const size_t next =
        selected < 0
            ? 0
            : (static_cast<size_t>(selected) + 1) % tracks.size();

    int64_t id = tracks[next].id;
    if (g_mpvApi.set_property(g_mpv, "aid", MPV_FORMAT_INT64, &id) >= 0) {
        RememberAudioTrack(tracks[next]);
        g_pollCounter = 3;
        PollMpv();
        ShowOsdText(L"Audio: " + TrackMenuLabel(tracks[next]), 1000);
    }
}

void CycleSubtitleTrack() {
    if (!g_mpvReady) return;

    const auto tracks = GetTracks("sub");
    if (tracks.empty()) {
        ShowOsdText(L"No subtitle tracks", 1000);
        return;
    }

    int selected = -1;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].selected) {
            selected = static_cast<int>(i);
            break;
        }
    }

    // Off -> first subtitle track.
    if (selected < 0) {
        int64_t id = tracks.front().id;
        if (g_mpvApi.set_property(g_mpv, "sid", MPV_FORMAT_INT64, &id) >= 0) {
            RememberSubtitleTrack(tracks.front());
            g_pollCounter = 3;
            PollMpv();
            ShowOsdText(L"Subtitles: " + TrackMenuLabel(tracks.front()), 1000);
        }
        return;
    }

    // Last subtitle track -> Off.
    const size_t next = static_cast<size_t>(selected) + 1;
    if (next >= tracks.size()) {
        if (g_mpvApi.set_property_string(g_mpv, "sid", "no") >= 0) {
            RememberSubtitlesOff();
            g_pollCounter = 3;
            PollMpv();
            ShowOsdText(L"Subtitles: Off", 1000);
        }
        return;
    }

    int64_t id = tracks[next].id;
    if (g_mpvApi.set_property(g_mpv, "sid", MPV_FORMAT_INT64, &id) >= 0) {
        RememberSubtitleTrack(tracks[next]);
        g_pollCounter = 3;
        PollMpv();
        ShowOsdText(L"Subtitles: " + TrackMenuLabel(tracks[next]), 1000);
    }
}

void ToggleMediaInfo() {
    if (!g_mpvReady) {
        MessageBoxW(
            g_main,
            L"Media info is unavailable because libmpv is not initialized.",
            kAppTitle,
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MpvGetFlag("idle-active", true)) {
        ShowOsdText(L"No media loaded", 1200);
        return;
    }

    if (!IsStatsOverlayAvailable()) {
        g_mediaInfoVisible = false;
        UpdateOsdPosition();
        ShowOsdText(L"Media info is unavailable in this libmpv build", 1800);
        return;
    }

    if (!g_mediaInfoVisible) {
        // The builtin stats overlay uses top-left ASS alignment and, with its
        // default non-persistent mode, mpv's normal OSD path. Apply the safe
        // Media Info margins before toggling it on.
        g_mediaInfoVisible = true;
        UpdateOsdPosition();

        if (!MpvCommand({"script-binding", "stats/display-stats-toggle"})) {
            g_mediaInfoVisible = false;
            UpdateOsdPosition();
            ShowOsdText(L"Media info is unavailable in this libmpv build", 1800);
        }
    } else {
        MpvCommand({"script-binding", "stats/display-stats-toggle"});
        g_mediaInfoVisible = false;

        // Restore the user's normal transient OSD location immediately.
        UpdateOsdPosition();
    }
}

void SetControlsVisible(bool visible) {
    if (!g_fullscreen) visible = true;
    if (g_controlsVisible == visible) return;
    g_controlsVisible = visible;
    ShowWindow(g_panel, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        SetWindowPos(g_panel, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        g_lastControlsHover = GetTickCount64();
    }
}

void LayoutVideoOverlay() {
    if (!g_video || !g_overlay) return;
    RECT vr{};
    GetClientRect(g_video, &vr);
    MoveWindow(g_overlay, 0, 0, std::max(0, static_cast<int>(vr.right - vr.left)), std::max(0, static_cast<int>(vr.bottom - vr.top)), TRUE);
    SetWindowPos(g_overlay, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void LayoutChildren(HWND hwnd) {
    RECT cr{};
    GetClientRect(hwnd, &cr);
    const int width = std::max(0, static_cast<int>(cr.right - cr.left));
    const int height = std::max(0, static_cast<int>(cr.bottom - cr.top));

    if (g_fullscreen) {
        ShowWindow(g_menuBar, SW_HIDE);
        MoveWindow(g_video, 0, 0, width, height, TRUE);
        MoveWindow(g_panel, 0, std::max(0, height - kPanelHeight), width, kPanelHeight, TRUE);
        if (g_controlsVisible) SetWindowPos(g_panel, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        ShowWindow(g_menuBar, SW_SHOW);
        MoveWindow(g_menuBar, 0, 0, width, kMenuHeight, TRUE);
        const int videoHeight = std::max(0, height - kMenuHeight - kPanelHeight);
        MoveWindow(g_video, 0, kMenuHeight, width, videoHeight, TRUE);
        MoveWindow(g_panel, 0, kMenuHeight + videoHeight, width, kPanelHeight, TRUE);
        SetControlsVisible(true);
    }

    LayoutVideoOverlay();
    if (!g_panel) return;
    RECT pr{};
    GetClientRect(g_panel, &pr);
    const int pw = std::max(0, static_cast<int>(pr.right - pr.left));

    MoveWindow(g_seek, 6, 2, std::max(20, pw - 12), kSeekHeight, TRUE);

    const int y = 25;
    const int h = kControlHeight;
    int x = 6;
    auto placeSquare = [&](HWND w) {
        MoveWindow(w, x, y, 26, h, TRUE);
        x += 30;
    };
    placeSquare(g_play);
    placeSquare(g_stop);
    placeSquare(g_prev);
    placeSquare(g_seekBack);
    placeSquare(g_seekForward);
    placeSquare(g_next);
    placeSquare(g_speedDown);
    placeSquare(g_speedUp);

    // AUTO is deliberately text-only and understated: dim grey when disabled,
    // normal transport text colour when enabled.
    MoveWindow(g_autoNext, x, y, 42, h, TRUE);
    x += 46;

    // Compact split playlist control: text button shows the native mpv OSD;
    // the adjacent triangle opens the clickable playlist menu.
    MoveWindow(g_playlistOsd, x, y, 62, h, TRUE);
    x += 64;
    MoveWindow(g_playlistMenu, x, y, 22, h, TRUE);
    x += 26;

    int right = pw - 7;
    const int timeW = 112;
    right -= timeW;
    MoveWindow(g_time, right, y + 4, timeW, 18, TRUE);

    right -= 5;
    const int volumeTextW = 42;
    right -= volumeTextW;
    MoveWindow(g_volumeText, right, y + 4, volumeTextW, 18, TRUE);

    right -= 4;
    const int volW = 80;
    right -= volW;
    MoveWindow(g_volume, right, y + 4, volW, 18, TRUE);

    right -= 4;
    right -= 26;
    MoveWindow(g_mute, right, y, 26, h, TRUE);

    // Middle information group:
    //   playback/resolution   [SUB] language   [AUD] language
    // Recompute it whenever track metadata changes so initial short placeholder
    // widths never stay stuck until the next fullscreen/resize cycle.
    const int middleLeft = x + 3;
    const int middleRight = right - 10;
    const int middleAvailable = std::max(20, middleRight - middleLeft);

    auto textWidth = [&](HWND label, const std::wstring& text, int fallback) {
        int width = fallback;
        HDC dc = GetDC(g_panel);
        if (dc) {
            HFONT oldFont = static_cast<HFONT>(SelectObject(dc, g_smallFont));
            SIZE sz{};
            if (!text.empty() &&
                GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz)) {
                width = sz.cx + 6;
            }
            SelectObject(dc, oldFont);
            ReleaseDC(g_panel, dc);
        }
        return width;
    };

    int statusW = textWidth(g_status, g_statusText, 90);
    int audioInfoW = textWidth(g_audioInfo, g_audioInfoText, 56);
    int subInfoW = textWidth(g_subInfo, g_subInfoText, 56);

    constexpr int trackButtonW = 38;
    constexpr int gap = 4;
    const int fixedW = trackButtonW * 2 + gap * 4;
    int textBudget = std::max(30, middleAvailable - fixedW);
    const int desiredText = statusW + audioInfoW + subInfoW;

    if (desiredText > textBudget) {
        // Preserve readable track labels first, then let the playback field ellipsize.
        audioInfoW = std::min(audioInfoW, std::max(42, textBudget / 4));
        subInfoW = std::min(subInfoW, std::max(42, textBudget / 4));
        statusW = std::max(30, textBudget - audioInfoW - subInfoW);
    }

    int middleX = middleLeft;
    MoveWindow(g_status, middleX, y + 4, statusW, 18, TRUE);
    middleX += statusW + gap;

    MoveWindow(g_subs, middleX, y, trackButtonW, h, TRUE);
    middleX += trackButtonW + gap;
    MoveWindow(g_subInfo, middleX, y + 4, subInfoW, 18, TRUE);
    middleX += subInfoW + gap;

    MoveWindow(g_audio, middleX, y, trackButtonW, h, TRUE);
    middleX += trackButtonW + gap;
    MoveWindow(g_audioInfo, middleX, y + 4, audioInfoW, 18, TRUE);

}

void SetFullscreenCursorHidden(bool hidden) {
    if (g_fullscreenCursorHidden == hidden) return;
    g_fullscreenCursorHidden = hidden;
    SetCursor(hidden ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
}

void NoteFullscreenMouseActivity() {
    if (!g_fullscreen) return;
    g_lastFullscreenMouseActivity = GetTickCount64();
    SetFullscreenCursorHidden(false);
}

void EnterFullscreen() {
    if (g_fullscreen) return;
    g_windowedStyle = static_cast<DWORD>(GetWindowLongPtrW(g_main, GWL_STYLE));
    g_windowedExStyle = static_cast<DWORD>(GetWindowLongPtrW(g_main, GWL_EXSTYLE));
    GetWindowRect(g_main, &g_windowedRect);
    g_windowedPlacement = WINDOWPLACEMENT{ sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(g_main, &g_windowedPlacement);

    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(g_main, MONITOR_DEFAULTTONEAREST), &mi);
    g_fullscreen = true;
    ShowWindow(g_menuBar, SW_HIDE);
    SetWindowLongPtrW(g_main, GWL_STYLE, (g_windowedStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(g_main, GWL_EXSTYLE, g_windowedExStyle & ~WS_EX_CLIENTEDGE);
    SetWindowPos(g_main, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    g_controlsVisible = false;
    ShowWindow(g_panel, SW_HIDE);
    g_lastFullscreenMouseActivity = GetTickCount64();
    SetFullscreenCursorHidden(false);
    SetTimer(g_main, TIMER_FULLSCREEN_HOVER, 100, nullptr);
    LayoutChildren(g_main);
}

void ExitFullscreen() {
    if (!g_fullscreen) return;
    KillTimer(g_main, TIMER_FULLSCREEN_HOVER);
    SetFullscreenCursorHidden(false);
    g_fullscreen = false;
    SetWindowLongPtrW(g_main, GWL_STYLE, g_windowedStyle);
    SetWindowLongPtrW(g_main, GWL_EXSTYLE, g_windowedExStyle);
    ShowWindow(g_menuBar, SW_SHOW);

    if (g_windowedPlacement.length == sizeof(WINDOWPLACEMENT)) {
        SetWindowPlacement(g_main, &g_windowedPlacement);
        SetWindowPos(g_main, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    } else {
        SetWindowPos(g_main, nullptr,
                     g_windowedRect.left, g_windowedRect.top,
                     g_windowedRect.right - g_windowedRect.left,
                     g_windowedRect.bottom - g_windowedRect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }
    g_controlsVisible = true;
    ShowWindow(g_panel, SW_SHOW);
    LayoutChildren(g_main);
}

void ToggleFullscreen() {
    if (g_fullscreen) ExitFullscreen(); else EnterFullscreen();
}

void CheckFullscreenControlsHover() {
    if (!g_fullscreen) return;
    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(g_main, &cursor);
    RECT client{};
    GetClientRect(g_main, &client);
    const ULONGLONG now = GetTickCount64();
    const bool inClient = PtInRect(&client, cursor) != FALSE;
    const bool overControls =
        inClient && g_controlsVisible &&
        cursor.y >= client.bottom - kPanelHeight && cursor.y <= client.bottom + 2;

    // Never let a fullscreen player on one monitor hide the pointer while it
    // is actually parked on another monitor/window.
    if (!inClient) SetFullscreenCursorHidden(false);

    // Keep the pointer available while the control panel is being used. Over
    // the video itself, hide it after a short idle period like normal players.
    if (overControls) {
        g_lastControlsHover = now;
        SetFullscreenCursorHidden(false);
    } else if (inClient && GetCapture() == nullptr &&
               now - g_lastFullscreenMouseActivity >= kFullscreenCursorHideMs) {
        SetFullscreenCursorHidden(true);
    }

    if (!g_controlsVisible) {
        if (inClient && cursor.y >= client.bottom - kRevealEdge && cursor.y <= client.bottom + 2) {
            SetControlsVisible(true);
        }
        return;
    }
    if (overControls) return;
    if (now - g_lastControlsHover > 1100 && GetCapture() == nullptr) SetControlsVisible(false);
}

RECT MenuItemRect(int index) {
    static constexpr int widths[] = { 42, 44, 42, 44 };
    int x = 6;
    for (int i = 0; i < index; ++i) x += widths[i];
    return RECT{ x, 0, x + widths[index], kMenuHeight };
}

int HitTestMenu(int x, int y) {
    if (y < 0 || y >= kMenuHeight) return -1;
    for (int i = 0; i < 4; ++i) {
        RECT r = MenuItemRect(i);
        if (x >= r.left && x < r.right) return i;
    }
    return -1;
}

LRESULT CALLBACK MenuMessageFilterProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == MSGF_MENU && g_menuBar && g_activeTopMenu >= 0) {
        MSG* message = reinterpret_cast<MSG*>(lParam);
        if (message && (message->message == WM_MOUSEMOVE ||
                        message->message == WM_LBUTTONDOWN ||
                        message->message == WM_LBUTTONUP)) {
            POINT p{};
            GetCursorPos(&p);
            ScreenToClient(g_menuBar, &p);
            const int hit = HitTestMenu(p.x, p.y);
            if (hit >= 0 && hit != g_activeTopMenu) {
                g_menuSwitchRequest = hit;
                EndMenu();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_menuHook, code, wParam, lParam);
}

HMENU BuildContextTrackSubmenu(bool audio, const std::vector<TrackInfo>& tracks) {
    HMENU submenu = CreatePopupMenu();
    if (!submenu) return nullptr;

    if (!audio) {
        bool anySelected = false;
        for (const auto& t : tracks) {
            if (t.selected) {
                anySelected = true;
                break;
            }
        }

        AppendMenuW(submenu,
                    MF_STRING | (anySelected ? 0 : MF_CHECKED),
                    IDM_CTX_SUB_TRACK_OFF,
                    L"Off");

        if (!tracks.empty()) {
            AppendMenuW(submenu, MF_SEPARATOR, 0, nullptr);
        }
    }

    if (tracks.empty()) {
        AppendMenuW(submenu, MF_STRING | MF_GRAYED, 0,
                    audio ? L"No audio tracks" : L"No subtitle tracks");
        return submenu;
    }

    const int base = audio ? IDM_CTX_AUDIO_TRACK_BASE : IDM_CTX_SUB_TRACK_BASE;
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        const std::wstring label = TrackMenuLabel(track);
        AppendMenuW(submenu,
                    MF_STRING | (track.selected ? MF_CHECKED : 0),
                    static_cast<UINT_PTR>(base + static_cast<int>(i)),
                    label.c_str());
    }

    return submenu;
}

HMENU BuildPlaylistSubmenu() {
    HMENU submenu = CreatePopupMenu();
    if (!submenu) return nullptr;

    const std::wstring showLabel =
        ShortcutMenuLabel(L"Show / hide playlist", ShortcutAction::TogglePlaylist);
    const std::wstring prevLabel =
        ShortcutMenuLabel(L"Previous item", ShortcutAction::PreviousPlaylist);
    const std::wstring nextLabel =
        ShortcutMenuLabel(L"Next item", ShortcutAction::NextPlaylist);

    AppendMenuW(submenu, MF_STRING | (g_playlistOsdVisible ? MF_CHECKED : 0),
                IDM_PLAYLIST_SHOW, showLabel.c_str());
    AppendMenuW(submenu, MF_STRING, IDM_PLAYLIST_PREV, prevLabel.c_str());
    AppendMenuW(submenu, MF_STRING, IDM_PLAYLIST_NEXT, nextLabel.c_str());
    AppendMenuW(submenu, MF_SEPARATOR, 0, nullptr);

    const auto entries = GetPlaylistEntries();
    if (entries.empty()) {
        AppendMenuW(submenu, MF_STRING | MF_GRAYED, 0, L"No playlist items");
        return submenu;
    }

    const bool anyPlaying = std::any_of(
        entries.begin(), entries.end(),
        [](const PlaylistEntry& entry) { return entry.playing; });

    const size_t visibleCount =
        std::min(entries.size(), static_cast<size_t>(kMaxPlaylistMenuItems));
    for (size_t i = 0; i < visibleCount; ++i) {
        const auto& entry = entries[i];
        const std::wstring label = PlaylistEntryLabel(entry, i);
        const bool active = entry.playing || (!anyPlaying && entry.current);
        AppendMenuW(submenu,
                    MF_STRING | (active ? MF_CHECKED : 0),
                    static_cast<UINT_PTR>(IDM_CTX_PLAYLIST_BASE + static_cast<int>(i)),
                    label.c_str());
    }

    if (entries.size() > visibleCount) {
        AppendMenuW(submenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(submenu, MF_STRING | MF_GRAYED, 0, L"Playlist too large to show completely");
    }

    return submenu;
}

void ShowPlaylistPopup(HWND anchor) {
    HMENU menu = BuildPlaylistSubmenu();
    if (!menu) return;

    RECT r{};
    GetWindowRect(anchor ? anchor : g_main, &r);
    const int chosen = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN,
        r.left, r.top, 0, g_main, nullptr);

    if (chosen) HandlePlaylistMenuChoice(chosen);
    DestroyMenu(menu);
}

void BuildMenus() {
    g_fileMenu = CreatePopupMenu();
    g_viewMenu = CreatePopupMenu();
    g_playMenu = CreatePopupMenu();
    g_helpMenu = CreatePopupMenu();

    const std::wstring openLabel =
        ShortcutMenuLabel(L"&Open...", ShortcutAction::OpenFile);
    AppendMenuW(g_fileMenu, MF_STRING, IDM_FILE_OPEN, openLabel.c_str());
    AppendMenuW(g_fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");

    const std::wstring fullscreenLabel =
        ShortcutMenuLabel(L"&Fullscreen", ShortcutAction::ToggleFullscreen);
    const std::wstring mediaInfoLabel =
        ShortcutMenuLabel(L"&Media info", ShortcutAction::MediaInfo);
    AppendMenuW(g_viewMenu, MF_STRING, IDM_VIEW_FULLSCREEN, fullscreenLabel.c_str());
    AppendMenuW(g_viewMenu, MF_STRING, IDM_VIEW_MEDIA_INFO, mediaInfoLabel.c_str());
    AppendMenuW(g_viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_viewMenu, MF_STRING, IDM_VIEW_OPTIONS, L"&Options...");

    HMENU mpvConfigMenu = CreatePopupMenu();
    AppendMenuW(mpvConfigMenu, MF_STRING,
                IDM_VIEW_OPEN_MPV_CONFIG, L"&Open mpv.conf");
    AppendMenuW(g_viewMenu, MF_POPUP,
                reinterpret_cast<UINT_PTR>(mpvConfigMenu),
                L"mpv &configuration");

    const std::wstring playLabel =
        ShortcutMenuLabel(L"&Play / Pause", ShortcutAction::PlayPause);
    const std::wstring stopLabel =
        ShortcutMenuLabel(L"&Stop", ShortcutAction::Stop);
    const std::wstring prevChapterLabel =
        ShortcutMenuLabel(L"Previous chapter", ShortcutAction::PreviousChapter);
    const std::wstring seekBackLabel =
        ShortcutMenuLabel(L"Seek back 5 seconds", ShortcutAction::SeekBackward);
    const std::wstring seekForwardLabel =
        ShortcutMenuLabel(L"Seek forward 5 seconds", ShortcutAction::SeekForward);
    const std::wstring nextChapterLabel =
        ShortcutMenuLabel(L"Next chapter", ShortcutAction::NextChapter);
    const std::wstring speedDownLabel =
        ShortcutMenuLabel(L"Decrease speed", ShortcutAction::SpeedDown);
    const std::wstring speedUpLabel =
        ShortcutMenuLabel(L"Increase speed", ShortcutAction::SpeedUp);
    const std::wstring muteLabel =
        ShortcutMenuLabel(L"Mute", ShortcutAction::Mute);

    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_TOGGLE, playLabel.c_str());
    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_STOP, stopLabel.c_str());
    AppendMenuW(g_playMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_PREV, prevChapterLabel.c_str());
    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_BACK, seekBackLabel.c_str());
    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_FORWARD, seekForwardLabel.c_str());
    AppendMenuW(g_playMenu, MF_STRING, IDM_PLAY_NEXT, nextChapterLabel.c_str());
    AppendMenuW(g_playMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_playMenu, MF_STRING, IDM_SPEED_DOWN, speedDownLabel.c_str());
    AppendMenuW(g_playMenu, MF_STRING, IDM_SPEED_UP, speedUpLabel.c_str());
    AppendMenuW(g_playMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_playMenu, MF_STRING, IDM_AUDIO_TRACKS, L"Audio tracks...");
    AppendMenuW(g_playMenu, MF_STRING, IDM_SUB_TRACKS, L"Subtitle tracks...");
    AppendMenuW(g_playMenu, MF_STRING, IDM_MUTE, muteLabel.c_str());

    AppendMenuW(g_helpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
}

void RefreshMenuShortcutLabels() {
    if (g_fileMenu) {
        const std::wstring label =
            ShortcutMenuLabel(L"&Open...", ShortcutAction::OpenFile);
        ModifyMenuW(g_fileMenu, IDM_FILE_OPEN,
                    MF_BYCOMMAND | MF_STRING, IDM_FILE_OPEN, label.c_str());
    }

    if (g_viewMenu) {
        const std::wstring fullscreen =
            ShortcutMenuLabel(L"&Fullscreen", ShortcutAction::ToggleFullscreen);
        const std::wstring mediaInfo =
            ShortcutMenuLabel(L"&Media info", ShortcutAction::MediaInfo);
        ModifyMenuW(g_viewMenu, IDM_VIEW_FULLSCREEN,
                    MF_BYCOMMAND | MF_STRING, IDM_VIEW_FULLSCREEN,
                    fullscreen.c_str());
        ModifyMenuW(g_viewMenu, IDM_VIEW_MEDIA_INFO,
                    MF_BYCOMMAND | MF_STRING, IDM_VIEW_MEDIA_INFO,
                    mediaInfo.c_str());
    }

    if (g_playMenu) {
        struct MenuShortcut {
            UINT id;
            const wchar_t* label;
            ShortcutAction action;
        };

        const MenuShortcut items[] = {
            { IDM_PLAY_TOGGLE, L"&Play / Pause", ShortcutAction::PlayPause },
            { IDM_PLAY_STOP, L"&Stop", ShortcutAction::Stop },
            { IDM_PLAY_PREV, L"Previous chapter", ShortcutAction::PreviousChapter },
            { IDM_PLAY_BACK, L"Seek back 5 seconds", ShortcutAction::SeekBackward },
            { IDM_PLAY_FORWARD, L"Seek forward 5 seconds", ShortcutAction::SeekForward },
            { IDM_PLAY_NEXT, L"Next chapter", ShortcutAction::NextChapter },
            { IDM_SPEED_DOWN, L"Decrease speed", ShortcutAction::SpeedDown },
            { IDM_SPEED_UP, L"Increase speed", ShortcutAction::SpeedUp },
            { IDM_MUTE, L"Mute", ShortcutAction::Mute }
        };

        for (const auto& item : items) {
            const std::wstring label = ShortcutMenuLabel(item.label, item.action);
            ModifyMenuW(g_playMenu, item.id,
                        MF_BYCOMMAND | MF_STRING, item.id, label.c_str());
        }
    }
}

void ShowTopMenu(int index) {
    int current = index;

    for (;;) {
        HMENU popup = nullptr;
        switch (current) {
        case 0: popup = g_fileMenu; break;
        case 1: popup = g_viewMenu; break;
        case 2: popup = g_playMenu; break;
        case 3: popup = g_helpMenu; break;
        default: return;
        }

        g_activeTopMenu = current;
        g_menuSwitchRequest = -1;
        g_menuHover = current;
        InvalidateRect(g_menuBar, nullptr, FALSE);

        RECT item = MenuItemRect(current);
        POINT p{ item.left, item.bottom };
        ClientToScreen(g_menuBar, &p);

        g_menuHook = SetWindowsHookExW(
            WH_MSGFILTER, MenuMessageFilterProc, nullptr, GetCurrentThreadId());

        const int chosen = TrackPopupMenu(
            popup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
            p.x, p.y, 0, g_main, nullptr);

        if (g_menuHook) {
            UnhookWindowsHookEx(g_menuHook);
            g_menuHook = nullptr;
        }

        if (g_menuSwitchRequest >= 0 && g_menuSwitchRequest != current) {
            current = g_menuSwitchRequest;
            continue;
        }

        g_activeTopMenu = -1;
        if (chosen) SendMessageW(g_main, WM_COMMAND, static_cast<WPARAM>(chosen), 0);
        break;
    }

    g_activeTopMenu = -1;
    g_menuHover = -1;
    InvalidateRect(g_menuBar, nullptr, FALSE);
}

void ShowContextMenu(POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    // Fetch each type once for this popup. The toolbar AUD/SUB buttons continue
    // to use ShowTrackMenu(); only the right-click path becomes cascading.
    const auto audioTracks = GetTracks("audio");
    const auto subtitleTracks = GetTracks("sub");

    HMENU audioMenu = BuildContextTrackSubmenu(true, audioTracks);
    HMENU subtitleMenu = BuildContextTrackSubmenu(false, subtitleTracks);
    HMENU playlistMenu = BuildPlaylistSubmenu();

    AppendMenuW(menu, MF_STRING, IDM_FILE_OPEN, L"Open...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_PLAY_TOGGLE, L"Play / Pause");
    AppendMenuW(menu, MF_STRING, IDM_PLAY_STOP, L"Stop");
    AppendMenuW(menu,
                MF_STRING | (g_autoPlayNextFile ? MF_CHECKED : 0),
                IDM_AUTO_NEXT_FILE,
                L"Auto-play next file in folder");
    if (playlistMenu) {
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(playlistMenu),
                    L"Playlist");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    if (audioMenu) {
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(audioMenu),
                    L"Audio tracks");
    }
    if (subtitleMenu) {
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(subtitleMenu),
                    L"Subtitle tracks");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_VIEW_FULLSCREEN, L"Fullscreen");
    AppendMenuW(menu, MF_STRING, IDM_VIEW_MEDIA_INFO, L"Media info");
    AppendMenuW(menu, MF_STRING, IDM_VIEW_OPTIONS, L"Options...");

    const int chosen = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        screenPoint.x, screenPoint.y, 0, g_main, nullptr);

    if (HandlePlaylistMenuChoice(chosen)) {
        // Handled by the shared playlist menu path.
    } else if (chosen == IDM_CTX_SUB_TRACK_OFF) {
        if (g_mpvReady &&
            g_mpvApi.set_property_string(g_mpv, "sid", "no") >= 0) {
            RememberSubtitlesOff();
            g_pollCounter = 3;
            PollMpv();
        }
    } else if (chosen >= IDM_CTX_AUDIO_TRACK_BASE &&
               chosen < IDM_CTX_AUDIO_TRACK_BASE + static_cast<int>(audioTracks.size())) {
        const size_t index = static_cast<size_t>(chosen - IDM_CTX_AUDIO_TRACK_BASE);
        int64_t id = audioTracks[index].id;
        if (g_mpvReady &&
            g_mpvApi.set_property(g_mpv, "aid", MPV_FORMAT_INT64, &id) >= 0) {
            RememberAudioTrack(audioTracks[index]);
            g_pollCounter = 3;
            PollMpv();
        }
    } else if (chosen >= IDM_CTX_SUB_TRACK_BASE &&
               chosen < IDM_CTX_SUB_TRACK_BASE + static_cast<int>(subtitleTracks.size())) {
        const size_t index = static_cast<size_t>(chosen - IDM_CTX_SUB_TRACK_BASE);
        int64_t id = subtitleTracks[index].id;
        if (g_mpvReady &&
            g_mpvApi.set_property(g_mpv, "sid", MPV_FORMAT_INT64, &id) >= 0) {
            RememberSubtitleTrack(subtitleTracks[index]);
            g_pollCounter = 3;
            PollMpv();
        }
    } else if (chosen) {
        SendMessageW(g_main, WM_COMMAND, static_cast<WPARAM>(chosen), 0);
    }

    // Destroying the parent menu also destroys its attached submenus.
    DestroyMenu(menu);
}

void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old = static_cast<HPEN>(SelectObject(dc, pen));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawTriangle(HDC dc, POINT a, POINT b, POINT c, COLORREF color) {
    POINT pts[] = { a, b, c };
    HBRUSH br = CreateSolidBrush(color);
    HBRUSH old = static_cast<HBRUSH>(SelectObject(dc, br));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    Polygon(dc, pts, 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, old);
    DeleteObject(pen);
    DeleteObject(br);
}

void DrawButtonGlyph(HDC dc, int id, const RECT& r) {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const COLORREF fg = C_TEXT;

    if (id == IDC_PLAY) {
        if (g_playing) {
            RECT a{ cx - 5, cy - 7, cx - 1, cy + 7 };
            RECT b{ cx + 2, cy - 7, cx + 6, cy + 7 };
            FillSolid(dc, a, fg);
            FillSolid(dc, b, fg);
        } else {
            DrawTriangle(dc, {cx - 5, cy - 8}, {cx - 5, cy + 8}, {cx + 8, cy}, fg);
        }
        return;
    }
    if (id == IDC_STOP) {
        RECT s{ cx - 6, cy - 6, cx + 6, cy + 6 };
        FillSolid(dc, s, fg);
        return;
    }
    if (id == IDC_PREV || id == IDC_NEXT) {
        const bool next = id == IDC_NEXT;
        const int dir = next ? 1 : -1;
        const int barX = cx + dir * 7;
        DrawLine(dc, barX, cy - 7, barX, cy + 7, fg, 2);
        if (next) DrawTriangle(dc, {cx - 6, cy - 7}, {cx - 6, cy + 7}, {cx + 5, cy}, fg);
        else DrawTriangle(dc, {cx + 6, cy - 7}, {cx + 6, cy + 7}, {cx - 5, cy}, fg);
        return;
    }
    if (id == IDC_SEEK_BACK || id == IDC_SEEK_FORWARD) {
        const bool next = id == IDC_SEEK_FORWARD;
        if (next) {
            DrawTriangle(dc, {cx - 8, cy - 6}, {cx - 8, cy + 6}, {cx, cy}, fg);
            DrawTriangle(dc, {cx, cy - 6}, {cx, cy + 6}, {cx + 8, cy}, fg);
        } else {
            DrawTriangle(dc, {cx + 8, cy - 6}, {cx + 8, cy + 6}, {cx, cy}, fg);
            DrawTriangle(dc, {cx, cy - 6}, {cx, cy + 6}, {cx - 8, cy}, fg);
        }
        return;
    }
    if (id == IDC_SPEED_DOWN) {
        RECT minus{ cx - 6, cy - 1, cx + 7, cy + 2 };
        FillSolid(dc, minus, fg);
        return;
    }
    if (id == IDC_SPEED_UP) {
        RECT horiz{ cx - 6, cy - 1, cx + 7, cy + 2 };
        RECT vert{ cx - 1, cy - 6, cx + 2, cy + 7 };
        FillSolid(dc, horiz, fg);
        FillSolid(dc, vert, fg);
        return;
    }
    if (id == IDC_PLAYLIST_MENU) {
        DrawTriangle(dc, {cx - 6, cy - 3}, {cx + 6, cy - 3}, {cx, cy + 4}, fg);
        return;
    }
    if (id == IDC_MUTE) {
        // Minimal speaker based on the user's reference. No sound-wave lines.
        // When muted, the entire icon (speaker + slash) turns teal.
        const COLORREF iconColor = g_muted ? C_TEAL : fg;
        const int sx = cx - 1;

        RECT body{ sx - 8, cy - 4, sx - 4, cy + 5 };
        FillSolid(dc, body, iconColor);

        POINT spk[] = {
            {sx - 4, cy - 4},
            {sx + 2, cy - 8},
            {sx + 2, cy + 8},
            {sx - 4, cy + 4}
        };
        HBRUSH br = CreateSolidBrush(iconColor);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, br));
        HPEN pen = CreatePen(PS_SOLID, 1, iconColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
        Polygon(dc, spk, 4);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(br);

        if (g_muted) {
            DrawLine(dc, sx - 8, cy - 8, sx + 8, cy + 8, C_TEAL, 2);
        }
        return;
    }

    const wchar_t* text =
        id == IDC_AUDIO ? L"AUD" :
        (id == IDC_SUBS ? L"SUB" :
        (id == IDC_AUTO_NEXT ? L"AUTO" :
        (id == IDC_PLAYLIST_OSD ? L"PLAYLIST" : L"")));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc,
                 id == IDC_AUTO_NEXT && !g_autoPlayNextFile
                    ? C_AUTO_OFF_TEXT
                    : fg);
    HFONT old = static_cast<HFONT>(SelectObject(dc, g_smallFont));
    RECT tr = r;
    DrawTextW(dc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LONG_PTR state = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    bool hover = (state & 1) != 0;
    bool pressed = (state & 2) != 0;

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(dc, r, pressed ? C_BUTTON_DOWN : C_BUTTON);
        if (hover) {
            HPEN pen = CreatePen(PS_SOLID, 1, C_HOVER);
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));
            HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
        }
        DrawButtonGlyph(dc, GetDlgCtrlID(hwnd), r);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!hover) {
            state |= 1;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        state &= ~1;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        state |= 2;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        const bool wasPressed = (state & 2) != 0;
        state &= ~2;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
        if (GetCapture() == hwnd) ReleaseCapture();
        RECT r{};
        GetClientRect(hwnd, &r);
        POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (wasPressed && PtInRect(&r, p)) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CAPTURECHANGED:
        state &= ~2;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ApplySeekFromX(HWND hwnd, int x) {
    if (g_duration <= 0.0) return;
    RECT r{};
    GetClientRect(hwnd, &r);
    const int width = std::max(1, static_cast<int>(r.right - r.left - 2));
    const int local = std::clamp(x - 1, 0, width);
    SeekAbsolute((static_cast<double>(local) / width) * g_duration);
}

LRESULT CALLBACK SeekProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillRect(dc, &r, g_seekBgBrush);
        if (g_duration > 0.0) {
            const double frac = std::clamp(g_timePos / g_duration, 0.0, 1.0);
            const int usable = std::max(1, static_cast<int>(r.right - r.left - 2));
            const int x = 1 + static_cast<int>(frac * usable + 0.5);

            // Fill the elapsed portion subtly, MPC-style, while keeping the overall bar dark.
            RECT elapsed{ r.left, r.top, static_cast<LONG>(std::clamp(x, static_cast<int>(r.left), static_cast<int>(r.right))), r.bottom };
            if (elapsed.right > elapsed.left) {
                FillRect(dc, &elapsed, g_seekFillBrush);
            }

            // MPC-style chapter markers: a thin grey line at each chapter boundary.
            // Cached brushes avoid creating/destroying one GDI brush per marker.
            for (double chapterTime : g_chapterTimes) {
                if (chapterTime <= 0.001 || chapterTime >= g_duration - 0.001) continue;
                const double chapterFrac =
                    std::clamp(chapterTime / g_duration, 0.0, 1.0);
                const int chapterX =
                    1 + static_cast<int>(chapterFrac * usable + 0.5);
                RECT marker{
                    chapterX,
                    r.top,
                    chapterX + 1,
                    r.bottom
                };
                FillRect(dc, &marker, g_chapterMarkerBrush);
            }

            // Three-pixel teal playhead: easier to find without becoming a chunky knob.
            RECT line{ x - 1, r.top, x + 2, r.bottom };
            FillRect(dc, &line, g_seekPlayheadBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        g_seekPointerDown = true;
        g_seekDragging = false;
        g_seekMouseDownPoint = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        // A normal click seeks on release, not on button-down. This avoids the
        // old two-seek "twitch" while still letting playback continue.
        return 0;
    case WM_MOUSEMOVE:
        if (g_seekPointerDown && GetCapture() == hwnd) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (!g_seekDragging) {
                const int dragX = std::max(1, GetSystemMetrics(SM_CXDRAG));
                const int dragY = std::max(1, GetSystemMetrics(SM_CYDRAG));
                if (std::abs(x - g_seekMouseDownPoint.x) >= dragX ||
                    std::abs(y - g_seekMouseDownPoint.y) >= dragY) {
                    g_seekDragging = true;
                }
            }
            if (g_seekDragging) ApplySeekFromX(hwnd, x);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_seekPointerDown) {
            // A click seeks exactly once here. A drag has already sought live
            // on mouse movement, so release only ends the interaction.
            if (!g_seekDragging) ApplySeekFromX(hwnd, GET_X_LPARAM(lParam));
            g_seekPointerDown = false;
            g_seekDragging = false;
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        return 0;
    case WM_CAPTURECHANGED:
        g_seekPointerDown = false;
        g_seekDragging = false;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ApplyVolumeFromX(HWND hwnd, int x) {
    RECT r{};
    GetClientRect(hwnd, &r);
    const int usable = std::max(1, static_cast<int>(r.right - r.left - 2));
    const int local = std::clamp(x - 1, 0, usable);
    const int value = static_cast<int>((static_cast<long long>(local) * 100 + usable / 2) / usable);
    SetVolume(value);
}

LRESULT CALLBACK VolumeProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(dc, r, C_BLACK);
        RECT inner{ r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
        FillSolid(dc, inner, C_VOLUME_BG);
        const int innerW = std::max(0, static_cast<int>(inner.right - inner.left));
        RECT fill = inner;
        fill.right = fill.left + (innerW * g_volumePercent) / 100;
        if (fill.right > fill.left) FillSolid(dc, fill, C_VOLUME_FILL);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        g_volumeDragging = true;
        ApplyVolumeFromX(hwnd, GET_X_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        if (g_volumeDragging && GetCapture() == hwnd) ApplyVolumeFromX(hwnd, GET_X_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        if (g_volumeDragging) {
            ApplyVolumeFromX(hwnd, GET_X_LPARAM(lParam));
            g_volumeDragging = false;
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        return 0;
    case WM_CAPTURECHANGED:
        g_volumeDragging = false;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MenuProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(reinterpret_cast<HDC>(wParam), r, C_PANEL);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(dc, r, C_PANEL);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT);
        HFONT old = static_cast<HFONT>(SelectObject(dc, g_font));
        static constexpr const wchar_t* labels[] = { L"File", L"View", L"Play", L"Help" };
        for (int i = 0; i < 4; ++i) {
            RECT item = MenuItemRect(i);
            if (i == g_menuHover) FillSolid(dc, item, RGB(48, 48, 48));
            RECT tr = item;
            tr.left += 6;
            DrawTextW(dc, labels[i], -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        SelectObject(dc, old);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int hit = HitTestMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit != g_menuHover) {
            g_menuHover = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_menuHover = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const int hit = HitTestMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit >= 0) ShowTopMenu(hit);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(reinterpret_cast<HDC>(wParam), r, C_PANEL);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(dc, r, C_PANEL);
        DrawLine(dc, 0, 0, r.right, 0, C_PANEL_EDGE, 1);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, C_PANEL);
        const HWND control = reinterpret_cast<HWND>(lParam);
        const bool dim = control == g_status || control == g_audioInfo || control == g_subInfo;
        SetTextColor(dc, dim ? C_TEXT_DIM : C_TEXT);
        return reinterpret_cast<LRESULT>(g_panelBrush);
    }
    case WM_COMMAND:
        SendMessageW(g_main, WM_COMMAND, wParam, lParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK VideoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(reinterpret_cast<HDC>(wParam), r, C_BG);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        if (!g_mpvReady || MpvGetFlag("idle-active", true)) {
            FillSolid(dc, r, C_BG);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(95, 95, 95));
            HFONT old = static_cast<HFONT>(SelectObject(dc, g_font));
            const wchar_t* idleText = g_initializingPlaybackEngine
                ? L"MPV WinterStatic Edition\nInitializing playback engine..."
                : L"MPV WinterStatic Edition\nMPC-style native player";
            DrawTextW(dc, idleText, -1, &r,
                      DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, old);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{};
        GetClientRect(hwnd, &r);
        FillSolid(dc, r, RGB(0, 0, 0));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        NoteFullscreenMouseActivity();
        break;
    case WM_LBUTTONUP:
        NoteFullscreenMouseActivity();
        if (g_ignoreNextVideoButtonUp) {
            g_ignoreNextVideoButtonUp = false;
            return 0;
        }
        // MPC-like compromise: single-click responds almost immediately, but a fast double-click
        // still gets fullscreen without leaving playback toggled.
        g_singleClickApplied = false;
        g_pendingSingleClick = true;
        SetTimer(g_main, TIMER_SINGLE_CLICK, g_singleClickDelayMs, nullptr);
        return 0;
    case WM_LBUTTONDBLCLK:
        NoteFullscreenMouseActivity();
        if (g_pendingSingleClick) {
            KillTimer(g_main, TIMER_SINGLE_CLICK);
            g_pendingSingleClick = false;
        } else if (g_singleClickApplied) {
            // If the shorter timer already fired, undo that one toggle before fullscreen.
            TogglePlay();
        }
        g_singleClickApplied = false;
        g_ignoreNextVideoButtonUp = true;
        ToggleFullscreen();
        return 0;
    case WM_RBUTTONDOWN:
        NoteFullscreenMouseActivity();
        break;
    case WM_RBUTTONUP: {
        NoteFullscreenMouseActivity();
        POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &p);
        ShowContextMenu(p);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_fullscreen) {
            NoteFullscreenMouseActivity();
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &p);
            ScreenToClient(g_main, &p);
            RECT r{};
            GetClientRect(g_main, &r);
            if (!g_controlsVisible && p.y >= r.bottom - kRevealEdge) SetControlsVisible(true);
        }
        return 0;
    case WM_SETCURSOR:
        if (g_fullscreen && g_fullscreenCursorHidden &&
            LOWORD(lParam) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND CreateButton(HWND parent, int id) {
    return CreateWindowExW(0, kButtonClass, nullptr,
                           WS_CHILD | WS_VISIBLE,
                           0, 0, 26, 26, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
}

void CreateUi(HWND hwnd) {
    g_menuBar = CreateWindowExW(0, kMenuClass, nullptr, WS_CHILD | WS_VISIBLE,
                                0, 0, 100, kMenuHeight, hwnd, nullptr, g_instance, nullptr);
    g_video = CreateWindowExW(0, kVideoClass, nullptr,
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                              0, 0, 100, 100, hwnd, nullptr, g_instance, nullptr);
    g_overlay = CreateWindowExW(WS_EX_LAYERED, kOverlayClass, nullptr,
                                WS_CHILD | WS_VISIBLE,
                                0, 0, 100, 100, g_video, nullptr, g_instance, nullptr);
    if (g_overlay) SetLayeredWindowAttributes(g_overlay, 0, 1, LWA_ALPHA);
    g_panel = CreateWindowExW(0, kPanelClass, nullptr,
                              WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, 100, kPanelHeight, hwnd, nullptr, g_instance, nullptr);

    g_seek = CreateWindowExW(0, kSeekClass, nullptr, WS_CHILD | WS_VISIBLE,
                             0, 0, 100, kSeekHeight, g_panel,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEEK)), g_instance, nullptr);

    g_play = CreateButton(g_panel, IDC_PLAY);
    g_stop = CreateButton(g_panel, IDC_STOP);
    g_prev = CreateButton(g_panel, IDC_PREV);
    g_seekBack = CreateButton(g_panel, IDC_SEEK_BACK);
    g_seekForward = CreateButton(g_panel, IDC_SEEK_FORWARD);
    g_next = CreateButton(g_panel, IDC_NEXT);
    g_speedDown = CreateButton(g_panel, IDC_SPEED_DOWN);
    g_speedUp = CreateButton(g_panel, IDC_SPEED_UP);
    g_autoNext = CreateButton(g_panel, IDC_AUTO_NEXT);
    g_playlistOsd = CreateButton(g_panel, IDC_PLAYLIST_OSD);
    g_playlistMenu = CreateButton(g_panel, IDC_PLAYLIST_MENU);
    g_audio = CreateButton(g_panel, IDC_AUDIO);
    g_subs = CreateButton(g_panel, IDC_SUBS);
    g_mute = CreateButton(g_panel, IDC_MUTE);

    g_volume = CreateWindowExW(0, kVolumeClass, nullptr, WS_CHILD | WS_VISIBLE,
                               0, 0, 80, 18, g_panel,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_VOLUME)), g_instance, nullptr);

    g_status = CreateWindowExW(0, L"STATIC", L"Stopped",
                               WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS,
                               0, 0, 120, 18, g_panel,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), g_instance, nullptr);
    g_audioInfo = CreateWindowExW(0, L"STATIC", L"--",
                                  WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS,
                                  0, 0, 60, 18, g_panel,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUDIO_INFO)), g_instance, nullptr);
    g_subInfo = CreateWindowExW(0, L"STATIC", L"--",
                                WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS,
                                0, 0, 60, 18, g_panel,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SUB_INFO)), g_instance, nullptr);
    g_volumeText = CreateWindowExW(0, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_CENTER,
                                   0, 0, 42, 18, g_panel,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_VOLUME_TEXT)), g_instance, nullptr);
    g_time = CreateWindowExW(0, L"STATIC", L"00:00 / 00:00",
                             WS_CHILD | WS_VISIBLE | SS_RIGHT,
                             0, 0, 112, 18, g_panel,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TIME)), g_instance, nullptr);
    ApplyFont(g_status, true);
    ApplyFont(g_audioInfo, true);
    ApplyFont(g_subInfo, true);
    ApplyFont(g_volumeText, true);
    ApplyFont(g_time, true);
    ApplyDarkTheme(g_panel);
    ApplyDarkTheme(g_status);
    ApplyDarkTheme(g_audioInfo);
    ApplyDarkTheme(g_subInfo);
    ApplyDarkTheme(g_volumeText);
    ApplyDarkTheme(g_time);
    UpdateVolumeLabel();
    UpdateTimeLabel();

    LayoutChildren(hwnd);
}

bool ExecuteShortcutAction(ShortcutAction action) {
    switch (action) {
    case ShortcutAction::OpenFile:
        OpenFileDialog(); return true;
    case ShortcutAction::PlayPause:
        TogglePlay(); return true;
    case ShortcutAction::Stop:
        StopPlayback(); return true;
    case ShortcutAction::PreviousChapter:
        ChapterStep(false); return true;
    case ShortcutAction::NextChapter:
        ChapterStep(true); return true;
    case ShortcutAction::SeekBackward:
        SeekBy(-kSeekStepSeconds); return true;
    case ShortcutAction::SeekForward:
        SeekBy(kSeekStepSeconds); return true;
    case ShortcutAction::SpeedDown:
        AdjustSpeed(-0.1); return true;
    case ShortcutAction::SpeedUp:
        AdjustSpeed(0.1); return true;
    case ShortcutAction::VolumeUp:
        AdjustVolume(kVolumeStep); return true;
    case ShortcutAction::VolumeDown:
        AdjustVolume(-kVolumeStep); return true;
    case ShortcutAction::Mute:
        ToggleMute(); return true;
    case ShortcutAction::ToggleFullscreen:
        ToggleFullscreen(); return true;
    case ShortcutAction::ExitFullscreen:
        if (g_fullscreen) ExitFullscreen();
        return true;
    case ShortcutAction::ToggleAutoNext:
        ToggleAutoPlayNextFile(); return true;
    case ShortcutAction::TogglePlaylist:
        TogglePlaylistOsd(); return true;
    case ShortcutAction::PreviousPlaylist:
        PlaylistStep(false); return true;
    case ShortcutAction::NextPlaylist:
        PlaylistStep(true); return true;
    case ShortcutAction::CycleAudio:
        CycleAudioTrack(); return true;
    case ShortcutAction::CycleSubtitle:
        CycleSubtitleTrack(); return true;
    case ShortcutAction::AudioDelayDown:
        AdjustSyncDelayMs("audio-delay", L"Audio delay", -kSyncStepMs,
                          g_audioDelayTouched); return true;
    case ShortcutAction::AudioDelayUp:
        AdjustSyncDelayMs("audio-delay", L"Audio delay", kSyncStepMs,
                          g_audioDelayTouched); return true;
    case ShortcutAction::ResetAudioDelay:
        SetSyncDelayMs("audio-delay", L"Audio delay", 0,
                       g_audioDelayTouched); return true;
    case ShortcutAction::SubtitleDelayDown:
        AdjustSyncDelayMs("sub-delay", L"Subtitle delay", -kSyncStepMs,
                          g_subtitleDelayTouched); return true;
    case ShortcutAction::SubtitleDelayUp:
        AdjustSyncDelayMs("sub-delay", L"Subtitle delay", kSyncStepMs,
                          g_subtitleDelayTouched); return true;
    case ShortcutAction::ResetSubtitleDelay:
        SetSyncDelayMs("sub-delay", L"Subtitle delay", 0,
                       g_subtitleDelayTouched); return true;
    case ShortcutAction::MediaInfo:
        ToggleMediaInfo(); return true;
    case ShortcutAction::Count:
        break;
    }
    return false;
}

bool HandleShortcutBinding(const KeyBinding& pressed) {
    if (pressed.vk == 0) return false;

    for (size_t action = 0; action < kShortcutActionCount; ++action) {
        for (size_t slot = 0; slot < kShortcutSlotCount; ++slot) {
            const KeyBinding& configured = g_shortcuts[action][slot];
            if (configured.vk != 0 && SameKeyBinding(configured, pressed)) {
                return ExecuteShortcutAction(static_cast<ShortcutAction>(action));
            }
        }
    }
    return false;
}

bool IsAppCommandVirtualKey(UINT vk) {
    switch (vk) {
    case VK_MEDIA_PREV_TRACK:
    case VK_MEDIA_NEXT_TRACK:
    case VK_MEDIA_PLAY_PAUSE:
    case VK_MEDIA_STOP:
    case VK_VOLUME_MUTE:
    case VK_VOLUME_DOWN:
    case VK_VOLUME_UP:
        return true;
    default:
        return false;
    }
}

bool HandleMediaShortcutEvent(const KeyBinding& pressed,
                              MediaShortcutSource source) {
    if (!IsAppCommandVirtualKey(pressed.vk)) return false;

    const ULONGLONG now = GetTickCount64();
    if (g_lastHandledMediaVk == pressed.vk &&
        g_lastHandledMediaSource != source &&
        now - g_lastHandledMediaTick <= kMediaShortcutDedupMs) {
        // Many Windows keyboards emit one physical media-key press twice:
        // first as VK_MEDIA_* and again as WM_APPCOMMAND (or vice versa).
        // Consume the second representation so one press runs one action.
        return true;
    }

    const bool handled = HandleShortcutBinding(pressed);
    if (handled) {
        g_lastHandledMediaVk = pressed.vk;
        g_lastHandledMediaTick = now;
        g_lastHandledMediaSource = source;
    }
    return handled;
}

bool HandleShortcut(WPARAM key) {
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetKeyState(VK_RWIN) & 0x8000) != 0) {
        return false;
    }

    KeyBinding pressed{};
    pressed.vk = static_cast<UINT>(key);
    pressed.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    pressed.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    pressed.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (IsAppCommandVirtualKey(pressed.vk)) {
        return HandleMediaShortcutEvent(pressed, MediaShortcutSource::KeyDown);
    }
    return HandleShortcutBinding(pressed);
}

UINT VirtualKeyFromAppCommand(int appCommand) {
    switch (appCommand) {
    case APPCOMMAND_MEDIA_PREVIOUSTRACK:
        return VK_MEDIA_PREV_TRACK;
    case APPCOMMAND_MEDIA_NEXTTRACK:
        return VK_MEDIA_NEXT_TRACK;
    case APPCOMMAND_MEDIA_PLAY_PAUSE:
        return VK_MEDIA_PLAY_PAUSE;
    case APPCOMMAND_MEDIA_STOP:
        return VK_MEDIA_STOP;
    case APPCOMMAND_VOLUME_MUTE:
        return VK_VOLUME_MUTE;
    case APPCOMMAND_VOLUME_DOWN:
        return VK_VOLUME_DOWN;
    case APPCOMMAND_VOLUME_UP:
        return VK_VOLUME_UP;
    default:
        return 0;
    }
}

bool HandleMediaAppCommand(LPARAM lParam) {
    const UINT vk = VirtualKeyFromAppCommand(GET_APPCOMMAND_LPARAM(lParam));
    if (vk == 0) return false;
    return HandleMediaShortcutEvent(MakeKeyBinding(vk),
                                    MediaShortcutSource::AppCommand);
}

bool IsModifierVirtualKey(UINT vk) {
    switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

int ShortcutActionIndexFromControlId(int id) {
    if (id >= IDC_SHORTCUT_PRIMARY_BASE &&
        id < IDC_SHORTCUT_PRIMARY_BASE + static_cast<int>(kShortcutActionCount)) {
        return id - IDC_SHORTCUT_PRIMARY_BASE;
    }
    if (id >= IDC_SHORTCUT_ALT_BASE &&
        id < IDC_SHORTCUT_ALT_BASE + static_cast<int>(kShortcutActionCount)) {
        return id - IDC_SHORTCUT_ALT_BASE;
    }
    return -1;
}

int ShortcutSlotFromControlId(int id) {
    if (id >= IDC_SHORTCUT_PRIMARY_BASE &&
        id < IDC_SHORTCUT_PRIMARY_BASE + static_cast<int>(kShortcutActionCount)) {
        return 0;
    }
    if (id >= IDC_SHORTCUT_ALT_BASE &&
        id < IDC_SHORTCUT_ALT_BASE + static_cast<int>(kShortcutActionCount)) {
        return 1;
    }
    return -1;
}

void UpdateShortcutEdit(HWND dialog, size_t action, size_t slot) {
    const int id = (slot == 0 ? IDC_SHORTCUT_PRIMARY_BASE : IDC_SHORTCUT_ALT_BASE) +
                   static_cast<int>(action);
    SetDlgItemTextW(dialog, id,
                    KeyBindingText(g_shortcutDialogBindings[action][slot]).c_str());
}

void UpdateAllShortcutEdits(HWND dialog) {
    for (size_t action = 0; action < kShortcutActionCount; ++action) {
        for (size_t slot = 0; slot < kShortcutSlotCount; ++slot) {
            UpdateShortcutEdit(dialog, action, slot);
        }
    }
}

void AssignShortcutDialogBinding(HWND edit, const KeyBinding& binding) {
    if (!edit || binding.vk == 0) return;

    const int id = GetDlgCtrlID(edit);
    const int action = ShortcutActionIndexFromControlId(id);
    const int slot = ShortcutSlotFromControlId(id);
    if (action < 0 || slot < 0) return;

    // Keep each shortcut unambiguous. Assigning an existing shortcut moves it
    // to the newly selected action/slot instead of creating a hidden conflict.
    for (size_t a = 0; a < kShortcutActionCount; ++a) {
        for (size_t s = 0; s < kShortcutSlotCount; ++s) {
            if (static_cast<int>(a) == action && static_cast<int>(s) == slot) {
                continue;
            }
            if (g_shortcutDialogBindings[a][s].vk != 0 &&
                SameKeyBinding(g_shortcutDialogBindings[a][s], binding)) {
                g_shortcutDialogBindings[a][s] = {};
                UpdateShortcutEdit(GetParent(edit), a, s);
            }
        }
    }

    g_shortcutDialogBindings[static_cast<size_t>(action)]
                            [static_cast<size_t>(slot)] = binding;
    UpdateShortcutEdit(GetParent(edit),
                       static_cast<size_t>(action),
                       static_cast<size_t>(slot));
}

LRESULT CALLBACK ShortcutEditProc(HWND hwnd, UINT msg,
                                  WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SETFOCUS:
        g_lastShortcutEdit = hwnd;
        break;

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTARROWS;

    case WM_APPCOMMAND: {
        const UINT vk = VirtualKeyFromAppCommand(GET_APPCOMMAND_LPARAM(lParam));
        if (vk != 0) {
            AssignShortcutDialogBinding(hwnd, MakeKeyBinding(vk));
            return TRUE;
        }
        break;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const UINT vk = static_cast<UINT>(wParam);
        if (vk == VK_TAB) {
            HWND dialog = GetParent(hwnd);
            const bool backwards = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            HWND next = GetNextDlgTabItem(dialog, hwnd, backwards);
            if (next) SetFocus(next);
            return 0;
        }
        if (IsModifierVirtualKey(vk)) return 0;
        if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
            (GetKeyState(VK_RWIN) & 0x8000) != 0) {
            return 0;
        }

        KeyBinding binding{};
        binding.vk = vk;
        binding.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        binding.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        binding.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        AssignShortcutDialogBinding(hwnd, binding);
        return 0;
    }

    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_PASTE:
    case WM_CUT:
    case WM_CLEAR:
        return 0;
    }

    return CallWindowProcW(g_shortcutEditOriginalProc, hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK ShortcutsProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        ApplyDarkTheme(dialog);
        g_shortcutDialogBindings = g_shortcuts;
        g_lastShortcutEdit = nullptr;

        for (size_t action = 0; action < kShortcutActionCount; ++action) {
            const int primaryId =
                IDC_SHORTCUT_PRIMARY_BASE + static_cast<int>(action);
            const int alternateId =
                IDC_SHORTCUT_ALT_BASE + static_cast<int>(action);

            HWND primary = GetDlgItem(dialog, primaryId);
            HWND alternate = GetDlgItem(dialog, alternateId);

            for (HWND edit : { primary, alternate }) {
                if (!edit) continue;
                ApplyDarkTheme(edit);
                const WNDPROC previous = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrW(edit, GWLP_WNDPROC,
                                     reinterpret_cast<LONG_PTR>(ShortcutEditProc)));
                if (!g_shortcutEditOriginalProc && previous) {
                    g_shortcutEditOriginalProc = previous;
                }
            }
        }

        for (int id : { IDC_SHORTCUT_CLEAR, IDC_SHORTCUT_RESET, IDOK, IDCANCEL }) {
            HWND child = GetDlgItem(dialog, id);
            if (child) ApplyDarkTheme(child);
        }

        UpdateAllShortcutEdits(dialog);

        RECT owner{}, dlg{};
        GetWindowRect(g_main, &owner);
        GetWindowRect(dialog, &dlg);
        const int x = owner.left +
            ((owner.right - owner.left) - (dlg.right - dlg.left)) / 2;
        const int y = owner.top +
            ((owner.bottom - owner.top) - (dlg.bottom - dlg.top)) / 2;
        SetWindowPos(dialog, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

        // Start on a neutral button rather than a capture field. That keeps
        // ordinary Enter/Esc dialog behavior until the user deliberately
        // clicks a shortcut box.
        HWND okButton = GetDlgItem(dialog, IDOK);
        if (okButton) SetFocus(okButton);
        return FALSE;
    }

    case WM_APPCOMMAND: {
        const UINT vk = VirtualKeyFromAppCommand(GET_APPCOMMAND_LPARAM(lParam));
        if (vk != 0) {
            HWND focus = GetFocus();
            if (focus && GetParent(focus) == dialog &&
                ShortcutActionIndexFromControlId(GetDlgCtrlID(focus)) >= 0) {
                AssignShortcutDialogBinding(focus, MakeKeyBinding(vk));
                return TRUE;
            }
        }
        break;
    }

    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(g_panelBrush);

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_PANEL);
        return reinterpret_cast<INT_PTR>(g_panelBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, RGB(28, 28, 28));
        return reinterpret_cast<INT_PTR>(g_editBrush ? g_editBrush : g_panelBrush);
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SHORTCUT_CLEAR: {
            HWND edit = g_lastShortcutEdit;
            if (edit && IsWindow(edit) && GetParent(edit) == dialog) {
                const int id = GetDlgCtrlID(edit);
                const int action = ShortcutActionIndexFromControlId(id);
                const int slot = ShortcutSlotFromControlId(id);
                if (action >= 0 && slot >= 0) {
                    g_shortcutDialogBindings[static_cast<size_t>(action)]
                                            [static_cast<size_t>(slot)] = {};
                    UpdateShortcutEdit(dialog,
                                       static_cast<size_t>(action),
                                       static_cast<size_t>(slot));
                    SetFocus(edit);
                }
            }
            return TRUE;
        }

        case IDC_SHORTCUT_RESET:
            g_shortcutDialogBindings = DefaultShortcutBindings();
            UpdateAllShortcutEdits(dialog);
            return TRUE;

        case IDOK:
            g_shortcuts = g_shortcutDialogBindings;
            SaveShortcutSettings();
            RefreshMenuShortcutLabels();
            g_lastShortcutEdit = nullptr;
            EndDialog(dialog, IDOK);
            return TRUE;

        case IDCANCEL:
            g_lastShortcutEdit = nullptr;
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}


INT_PTR CALLBACK OptionsProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        ApplyDarkTheme(dialog);

        const int ids[] = {
            IDC_OPT_DELAY, IDC_OPT_OSD_SIZE, IDC_OPT_OSD, IDC_OPT_PLAYLIST_START_OSD,
            IDC_OPT_GPU_API, IDC_OPT_SUB_SIZE, IDC_OPT_AUDIO_LANG, IDC_OPT_SUB_LANG,
            IDC_OPT_EXIT_FULLSCREEN_END, IDC_OPT_SHORTCUTS, IDOK, IDCANCEL
        };
        for (int id : ids) {
            HWND child = GetDlgItem(dialog, id);
            if (child) ApplyDarkTheme(child);
        }

        SetDlgItemInt(dialog, IDC_OPT_DELAY, g_singleClickDelayMs, FALSE);
        SetDlgItemInt(dialog, IDC_OPT_OSD_SIZE, g_osdFontSize, FALSE);
        SetDlgItemInt(dialog, IDC_OPT_SUB_SIZE, g_subtitleFontSize, FALSE);

        HWND combo = GetDlgItem(dialog, IDC_OPT_OSD);
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Center - golden ratio"));
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Top left"));
        SendMessageW(combo, CB_SETCURSEL,
                     g_osdPosition == OsdPosition::TopLeft ? 1 : 0, 0);
        SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 20);
        SendMessageW(combo, CB_SETITEMHEIGHT, 0, 20);

        HWND playlistStartCombo = GetDlgItem(dialog, IDC_OPT_PLAYLIST_START_OSD);
        SendMessageW(playlistStartCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Nothing"));
        SendMessageW(playlistStartCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Show title"));
        SendMessageW(playlistStartCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Show playlist"));
        SendMessageW(playlistStartCombo, CB_SETCURSEL,
                     static_cast<WPARAM>(g_playlistStartOsd), 0);
        SendMessageW(playlistStartCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 20);
        SendMessageW(playlistStartCombo, CB_SETITEMHEIGHT, 0, 20);

        HWND gpuApiCombo = GetDlgItem(dialog, IDC_OPT_GPU_API);
        SendMessageW(gpuApiCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Auto (recommended)"));
        SendMessageW(gpuApiCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Direct3D 11"));
        SendMessageW(gpuApiCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"Vulkan"));
        SendMessageW(gpuApiCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"OpenGL"));
        SendMessageW(gpuApiCombo, CB_SETCURSEL,
                     static_cast<WPARAM>(g_gpuApi), 0);
        SendMessageW(gpuApiCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 20);
        SendMessageW(gpuApiCombo, CB_SETITEMHEIGHT, 0, 20);

        SetDlgItemTextW(dialog, IDC_OPT_AUDIO_LANG,
                        g_preferredAudioLanguage.c_str());
        SetDlgItemTextW(dialog, IDC_OPT_SUB_LANG,
                        g_preferredSubtitleLanguage.c_str());
        CheckDlgButton(dialog, IDC_OPT_EXIT_FULLSCREEN_END,
                       g_exitFullscreenOnPlaybackEnd ? BST_CHECKED : BST_UNCHECKED);

        RECT owner{}, dlg{};
        GetWindowRect(g_main, &owner);
        GetWindowRect(dialog, &dlg);
        const int x = owner.left +
            ((owner.right - owner.left) - (dlg.right - dlg.left)) / 2;
        const int y = owner.top +
            ((owner.bottom - owner.top) - (dlg.bottom - dlg.top)) / 2;
        SetWindowPos(dialog, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(g_panelBrush);

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_PANEL);
        return reinterpret_cast<INT_PTR>(g_panelBrush);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, RGB(28, 28, 28));
        return reinterpret_cast<INT_PTR>(g_editBrush ? g_editBrush : g_panelBrush);
    }

    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure && (measure->CtlID == IDC_OPT_OSD ||
                        measure->CtlID == IDC_OPT_PLAYLIST_START_OSD ||
                        measure->CtlID == IDC_OPT_GPU_API)) {
            measure->itemHeight = 20;
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw && (draw->CtlID == IDC_OPT_OSD ||
                     draw->CtlID == IDC_OPT_PLAYLIST_START_OSD ||
                     draw->CtlID == IDC_OPT_GPU_API)) {
            const bool selected = (draw->itemState & ODS_SELECTED) != 0;
            const COLORREF background = selected ? RGB(48, 48, 48) : RGB(28, 28, 28);

            HBRUSH brush = CreateSolidBrush(background);
            FillRect(draw->hDC, &draw->rcItem, brush);
            DeleteObject(brush);

            wchar_t text[128]{};
            HWND combo = GetDlgItem(dialog, static_cast<int>(draw->CtlID));
            int item = static_cast<int>(draw->itemID);
            if (item < 0) {
                item = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
            }
            if (item >= 0) {
                SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(item),
                             reinterpret_cast<LPARAM>(text));
            }

            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, C_TEXT);

            HFONT oldFont = nullptr;
            if (g_font) {
                oldFont = static_cast<HFONT>(
                    SelectObject(draw->hDC, g_font));
            }

            RECT textRect = draw->rcItem;
            textRect.left += 7;
            textRect.right -= 4;
            DrawTextW(draw->hDC, text, -1, &textRect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            if (oldFont) {
                SelectObject(draw->hDC, oldFont);
            }

            if ((draw->itemState & ODS_FOCUS) != 0) {
                RECT focus = draw->rcItem;
                InflateRect(&focus, -1, -1);
                DrawFocusRect(draw->hDC, &focus);
            }
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OPT_SHORTCUTS:
            DialogBoxParamW(g_instance, MAKEINTRESOURCEW(203), dialog,
                            ShortcutsProc, 0);
            return TRUE;

        case IDOK: {
            BOOL ok = FALSE;
            const UINT delay = GetDlgItemInt(dialog, IDC_OPT_DELAY, &ok, FALSE);
            if (ok) {
                g_singleClickDelayMs =
                    static_cast<UINT>(std::clamp<int>(static_cast<int>(delay), 20, 500));
            }

            BOOL sizeOk = FALSE;
            const UINT osdSize = GetDlgItemInt(dialog, IDC_OPT_OSD_SIZE, &sizeOk, FALSE);
            if (sizeOk) {
                g_osdFontSize = std::clamp<int>(static_cast<int>(osdSize), 24, 180);
            }

            BOOL subtitleSizeOk = FALSE;
            const UINT subtitleSize =
                GetDlgItemInt(dialog, IDC_OPT_SUB_SIZE, &subtitleSizeOk, FALSE);
            if (subtitleSizeOk) {
                g_subtitleFontSize =
                    std::clamp<int>(static_cast<int>(subtitleSize), 20, 120);
            }

            HWND combo = GetDlgItem(dialog, IDC_OPT_OSD);
            const int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
            g_osdPosition = sel == 1 ? OsdPosition::TopLeft : OsdPosition::GoldenCenter;

            HWND playlistStartCombo = GetDlgItem(dialog, IDC_OPT_PLAYLIST_START_OSD);
            const int playlistStartSel = static_cast<int>(
                SendMessageW(playlistStartCombo, CB_GETCURSEL, 0, 0));
            g_playlistStartOsd = static_cast<PlaylistStartOsd>(
                std::clamp(playlistStartSel, 0, 2));

            HWND gpuApiCombo = GetDlgItem(dialog, IDC_OPT_GPU_API);
            const int gpuApiSel = static_cast<int>(
                SendMessageW(gpuApiCombo, CB_GETCURSEL, 0, 0));
            g_gpuApi = static_cast<GpuApi>(std::clamp(gpuApiSel, 0, 3));

            wchar_t audio[128]{};
            wchar_t sub[128]{};
            GetDlgItemTextW(dialog, IDC_OPT_AUDIO_LANG, audio,
                            static_cast<int>(std::size(audio)));
            GetDlgItemTextW(dialog, IDC_OPT_SUB_LANG, sub,
                            static_cast<int>(std::size(sub)));

            g_preferredAudioLanguage = audio;
            g_preferredSubtitleLanguage = sub;
            g_exitFullscreenOnPlaybackEnd =
                IsDlgButtonChecked(dialog, IDC_OPT_EXIT_FULLSCREEN_END) == BST_CHECKED;

            if (g_mpvReady) {
                if (!g_preferredAudioLanguage.empty()) {
                    MpvSetString("alang", g_preferredAudioLanguage);
                }
                if (!g_preferredSubtitleLanguage.empty()) {
                    MpvSetString("slang", g_preferredSubtitleLanguage);
                }
                MpvCommand({"set", "osd-font-size", std::to_string(g_osdFontSize)});
                MpvCommand({"set", "sub-font-size", std::to_string(g_subtitleFontSize)});
                if (g_playlistOsdVisible) ShowPlaylistOsd();
                else UpdateOsdPosition();
            }

            SaveSettings();
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}


INT_PTR CALLBACK AboutProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        ApplyDarkTheme(dialog);

        HWND ok = GetDlgItem(dialog, IDOK);
        if (ok) ApplyDarkTheme(ok);
        HWND github = GetDlgItem(dialog, IDC_ABOUT_GITHUB);
        if (github) ApplyDarkTheme(github);

        RECT owner{}, dlg{};
        GetWindowRect(g_main, &owner);
        GetWindowRect(dialog, &dlg);
        const int x = owner.left +
            ((owner.right - owner.left) - (dlg.right - dlg.left)) / 2;
        const int y = owner.top +
            ((owner.bottom - owner.top) - (dlg.bottom - dlg.top)) / 2;
        SetWindowPos(dialog, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(g_panelBrush);

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT);
        return reinterpret_cast<INT_PTR>(g_panelBrush);
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ABOUT_GITHUB) {
            ShellExecuteW(dialog, L"open", kProjectUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void HandleCommand(int id) {
    switch (id) {
    case IDC_PLAY:
    case IDM_PLAY_TOGGLE:
        TogglePlay(); break;
    case IDC_STOP:
    case IDM_PLAY_STOP:
        StopPlayback(); break;
    case IDC_PREV:
    case IDM_PLAY_PREV:
        ChapterStep(false); break;
    case IDC_SEEK_BACK:
    case IDM_PLAY_BACK:
        SeekBy(-kSeekStepSeconds); break;
    case IDC_SEEK_FORWARD:
    case IDM_PLAY_FORWARD:
        SeekBy(kSeekStepSeconds); break;
    case IDC_NEXT:
    case IDM_PLAY_NEXT:
        ChapterStep(true); break;
    case IDC_SPEED_DOWN:
    case IDM_SPEED_DOWN:
        AdjustSpeed(-0.1); break;
    case IDC_SPEED_UP:
    case IDM_SPEED_UP:
        AdjustSpeed(0.1); break;
    case IDC_AUTO_NEXT:
    case IDM_AUTO_NEXT_FILE:
        ToggleAutoPlayNextFile(); break;
    case IDC_PLAYLIST_OSD:
        TogglePlaylistOsd(); break;
    case IDM_PLAYLIST_SHOW:
        TogglePlaylistOsd(); break;
    case IDC_PLAYLIST_MENU:
        ShowPlaylistPopup(g_playlistMenu); break;
    case IDM_PLAYLIST_PREV:
        PlaylistStep(false); break;
    case IDM_PLAYLIST_NEXT:
        PlaylistStep(true); break;
    case IDC_AUDIO:
    case IDM_AUDIO_TRACKS:
        ShowTrackMenu(g_audio, true); break;
    case IDC_SUBS:
    case IDM_SUB_TRACKS:
        ShowTrackMenu(g_subs, false); break;
    case IDC_MUTE:
    case IDM_MUTE:
        ToggleMute(); break;
    case IDM_FILE_OPEN:
        OpenFileDialog(); break;
    case IDM_VIEW_FULLSCREEN:
        ToggleFullscreen(); break;
    case IDM_VIEW_MEDIA_INFO:
        ToggleMediaInfo(); break;
    case IDM_VIEW_OPTIONS:
        DialogBoxParamW(g_instance, MAKEINTRESOURCEW(201), g_main,
                        OptionsProc, 0);
        break;
    case IDM_VIEW_OPEN_MPV_CONFIG:
        OpenMpvConfig();
        break;
    case IDM_FILE_EXIT:
        DestroyWindow(g_main); break;
    case IDM_HELP_ABOUT:
        DialogBoxParamW(g_instance, MAKEINTRESOURCEW(202), g_main,
                        AboutProc, 0);
        break;
    }
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        ApplyDarkTheme(hwnd);
        CreateUi(hwnd);
        DragAcceptFiles(hwnd, TRUE);
        return 0;
    case WM_SIZE:
        LayoutChildren(hwnd);
        if (g_playlistOsdVisible) RenderPlaylistOverlay();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 650;
        info->ptMinTrackSize.y = 400;
        return 0;
    }
    case WM_APPCOMMAND:
        if (HandleMediaAppCommand(lParam)) return TRUE;
        break;
    case WM_COMMAND:
        HandleCommand(LOWORD(wParam));
        return 0;
    case WM_APP_AUTO_NEXT_FILE:
        PlayPendingAutoNextFile();
        return 0;
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wParam);
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        paths.reserve(count);

        for (UINT i = 0; i < count; ++i) {
            const UINT length = DragQueryFileW(drop, i, nullptr, 0);
            if (length == 0) continue;
            std::vector<wchar_t> path(static_cast<size_t>(length) + 1, L'\0');
            if (DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size()))) {
                paths.emplace_back(path.data());
            }
        }

        DragFinish(drop);
        LoadPlaylist(paths);
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_SINGLE_CLICK) {
            KillTimer(hwnd, TIMER_SINGLE_CLICK);
            if (g_pendingSingleClick) {
                g_pendingSingleClick = false;
                TogglePlay();
                g_singleClickApplied = true;
            }
            return 0;
        }
        if (wParam == TIMER_FULLSCREEN_HOVER) {
            CheckFullscreenControlsHover();
            return 0;
        }
        if (wParam == TIMER_MPV_POLL) {
            PollMpv();
            return 0;
        }
        if (wParam == TIMER_PLAYLIST_OSD_RESTORE) {
            KillTimer(hwnd, TIMER_PLAYLIST_OSD_RESTORE);
            if (g_playlistOsdVisible) RenderPlaylistOverlay();
            else RemovePlaylistOverlay();
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        SaveCurrentResumePosition();
        FlushResumePositions();
        SaveSettings();
        KillTimer(hwnd, TIMER_SINGLE_CLICK);
        KillTimer(hwnd, TIMER_FULLSCREEN_HOVER);
        KillTimer(hwnd, TIMER_MPV_POLL);
        KillTimer(hwnd, TIMER_PLAYLIST_OSD_RESTORE);
        ShutdownMpv();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterClasses() {
    HICON icon = LoadIconW(g_instance, MAKEINTRESOURCEW(101));
    if (!icon) icon = LoadIconW(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kMainClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = VideoProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kVideoClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MenuProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush;
    wc.lpszClassName = kMenuClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PanelProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush;
    wc.lpszClassName = kPanelClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SeekProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = g_panelBrush;
    wc.lpszClassName = kSeekClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ButtonProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush;
    wc.lpszClassName = kButtonClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kOverlayClass;
    if (!RegisterClassExW(&wc)) return false;

    wc = WNDCLASSEXW{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = VolumeProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = g_panelBrush;
    wc.lpszClassName = kVolumeClass;
    return RegisterClassExW(&wc) != 0;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    LoadSettings();
    LoadResumePositions();
    g_instance = instance;
    EnableDarkModeForApp();
    g_panelBrush = CreateSolidBrush(C_PANEL);
    g_editBrush = CreateSolidBrush(RGB(28, 28, 28));
    g_seekBgBrush = CreateSolidBrush(C_SEEK_BG);
    g_seekFillBrush = CreateSolidBrush(C_SEEK_FILL);
    g_chapterMarkerBrush = CreateSolidBrush(C_CHAPTER_MARKER);
    g_seekPlayheadBrush = CreateSolidBrush(C_TEAL);

    g_font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_smallFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    if (!RegisterClasses()) {
        MessageBoxW(nullptr, L"Could not register the native player window classes.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    BuildMenus();

    const int initialX = g_haveSavedWindowRect
        ? static_cast<int>(g_savedWindowRect.left) : CW_USEDEFAULT;
    const int initialY = g_haveSavedWindowRect
        ? static_cast<int>(g_savedWindowRect.top) : CW_USEDEFAULT;
    const int initialW = g_haveSavedWindowRect
        ? static_cast<int>(g_savedWindowRect.right - g_savedWindowRect.left) : 1060;
    const int initialH = g_haveSavedWindowRect
        ? static_cast<int>(g_savedWindowRect.bottom - g_savedWindowRect.top) : 660;

    g_main = CreateWindowExW(0, kMainClass, kAppTitle,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             initialX, initialY, initialW, initialH,
                             nullptr, nullptr, instance, nullptr);
    if (!g_main) {
        MessageBoxW(nullptr, L"Could not create the main player window.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    ApplyDarkTheme(g_main);
    LoadStateIcons();
    UpdateWindowStateIcon(false);
    const int initialShow = g_startMaximized ? SW_SHOWMAXIMIZED : showCommand;
    ShowWindow(g_main, initialShow);
    UpdateWindow(g_main);

    // Show the real player window before the potentially slow first libmpv/DLL
    // initialization. Freshly copied binaries may be inspected by Windows or
    // security software on first launch; this makes it clear that the app has
    // started instead of appearing unresponsive.
    g_initializingPlaybackEngine = true;
    if (g_status) SetWindowTextW(g_status, L"Initializing playback engine...");
    if (g_video) {
        InvalidateRect(g_video, nullptr, TRUE);
        UpdateWindow(g_video);
    }
    const bool mpvInitialized = InitMpv();
    g_initializingPlaybackEngine = false;
    if (g_video) InvalidateRect(g_video, nullptr, TRUE);
    if (mpvInitialized) SetTimer(g_main, TIMER_MPV_POLL, 250, nullptr);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        std::vector<std::wstring> paths;
        paths.reserve(static_cast<size_t>(argc - 1));
        for (int i = 1; i < argc; ++i) {
            if (argv[i] && *argv[i]) paths.emplace_back(argv[i]);
        }
        LoadPlaylist(paths);
    }
    if (argv) LocalFree(argv);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_MOUSEWHEEL && GetForegroundWindow() == g_main) {
            AdjustVolumeFromWheel(msg.wParam);
            continue;
        }
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) && GetForegroundWindow() == g_main) {
            if (HandleShortcut(msg.wParam)) continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_fileMenu) DestroyMenu(g_fileMenu);
    if (g_viewMenu) DestroyMenu(g_viewMenu);
    if (g_playMenu) DestroyMenu(g_playMenu);
    if (g_helpMenu) DestroyMenu(g_helpMenu);
    if (g_font) DeleteObject(g_font);
    if (g_smallFont) DeleteObject(g_smallFont);
    if (g_panelBrush) DeleteObject(g_panelBrush);
    if (g_editBrush) DeleteObject(g_editBrush);
    if (g_seekBgBrush) DeleteObject(g_seekBgBrush);
    if (g_seekFillBrush) DeleteObject(g_seekFillBrush);
    if (g_chapterMarkerBrush) DeleteObject(g_chapterMarkerBrush);
    if (g_seekPlayheadBrush) DeleteObject(g_seekPlayheadBrush);
    return static_cast<int>(msg.wParam);
}
