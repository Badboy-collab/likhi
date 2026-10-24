#pragma once
#include <windows.h>
#include <cstdint>

namespace likhi {

enum class ModeTarget : uint32_t {
    kBangla = 0,
    kEnglish = 1,
    kBanglaToEnglish = 2
};

struct ModeSharedData {
    uint32_t magic;           // 0x4C494B48 ('LIKH')
    uint32_t mode;            // 0 = Bangla, 1 = English
    uint32_t version;         // increments on every change
};

class ModeIpc {
public:
    static ModeIpc& Instance() {
        static ModeIpc inst;
        return inst;
    }

    ModeTarget GetMode() {
        EnsureMapped();
        if (shared_data_) {
            return static_cast<ModeTarget>(shared_data_->mode);
        }
        return local_mode_;
    }

    void SetMode(ModeTarget mode) {
        EnsureMapped();
        local_mode_ = mode;
        if (shared_data_) {
            shared_data_->mode = static_cast<uint32_t>(mode);
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared_data_->version));
        }
        BroadcastChange();
    }

    void ToggleMode() {
        ModeTarget current = GetMode();
        if (current == ModeTarget::kEnglish) {
            SetMode(ModeTarget::kBangla);
        } else {
            SetMode(ModeTarget::kEnglish);
        }
    }

    void ToggleBanglaToEnglish() {
        ModeTarget current = GetMode();
        if (current == ModeTarget::kBanglaToEnglish) {
            SetMode(ModeTarget::kBangla);
        } else {
            SetMode(ModeTarget::kBanglaToEnglish);
        }
    }

    uint32_t GetVersion() {
        EnsureMapped();
        if (shared_data_) {
            return shared_data_->version;
        }
        return 0;
    }

    static UINT GetBroadcastMessage() {
        static UINT msg = RegisterWindowMessageW(L"LikhiConversionModeChangedMessage");
        return msg;
    }

    static UINT GetShowTopBarMessage() {
        static UINT msg = RegisterWindowMessageW(L"LikhiShowTopBarMessage");
        return msg;
    }

    static UINT GetResetTopBarMessage() {
        static UINT msg = RegisterWindowMessageW(L"LikhiResetTopBarMessage");
        return msg;
    }

    void BroadcastChange() {
        PostMessageW(HWND_BROADCAST, GetBroadcastMessage(), static_cast<WPARAM>(GetMode()), 0);
    }

private:
    ModeIpc() : h_map_(nullptr), shared_data_(nullptr), local_mode_(ModeTarget::kBangla) {
        EnsureMapped();
    }

    ~ModeIpc() {
        if (shared_data_) {
            UnmapViewOfFile(shared_data_);
            shared_data_ = nullptr;
        }
        if (h_map_) {
            CloseHandle(h_map_);
            h_map_ = nullptr;
        }
    }

    void EnsureMapped() {
        if (shared_data_) return;

        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = &sd;

        h_map_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            &sa,
            PAGE_READWRITE,
            0,
            sizeof(ModeSharedData),
            L"Local\\Likhi_Mode_Shared_Memory_v1"
        );

        if (!h_map_) {
            h_map_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\Likhi_Mode_Shared_Memory_v1");
        }

        if (h_map_) {
            bool is_new = (GetLastError() != ERROR_ALREADY_EXISTS);
            shared_data_ = static_cast<ModeSharedData*>(MapViewOfFile(h_map_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ModeSharedData)));
            if (shared_data_ && is_new) {
                shared_data_->magic = 0x4C494B48;
                shared_data_->mode = 0; // Bangla by default
                shared_data_->version = 1;
            }
        }
    }

    HANDLE h_map_;
    ModeSharedData* shared_data_;
    ModeTarget local_mode_;
};

} // namespace likhi
