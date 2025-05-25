#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <time.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <dbghelp.h>
#include <psapi.h>

// Configuration options
struct Config {
    bool logToFile;
    bool logToConsole;
    bool showTimestamps;
    bool captureOutputDebugString;
    bool captureETW;
    bool captureD3D;
    bool filterMessages;
    std::string logFilePath;
    std::string filterPattern;
} config;

// Original DLL functions we'll forward to
static HMODULE originalDLL = NULL;

// Function pointer types for XInput functions
typedef DWORD (WINAPI *XInputGetStateFunc)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD (WINAPI *XInputSetStateFunc)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
typedef DWORD (WINAPI *XInputGetCapabilitiesFunc)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
typedef DWORD (WINAPI *XInputGetBatteryInformationFunc)(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation);
typedef DWORD (WINAPI *XInputGetKeystrokeFunc)(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke);

// Function pointers to original functions
XInputGetStateFunc OriginalXInputGetState = NULL;
XInputSetStateFunc OriginalXInputSetState = NULL;
XInputGetCapabilitiesFunc OriginalXInputGetCapabilities = NULL;
XInputGetBatteryInformationFunc OriginalXInputGetBatteryInformation = NULL;
XInputGetKeystrokeFunc OriginalXInputGetKeystroke = NULL;

// Console and file for debug output
FILE* consoleOutput = NULL;
std::ofstream fileOutput;

// Hotkey handling
BOOL isLoggingEnabled = TRUE;

// Process information
char processName[MAX_PATH] = "Unknown";
DWORD processId = 0;

// Debug message buffer
std::vector<std::string> messageBuffer;
const size_t MAX_BUFFER_SIZE = 1000;

// Forward declarations
void WriteLog(const char* format, ...);
void ToggleLogging();
void ShowProcessInfo();
void CaptureDebugMessages();

// Original OutputDebugString function
typedef void (WINAPI *OutputDebugStringAFunc)(LPCSTR lpOutputString);
typedef void (WINAPI *OutputDebugStringWFunc)(LPCWSTR lpOutputString);
OutputDebugStringAFunc OriginalOutputDebugStringA = NULL;
OutputDebugStringWFunc OriginalOutputDebugStringW = NULL;

// Get current timestamp
std::string GetTimestamp() {
    if (!config.showTimestamps) return "";
    
    char buffer[26];
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", &timeinfo);
    return std::string(buffer);
}

// Write to log (console and/or file)
void WriteLog(const char* format, ...) {
    if (!isLoggingEnabled) return;
    
    std::string timestamp = GetTimestamp();
    
    va_list args;
    va_start(args, format);
    
    if (config.logToConsole && consoleOutput != NULL) {
        fprintf(consoleOutput, timestamp.c_str());
        vfprintf(consoleOutput, format, args);
        fflush(consoleOutput);
    }
    
    if (config.logToFile && fileOutput.is_open()) {
        char buffer[4096];
        vsprintf_s(buffer, format, args);
        fileOutput << timestamp << buffer;
        fileOutput.flush();
    }
    
    va_end(args);
}

// Toggle logging on/off
void ToggleLogging() {
    isLoggingEnabled = !isLoggingEnabled;
    if (isLoggingEnabled) {
        WriteLog("Logging enabled\n");
    }
}

// Show process information
void ShowProcessInfo() {
    WriteLog("\n=== Process Information ===\n");
    WriteLog("Process Name: %s\n", processName);
    WriteLog("Process ID: %d\n", processId);
    
    // Get module information
    HMODULE hMods[1024];
    DWORD cbNeeded;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    
    if (hProcess) {
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            WriteLog("Loaded Modules:\n");
            for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)) && i < 20; i++) {
                char modName[MAX_PATH];
                if (GetModuleFileNameExA(hProcess, hMods[i], modName, sizeof(modName))) {
                    WriteLog("  %s\n", modName);
                }
            }
            
            if (cbNeeded / sizeof(HMODULE) > 20) {
                WriteLog("  ... and %d more modules\n", (cbNeeded / sizeof(HMODULE)) - 20);
            }
        }
        CloseHandle(hProcess);
    }
    
    WriteLog("===========================\n\n");
}

// Our implementation of OutputDebugStringA
void WINAPI HookedOutputDebugStringA(LPCSTR lpOutputString) {
    if (config.captureOutputDebugString) {
        // Check if we should filter this message
        bool shouldLog = true;
        if (config.filterMessages && !config.filterPattern.empty()) {
            shouldLog = (strstr(lpOutputString, config.filterPattern.c_str()) != NULL);
        }
        
        if (shouldLog) {
            WriteLog("[DEBUG] %s", lpOutputString);
            
            // Add to buffer
            if (messageBuffer.size() >= MAX_BUFFER_SIZE) {
                messageBuffer.erase(messageBuffer.begin());
            }
            messageBuffer.push_back(lpOutputString);
        }
    }
    
    // Call original function
    if (OriginalOutputDebugStringA) {
        OriginalOutputDebugStringA(lpOutputString);
    }
}

