#include "utils.hpp"
#include <fstream>
#include <vector>
#include <Windows.h>
#include <ntstatus.h>
#include <winternl.h>

namespace utils {
    bool ReadFileToMemory(const std::string& file_path, std::vector<uint8_t>* out_buffer) {
        std::ifstream file_ifstream(file_path, std::ios::binary);
        if (!file_ifstream.is_open())
            return false;

        file_ifstream.seekg(0, std::ios::end);
        std::streamsize file_size = file_ifstream.tellg();
        file_ifstream.seekg(0, std::ios::beg);

        out_buffer->resize(file_size);
        if (!file_ifstream.read(reinterpret_cast<char*>(out_buffer->data()), file_size)) {
            file_ifstream.close();
            return false;
        }

        file_ifstream.close();
        return true;
    }

    bool CreateFileFromMemory(const std::string& desired_file_path, const char* address, size_t size) {
        std::ofstream file_ofstream(desired_file_path, std::ios_base::out | std::ios_base::binary);
        if (!file_ofstream.is_open())
            return false;

        if (!file_ofstream.write(address, size)) {
            file_ofstream.close();
            return false;
        }

        file_ofstream.close();
        return true;
    }

    uint64_t GetKernelModuleAddress(const std::string& module_name) {
        void* buffer = nullptr;
        ULONG buffer_size = 0;

        NTSTATUS status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);
        if (status != STATUS_INFO_LENGTH_MISMATCH)
            return 0;

        buffer = VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buffer)
            return 0;

        status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemModuleInformation), buffer, buffer_size, &buffer_size);
        if (!NT_SUCCESS(status)) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            return 0;
        }

        PRTL_PROCESS_MODULES modules = reinterpret_cast<PRTL_PROCESS_MODULES>(buffer);
        for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
            UNICODE_STRING moduleName = modules->Modules[i].FullPathName;
            std::wstring moduleNameStr(moduleName.Buffer, moduleName.Length / sizeof(wchar_t));

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::string current_module_name = converter.to_bytes(moduleNameStr);

            size_t pos = current_module_name.find_last_of('\\');
            if (pos != std::string::npos) {
                current_module_name = current_module_name.substr(pos + 1);
            }

            if (_stricmp(current_module_name.c_str(), module_name.c_str()) == 0) {
                uint64_t result = reinterpret_cast<uint64_t>(modules->Modules[i].ImageBase);
                VirtualFree(buffer, 0, MEM_RELEASE);
                return result;
            }
        }

        VirtualFree(buffer, 0, MEM_RELEASE);
        return 0;
    }
} // namespace utils