// Our implementation of OutputDebugStringW
void WINAPI HookedOutputDebugStringW(LPCWSTR lpOutputString) {
    if (config.captureOutputDebugString) {
        // Convert wide string to narrow for logging
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, lpOutputString, -1, NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, lpOutputString, -1, &strTo[0], size_needed, NULL, NULL);
        
        // Check if we should filter this message
        bool shouldLog = true;
        if (config.filterMessages && !config.filterPattern.empty()) {
            shouldLog = (strTo.find(config.filterPattern) != std::string::npos);
        }
        
        if (shouldLog) {
            WriteLog("[DEBUG-W] %s", strTo.c_str());
            
            // Add to buffer
            if (messageBuffer.size() >= MAX_BUFFER_SIZE) {
                messageBuffer.erase(messageBuffer.begin());
            }
            messageBuffer.push_back(strTo);
        }
    }
    
    // Call original function
    if (OriginalOutputDebugStringW) {
        OriginalOutputDebugStringW(lpOutputString);
    }
}

// Check for hotkeys
void CheckHotkeys() {
    // Check if Alt+L is pressed to toggle logging
    if (GetAsyncKeyState(VK_MENU) & 0x8000 && GetAsyncKeyState('L') & 0x8000) {
        static bool keyPressed = false;
        if (!keyPressed) {
            ToggleLogging();
            keyPressed = true;
        }
    } else {
        static bool keyPressed = false;
        keyPressed = false;
    }
    
    // Check if Alt+I is pressed to show process info
    if (GetAsyncKeyState(VK_MENU) & 0x8000 && GetAsyncKeyState('I') & 0x8000) {
        static bool keyPressed = false;
        if (!keyPressed) {
            ShowProcessInfo();
            keyPressed = true;
        }
    } else {
        static bool keyPressed = false;
        keyPressed = false;
    }
}

// Our implementation of XInputGetState - used as a hook point
DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
    if (OriginalXInputGetState == NULL) {
        WriteLog("Error: Original XInputGetState function not found\n");
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    
    // Call the original function
    DWORD result = OriginalXInputGetState(dwUserIndex, pState);
    
    // Check for hotkeys
    CheckHotkeys();
    
    return result;
}

// Minimal implementations of other XInput functions
DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
    if (OriginalXInputSetState == NULL) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return OriginalXInputSetState(dwUserIndex, pVibration);
}

DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) {
    if (OriginalXInputGetCapabilities == NULL) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return OriginalXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
}

DWORD WINAPI XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation) {
    if (OriginalXInputGetBatteryInformation == NULL) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return OriginalXInputGetBatteryInformation(dwUserIndex, devType, pBatteryInformation);
}

DWORD WINAPI XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke) {
    if (OriginalXInputGetKeystroke == NULL) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return OriginalXInputGetKeystroke(dwUserIndex, dwReserved, pKeystroke);
}

// Hook Windows debug functions
void HookDebugFunctions() {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32) {
        // Get original function addresses
        OriginalOutputDebugStringA = (OutputDebugStringAFunc)GetProcAddress(kernel32, "OutputDebugStringA");
        OriginalOutputDebugStringW = (OutputDebugStringWFunc)GetProcAddress(kernel32, "OutputDebugStringW");
        
        // Hook functions by patching IAT
        if (OriginalOutputDebugStringA) {
            DWORD oldProtect;
            FARPROC* pIAT = (FARPROC*)GetProcAddress(kernel32, "OutputDebugStringA");
            VirtualProtect(pIAT, sizeof(FARPROC), PAGE_READWRITE, &oldProtect);
            *pIAT = (FARPROC)HookedOutputDebugStringA;
            VirtualProtect(pIAT, sizeof(FARPROC), oldProtect, &oldProtect);
            WriteLog("Hooked OutputDebugStringA\n");
        }
        
        if (OriginalOutputDebugStringW) {
            DWORD oldProtect;
            FARPROC* pIAT = (FARPROC*)GetProcAddress(kernel32, "OutputDebugStringW");
            VirtualProtect(pIAT, sizeof(FARPROC), PAGE_READWRITE, &oldProtect);
            *pIAT = (FARPROC)HookedOutputDebugStringW;
            VirtualProtect(pIAT, sizeof(FARPROC), oldProtect, &oldProtect);
            WriteLog("Hooked OutputDebugStringW\n");
        }
    }
}

// Load configuration
void LoadConfig() {
    // Default configuration
    config.logToFile = true;
    config.logToConsole = true;
    config.showTimestamps = true;
    config.captureOutputDebugString = true;
    config.captureETW = false;  // More complex, not implemented yet
    config.captureD3D = false;  // More complex, not implemented yet
    config.filterMessages = false;
    config.logFilePath = "game_debug_log.txt";
    config.filterPattern = "";
    
    // Try to load from config file
    FILE* configFile = NULL;
    if (fopen_s(&configFile, "xinput_logger.cfg", "r") == 0) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), configFile)) {
            char key[128], value[128];
            if (sscanf_s(buffer, "%[^=]=%s", key, (unsigned)sizeof(key), value, (unsigned)sizeof(value)) == 2) {
                if (strcmp(key, "logToFile") == 0) config.logToFile = (strcmp(value, "true") == 0);
                else if (strcmp(key, "logToConsole") == 0) config.logToConsole = (strcmp(value, "true") == 0);
                else if (strcmp(key, "showTimestamps") == 0) config.showTimestamps = (strcmp(value, "true") == 0);
                else if (strcmp(key, "captureOutputDebugString") == 0) config.captureOutputDebugString = (strcmp(value, "true") == 0);
                else if (strcmp(key, "captureETW") == 0) config.captureETW = (strcmp(value, "true") == 0);
                else if (strcmp(key, "captureD3D") == 0) config.captureD3D = (strcmp(value, "true") == 0);
                else if (strcmp(key, "filterMessages") == 0) config.filterMessages = (strcmp(value, "true") == 0);
                else if (strcmp(key, "logFilePath") == 0) config.logFilePath = value;
                else if (strcmp(key, "filterPattern") == 0) config.filterPattern = value;
            }
        }
        fclose(configFile);
    }
}

// Get process information
void GetProcessInfo() {
    processId = GetCurrentProcessId();
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        if (GetModuleFileNameExA(hProcess, NULL, processName, MAX_PATH) == 0) {
            strcpy_s(processName, "Unknown");
        } else {
            // Extract just the filename from the path
            char* lastSlash = strrchr(processName, '\\');
            if (lastSlash) {
                strcpy_s(processName, lastSlash + 1);
            }
        }
        CloseHandle(hProcess);
    }
}

// Initialize the DLL
BOOL InitializeDLL() {
    // Load configuration
    LoadConfig();
    
    // Get process information
    GetProcessInfo();
    
    // Create console window if needed
    if (config.logToConsole) {
        AllocConsole();
        freopen_s(&consoleOutput, "CONOUT$", "w", stdout);
    }
    
    // Open log file if needed
    if (config.logToFile) {
        fileOutput.open(config.logFilePath);
    }
    
    // Print header
    WriteLog("=== Game Debug Trace Logger Loaded ===\n");
    WriteLog("Injected into process: %s (PID: %d)\n\n", processName, processId);
    WriteLog("Hotkeys:\n");
    WriteLog("  Alt+L: Toggle logging\n");
    WriteLog("  Alt+I: Show process information\n\n");
    
    // Try to load the original DLL from system directory
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, MAX_PATH, "\\xinput1_3.dll");
    
    originalDLL = LoadLibraryA(systemPath);
    if (originalDLL == NULL) {
        WriteLog("Failed to load original xinput1_3.dll from %s\n", systemPath);
        return FALSE;
    }
    
    WriteLog("Loaded original DLL from: %s\n", systemPath);
    
    // Get the original function addresses
    OriginalXInputGetState = (XInputGetStateFunc)GetProcAddress(originalDLL, "XInputGetState");
    OriginalXInputSetState = (XInputSetStateFunc)GetProcAddress(originalDLL, "XInputSetState");
    OriginalXInputGetCapabilities = (XInputGetCapabilitiesFunc)GetProcAddress(originalDLL, "XInputGetCapabilities");
    OriginalXInputGetBatteryInformation = (XInputGetBatteryInformationFunc)GetProcAddress(originalDLL, "XInputGetBatteryInformation");
    OriginalXInputGetKeystroke = (XInputGetKeystrokeFunc)GetProcAddress(originalDLL, "XInputGetKeystroke");
    
    if (OriginalXInputGetState == NULL || OriginalXInputSetState == NULL || OriginalXInputGetCapabilities == NULL) {
        WriteLog("Failed to get addresses of essential XInput functions\n");
        return FALSE;
    }
    
    WriteLog("Successfully hooked XInput functions\n");
    
    // Hook debug functions
    if (config.captureOutputDebugString) {
        HookDebugFunctions();
    }
    
    // Show process information
    ShowProcessInfo();
    
    return TRUE;
}

// Clean up when DLL is unloaded
void CleanupDLL() {
    WriteLog("\n=== Game Debug Trace Logger Unloaded ===\n");
    WriteLog("Captured %zu debug messages\n", messageBuffer.size());
    
    if (originalDLL != NULL) {
        FreeLibrary(originalDLL);
    }
    
    if (fileOutput.is_open()) {
        fileOutput.close();
    }
    
    if (consoleOutput != NULL) {
        fclose(consoleOutput);
        FreeConsole();
    }
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            return InitializeDLL();
        case DLL_PROCESS_DETACH:
            CleanupDLL();
            break;
    }
    return TRUE;
}