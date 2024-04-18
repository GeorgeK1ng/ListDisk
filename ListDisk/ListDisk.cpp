#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <winioctl.h>
#include <SetupAPI.h>
#include <InitGuid.h>

#include <fcntl.h>
#include <io.h>

#include <map>
#include <string>

#include <ShlObj.h>

// Define GPT partition type GUIDs
EXTERN_C const GUID PARTITION_MSFT_RESERVED_GUID = { 0xE3C9E316, 0x0B5C, 0x4DB8, { 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE } };
EXTERN_C const GUID PARTITION_BASIC_DATA_GUID = { 0xEBD0A0A2, 0xB9E5, 0x4433, { 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };
EXTERN_C const GUID PARTITION_LDM_DATA_GUID = { 0xA0A2B9E5, 0x4333, 0x87C0, { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7, 0x00, 0x00 } };
EXTERN_C const GUID PARTITION_WINDOWS_RE_GUID = { 0xde94bba4, 0x06d1, 0x4d40, { 0xa1, 0x6a, 0xbf, 0xd5, 0x01, 0x79, 0xd6, 0xac } };
EXTERN_C const GUID PARTITION_BIOS_BOOT_GUID = { 0x21686148, 0x6449, 0x6E6F, { 0x74, 0x6F, 0x44, 0x43, 0x00, 0x00, 0x00, 0x00 } };

// Define additional GPT partition type GUIDs
EXTERN_C const GUID PARTITION_LINUX_DATA_GUID = { 0x0FC63DAF, 0x8483, 0x4772, { 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0xE4, 0x5A } };
EXTERN_C const GUID PARTITION_BDE_GUID = { 0x5808C8AA, 0x7E8F, 0x42E0, { 0x85, 0xD2, 0xE1, 0xE9, 0x04, 0x34, 0xCF, 0xB3 } };
EXTERN_C const GUID PARTITION_ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, { 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };

DEFINE_GUID(PARTITION_STORAGE_SPACES_GUID, 0xE75CAF8F, 0xF680, 0x4CEE, 0xAF, 0xA3, 0xB0, 0x01, 0xE5, 0x6E, 0xFC, 0x2E);
DEFINE_GUID(PARTITION_STORAGE_SPACES_RE_GUID, 0xA19D880F, 0x05FC, 0x4D3B, 0xA0, 0x06, 0x74, 0x3F, 0x37, 0x23, 0x06, 0xFE);

// Define MBR partition type constants
#define PARTITION_FAT12               0x01
#define PARTITION_HIDDEN_FAT12        0x11  // Hidden FAT12
#define PARTITION_FAT16_SMALL         0x04
#define PARTITION_HIDDEN_FAT16_SMALL  0x14  // Hidden FAT16 <32M
#define PARTITION_EXTENDED_CHS        0x05
#define PARTITION_FAT16               0x06
#define PARTITION_HIDDEN_FAT16        0x16  // Hidden FAT16
#define PARTITION_NTFS                0x07  // This ID is also commonly used for exFAT partitions
#define PARTITION_NTFS_HIDDEN         0x17  // Hidden NTFS or Hidden HPFS
#define PARTITION_FAT32_CHS           0x0B
#define PARTITION_HIDDEN_FAT32_CHS    0x1B  // Hidden FAT32 CHS
#define PARTITION_FAT32_LBA           0x0C
#define PARTITION_HIDDEN_FAT32_LBA    0x1C  // Hidden FAT32 LBA
#define PARTITION_FAT16_LBA           0x0E
#define PARTITION_HIDDEN_FAT16_LBA    0x1E  // Hidden FAT16 LBA
#define PARTITION_EXTENDED_LBA        0x0F
#define PARTITION_OS2_BOOT_MANAGER    0x0A  // OS/2 Boot Manager/Extended Partition
#define PARTITION_WINDOWS_RE          0x27  // Windows Recovery Environment

// Additional MBR partition types
#define PARTITION_LINUX               0x83
#define PARTITION_LINUX_SWAP          0x82
#define PARTITION_LINUX_LVM           0x8E
#define PARTITION_EFI_SYSTEM          0xEF  // EFI File System
#define PARTITION_MSFT_RESERVED       0x42
#define PARTITION_LINUX_EXTENDED      0x85
#define PARTITION_NTFS_VOLUME_SET     0x86
#define PARTITION_NTFS_VOLUME_SET_2   0x87
#define PARTITION_LINUX_PLAINTEXT     0x91
#define PARTITION_HIDDEN_LINUX        0x93  // Hidden Linux Native
#define PARTITION_FREEBSD_SLICE       0xA5
#define PARTITION_OPENBSD_SLICE       0xA6
#define PARTITION_NETBSD_SLICE        0xA9
#define PARTITION_NEXTSTEP            0xA7
#define PARTITION_MAC_OS_X            0xAB
#define PARTITION_GPT_PROTECTIVE      0xEE  // GPT Protective

// Additional specific types (optional depending on use case)
#define PARTITION_SOLARIS             0xBE
#define PARTITION_MAC_HFS             0xAF
#define PARTITION_AMIGA               0xDB
#define PARTITION_BSDI                0xEB

#ifndef IOCTL_DISK_RESCAN_DISK
#define IOCTL_DISK_RESCAN_DISK CTL_CODE(IOCTL_DISK_BASE, 0x0007, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

enum class Align {
    Left,
    Right
};

struct PartitionInfo {
    DWORD DiskNumber = 0;            // Physical disk number
    DWORD PartitionNumber = 0;       // Physical partition number
    PARTITION_STYLE PartitionStyle = PARTITION_STYLE_RAW;
    LONGLONG PartitionLength = 0;    // Size
    std::wstring VolumeName;         // Label
    std::wstring FileSystem;
    std::wstring DriveLetter;
    std::wstring MountPoint;
    std::wstring MountPointName;     /* Volume mount point like \ ? \Volume{24379f2b-7a05-44b4-a9c3-6ef5fd068451} */
    std::wstring PartitionType;
    GUID PartitionId = {};           // Added for GPT partition information
    std::wstring GptName;            // For GPT partition name - doesn't contain all defined names
    BOOLEAN BootIndicator = FALSE;   // MBR Boot indicator
};


std::vector<PartitionInfo> GetPartitionInfo() {
    std::vector<PartitionInfo> partitions;
    HANDLE hDrive = INVALID_HANDLE_VALUE; // Initialize hDrive outside the loop
    HANDLE hVolume = INVALID_HANDLE_VALUE; // Initialize hDrive outside the loop
    std::unique_ptr<char[]> buffer;       // Define buffer outside the loop

    int driveIndex = 0;
    while (true) {
        WCHAR driveName[25];
        swprintf_s(driveName, L"\\\\.\\PhysicalDrive%d", driveIndex);

        hDrive = INVALID_HANDLE_VALUE;
        hDrive = CreateFileW(driveName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        if (hDrive == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                break;
            }
            std::cerr << "Failed to open " << driveName << ", Error: " << GetLastError() << std::endl;
            driveIndex++;
            continue;
        }

        DWORD bufferSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 128;
        std::unique_ptr<char[]> buffer(new char[bufferSize]);
        DRIVE_LAYOUT_INFORMATION_EX* driveLayout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.get());
        DWORD bytesReturned = 0;

        BOOL result = DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, driveLayout, bufferSize, &bytesReturned, NULL);
        if (result) {
            for (DWORD p = 0; p < driveLayout->PartitionCount; ++p) {
                PARTITION_INFORMATION_EX& partition = driveLayout->PartitionEntry[p];
                if (partition.PartitionNumber != 0) {
                    PartitionInfo info;
                    info.DiskNumber = driveIndex;
                    info.PartitionNumber = partition.PartitionNumber;
                    info.PartitionStyle = partition.PartitionStyle;
                    info.PartitionLength = partition.PartitionLength.QuadPart;


                    if (partition.PartitionStyle != PARTITION_STYLE_RAW) {

                        if (partition.PartitionStyle == PARTITION_STYLE_GPT) {
                            PARTITION_INFORMATION_GPT& gptPartition = partition.Gpt;
                            info.PartitionId = gptPartition.PartitionId;

                            // Convert GPT name (which is in WCHAR array) to std::wstring
                            wchar_t gptNameArray[36] = {}; // Array size based on Windows API documentation
                            wcscpy_s(gptNameArray, gptPartition.Name);
                            info.GptName = std::wstring(gptNameArray);

                            if (IsEqualGUID(gptPartition.PartitionType, PARTITION_ESP_GUID))
                                info.PartitionType = L"EFI System";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_MSFT_RESERVED_GUID))
                                info.PartitionType = L"Reserved";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_BASIC_DATA_GUID))
                                info.PartitionType = L"Basic Data";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_LDM_DATA_GUID))
                                info.PartitionType = L"LDM Data";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_WINDOWS_RE_GUID))
                                info.PartitionType = L"Windows RE";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_BIOS_BOOT_GUID))
                                info.PartitionType = L"BIOS Boot";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_LINUX_DATA_GUID))
                                info.PartitionType = L"Linux Data";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_BDE_GUID))
                                info.PartitionType = L"BitLocker";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_STORAGE_SPACES_GUID))
                                info.PartitionType = L"Storage Spaces";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_STORAGE_SPACES_RE_GUID))
                                info.PartitionType = L"Storage Spaces Recovery";
                            else if (IsEqualGUID(gptPartition.PartitionType, PARTITION_ESP_GUID))
                                info.PartitionType = L"EFI System Partition";
                            else
                                info.PartitionType = L"Unknown";
                        }
                        else if (partition.PartitionStyle == PARTITION_STYLE_MBR) {
                            switch (partition.Mbr.PartitionType) {
                            case PARTITION_FAT12: info.PartitionType = L"FAT12"; break;
                            case PARTITION_HIDDEN_FAT12: info.PartitionType = L"Hidden FAT12"; break;
                            case PARTITION_FAT16_SMALL: info.PartitionType = L"FAT16 (small)"; break;
                            case PARTITION_HIDDEN_FAT16_SMALL: info.PartitionType = L"Hidden FAT16 (small)"; break;
                            case PARTITION_EXTENDED_CHS: info.PartitionType = L"Extended (CHS)"; break;
                            case PARTITION_FAT16: info.PartitionType = L"FAT16"; break;
                            case PARTITION_HIDDEN_FAT16: info.PartitionType = L"Hidden FAT16"; break;
                            case PARTITION_NTFS: info.PartitionType = L"NTFS"; break;
                            case PARTITION_NTFS_HIDDEN: info.PartitionType = L"NTFS Hidden"; break;
                            case PARTITION_FAT32_CHS: info.PartitionType = L"FAT32 (CHS)"; break;
                            case PARTITION_HIDDEN_FAT32_CHS: info.PartitionType = L"Hidden FAT32 (CHS)"; break;
                            case PARTITION_FAT32_LBA: info.PartitionType = L"FAT32 (LBA)"; break;
                            case PARTITION_HIDDEN_FAT32_LBA: info.PartitionType = L"Hidden FAT32 (LBA)"; break;
                            case PARTITION_FAT16_LBA: info.PartitionType = L"FAT16 (LBA)"; break;
                            case PARTITION_HIDDEN_FAT16_LBA: info.PartitionType = L"Hidden FAT16 (LBA)"; break;
                            case PARTITION_EXTENDED_LBA: info.PartitionType = L"Extended (LBA)"; break;
                            case PARTITION_LINUX: info.PartitionType = L"Linux"; break;
                            case PARTITION_LINUX_SWAP: info.PartitionType = L"Linux Swap"; break;
                            case PARTITION_LINUX_LVM: info.PartitionType = L"Linux LVM"; break;
                            case PARTITION_EFI_SYSTEM: info.PartitionType = L"EFI System"; break;
                            case PARTITION_MSFT_RESERVED: info.PartitionType = L"MSFT Reserved"; break;
                            case PARTITION_LINUX_EXTENDED: info.PartitionType = L"Linux Extended"; break;
                            case PARTITION_NTFS_VOLUME_SET: info.PartitionType = L"NTFS Volume Set"; break;
                            case PARTITION_NTFS_VOLUME_SET_2: info.PartitionType = L"NTFS Volume Set 2"; break;
                            case PARTITION_LINUX_PLAINTEXT: info.PartitionType = L"Linux Plaintext"; break;
                            case PARTITION_HIDDEN_LINUX: info.PartitionType = L"Hidden Linux Native"; break;
                            case PARTITION_FREEBSD_SLICE: info.PartitionType = L"FreeBSD Slice"; break;
                            case PARTITION_OPENBSD_SLICE: info.PartitionType = L"OpenBSD Slice"; break;
                            case PARTITION_NETBSD_SLICE: info.PartitionType = L"NetBSD Slice"; break;
                            case PARTITION_NEXTSTEP: info.PartitionType = L"NeXTSTEP"; break;
                            case PARTITION_MAC_OS_X: info.PartitionType = L"Mac OS X"; break;
                            case PARTITION_GPT_PROTECTIVE: info.PartitionType = L"GPT Protective"; break;
                            case PARTITION_SOLARIS: info.PartitionType = L"Solaris"; break;
                            case PARTITION_MAC_HFS: info.PartitionType = L"Mac HFS"; break;
                            case PARTITION_AMIGA: info.PartitionType = L"Amiga"; break;
                            case PARTITION_BSDI: info.PartitionType = L"BSDI"; break;
                            default: info.PartitionType = L"Unknown"; break;
                            }


                            // Append (Boot) when partition marked as bootable
                            if (partition.Mbr.BootIndicator != 0) {
                                info.PartitionType += L" *";
                                info.BootIndicator = partition.Mbr.BootIndicator;
                            }
                        }

                        // Generate the volume path
                        WCHAR volumePath[MAX_PATH];
                        swprintf_s(volumePath, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%d\\Partition%d", info.DiskNumber, info.PartitionNumber);

                        //std::wcout << volumePath << std::endl;

                        // Get volume information
                        WCHAR volumeName[MAX_PATH] = { 0 };
                        WCHAR fileSystem[MAX_PATH] = { 0 };

                        hVolume = CreateFileW(volumePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

                        if (hVolume != INVALID_HANDLE_VALUE) {
                            WCHAR rootPath[MAX_PATH] = { 0 };
                            if (GetVolumePathNameW(volumePath, rootPath, MAX_PATH)) {
                                info.MountPoint = rootPath;
                            }

                            if (GetVolumeInformationW(rootPath, volumeName, MAX_PATH, NULL, NULL, NULL, fileSystem, MAX_PATH)) {
                                info.VolumeName = std::wstring(volumeName);
                                info.FileSystem = std::wstring(fileSystem);

                            }

                            WCHAR volumeName[MAX_PATH] = { 0 };
                            if (GetVolumeNameForVolumeMountPointW(rootPath, volumeName, MAX_PATH)) {
                                info.MountPointName = volumeName; // VolumeMountPoint

                            //    std::wcout << volumeName << std::endl;

                                // Now, let's get the drive letter associated with this volume.
                                std::vector<WCHAR> buffer(MAX_PATH);
                                DWORD bufferSize = static_cast<DWORD>(buffer.size());

                                if (GetVolumePathNamesForVolumeNameW(volumeName, buffer.data(), bufferSize, &bufferSize)) {
                                    // The 'buffer' variable now contains the drive letter(s) associated with the volume.
                                    info.DriveLetter = buffer.data();

                                }
                            }

                        }

                        CloseHandle(hVolume);
                    }

                    partitions.push_back(info);
                }
            }
        }
        else {
            std::cerr << "IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for " << driveName << " with error " << GetLastError() << std::endl;
        }

        CloseHandle(hDrive);
        driveIndex++;
    }

    return partitions;
}

std::wstring PartitionStyleToWString(PARTITION_STYLE style) {
    switch (style) {
    case PARTITION_STYLE_MBR: return L"MBR";
    case PARTITION_STYLE_GPT: return L"GPT";
    case PARTITION_STYLE_RAW: return L"RAW";
    default: return L"Unknown";
    }
}

std::wstring FormatSize(LONGLONG size) {
    std::wstringstream stream;
    stream << std::right << std::setw(7);
    if (size >= (1LL << 40)) {
        stream << std::fixed << std::setprecision(2) << (size / (double)(1LL << 40)) << L" TB";
    }
    else if (size >= (1LL << 30)) {
        stream << std::fixed << std::setprecision(2) << (size / (double)(1LL << 30)) << L" GB";
    }
    else if (size >= (1LL << 20)) {
        stream << std::fixed << std::setprecision(2) << (size / (double)(1LL << 20)) << L" MB";
    }
    else if (size >= (1LL << 10)) {
        stream << std::fixed << std::setprecision(2) << (size / (double)(1LL << 10)) << L" KB";
    }
    else {
        stream << size << L" Bytes";
    }
    return stream.str();
}


std::wstring FormatLetter(const std::wstring& input) {
    std::wstring formatted = input;

    // Check if the last character is a backslash
    if (!formatted.empty() && formatted.back() == L'\\') {
        // Remove the last character (backslash)
        formatted.pop_back();
    }

    return formatted;
}


bool IsDirectoryEmpty(const wchar_t* path) {
    WIN32_FIND_DATAW findFileData;
    HANDLE findHandle = FindFirstFileW((std::wstring(path) + L"\\*").c_str(), &findFileData);

    if (findHandle == INVALID_HANDLE_VALUE) {
        return true; // Directory is empty or doesn't exist
    }
    else {
        FindClose(findHandle);
        return false; // Directory contains files or subdirectories
    }
}

std::wstring GetMountPointName(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    for (const auto& partition : partitions) {
        if (partition.DiskNumber == diskNumber && partition.PartitionNumber == partitionNumber) {
            return partition.MountPointName;
        }
    }
    return L""; // Return empty string if not found
}

std::wstring GetDriveLetter(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    for (const auto& partition : partitions) {
        if (partition.DiskNumber == diskNumber && partition.PartitionNumber == partitionNumber) {
        //    std::wcout << L"Found Drive Letter: " << partition.DriveLetter << std::endl; // Debug print
            if (!partition.DriveLetter.empty()) {
                return partition.DriveLetter;
            }
        }
    }
    //std::wcerr << L"Drive letter not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
    return L""; // Return empty string if not found
}


PARTITION_STYLE GetPartitionStyle(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    for (const auto& partition : partitions) {
        if (partition.DiskNumber == diskNumber && partition.PartitionNumber == partitionNumber) {
            return partition.PartitionStyle;
        }
    }
    // If not found, throw an exception
    throw std::runtime_error("Partition not found for Disk " + std::to_string(diskNumber) + ", Partition " + std::to_string(partitionNumber));
}



std::wstring FormatColumn(const std::wstring& inputString, size_t length, Align align) {
    // First, ensure the input string does not have trailing null characters
    std::wstring trimmedInput = inputString;
    size_t nullPos = trimmedInput.find(L'\0');
    if (nullPos != std::wstring::npos) {
        trimmedInput.resize(nullPos); // Remove trailing null characters
    }

    // Determine the number of padding characters needed
    size_t paddingLength = length > trimmedInput.length() ? length - trimmedInput.length() : 0;

    // Build the output string with appropriate padding
    std::wstring output;
    if (align == Align::Left) {
        output = trimmedInput + std::wstring(paddingLength, L' '); // Pad on the right
    }
    else {
        output = std::wstring(paddingLength, L' ') + trimmedInput; // Pad on the left
    }

    // Ensure the output is not longer than the specified length
    if (output.length() > length) {
        output.resize(length);
    }

    return output;
}

bool MountPartition(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, const std::wstring& driveLetter) {
    std::wstring volumeGUIDPath = GetMountPointName(partitions, diskNumber, partitionNumber);

    if (volumeGUIDPath.empty()) {
        std::wcerr << L"Volume not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return false;
    }

    // Variable to store the required buffer size
    DWORD requiredBufferSize = 0;

    // Check if the drive letter is already in use
    if (!GetVolumePathNamesForVolumeNameW(driveLetter.c_str(), NULL, 0, &requiredBufferSize)) {
        if (SetVolumeMountPoint(driveLetter.c_str(), volumeGUIDPath.c_str())) {
         //   std::wcout << L"Successfully mounted " << volumeGUIDPath << L" to " << driveLetter << std::endl;
            return true;
        }
        else {
            std::wcerr << L"Failed to mount " << volumeGUIDPath << L" to " << driveLetter << L". Error: " << GetLastError() << std::endl;
            return false;
        }
    }
    else {
        std::wcerr << L"Drive letter " << driveLetter << L" is already in use." << std::endl;
        return false;
    }
}

bool UnMountPartition(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    std::wstring volumeLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    if (volumeLetter.empty()) {
        std::wcerr << L"Drive letter not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return false;
    }

    if (DeleteVolumeMountPointW(volumeLetter.c_str())) {
    //    std::wcout << L"Successfully unmounted " << volumeLetter << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to unmount " << volumeLetter << L". Error: " << GetLastError() << std::endl;
        return false;
    }
}


bool UnMountPartitionByDriveLetter(const wchar_t* driveLetter) {
    std::wstring volumePath = driveLetter;
    if (!volumePath.empty() && volumePath.back() != L'\\') {
        volumePath += L'\\';
    }

    if (DeleteVolumeMountPointW(volumePath.c_str())) {
    //    std::wcout << L"Successfully unmounted drive " << driveLetter << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to unmount drive " << driveLetter << L". Error: " << GetLastError() << std::endl;
        return false;
    }
}


bool UnMountVolume(const std::wstring& volumeMountPoint) {
    HANDLE hVolume = INVALID_HANDLE_VALUE;
    BOOL result = FALSE;
    DWORD bytesReturned = 0;

    // Open the volume
    hVolume = CreateFileW(volumeMountPoint.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolume == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open volume: " << volumeMountPoint << L". Error: " << GetLastError() << std::endl;
        return false;
    }

    // Lock the volume
    result = DeviceIoControl(hVolume, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    if (!result) {
        std::wcerr << L"Failed to lock volume: " << volumeMountPoint << L". Error: " << GetLastError() << std::endl;
        CloseHandle(hVolume);
        return false;
    }

    // Dismount the volume
    result = DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    if (!result) {
        std::wcerr << L"Failed to dismount volume: " << volumeMountPoint << L". Error: " << GetLastError() << std::endl;
        CloseHandle(hVolume);
        return false;
    }

    // Close the handle to the volume
    CloseHandle(hVolume);

    std::wcout << L"Successfully unmounted volume: " << volumeMountPoint << std::endl;
    return true;
}



bool UnmountVolumeIfMounted(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    WCHAR volumePath[MAX_PATH];
    swprintf_s(volumePath, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%d\\Partition%d", diskNumber, partitionNumber);

    //std::wcout << volumePath << std::endl;

    HANDLE hVolume = CreateFileW(volumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolume == INVALID_HANDLE_VALUE) {
        // If the volume is not mounted, or the path is incorrect, we assume it's safe to proceed
        return true;
    }

    // Dismount the volume
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    CloseHandle(hVolume);

    if (!result) {
        std::wcerr << L"Failed to dismount the volume. Error: " << GetLastError() << std::endl;
        return false;
    }

    //std::wcout << L"Volume on Partition " << partitionNumber << L" on Disk " << diskNumber << L" has been dismounted." << std::endl;
    return true;
}


bool MountVolumeIfNotMounted(DWORD diskNumber, DWORD partitionNumber) {
    auto partitions = GetPartitionInfo();
    std::wstring volumeGUIDPath = GetMountPointName(partitions, diskNumber, partitionNumber);

    if (volumeGUIDPath.empty()) {
        std::wcerr << L"Volume GUID Path not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return false;
    }

    // Find the first available drive letter
    WCHAR driveLetterPath[] = L"X:\\";
    bool isDriveLetterAvailable = false;
    for (WCHAR driveLetter = L'C'; driveLetter <= L'Z'; ++driveLetter) {
        driveLetterPath[0] = driveLetter;
        if (GetDriveType(driveLetterPath) == DRIVE_NO_ROOT_DIR) {
            isDriveLetterAvailable = true;
            break;
        }
    }

    if (!isDriveLetterAvailable) {
        std::wcerr << L"No available drive letters." << std::endl;
        return false;
    }

    // Assign the first available drive letter
    if (SetVolumeMountPointW(driveLetterPath, volumeGUIDPath.c_str())) {
        std::wcout << L"Volume on Partition " << partitionNumber << L" on Disk " << diskNumber << L" has been mounted to " << driveLetterPath << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to assign drive letter to the volume. Error: " << GetLastError() << std::endl;
        return false;
    }
}



bool SetLabel(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, const std::wstring& newLabel) {
    std::wstring volumeLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    // Retrieve the drive letter using your existing functions
    std::wstring driveLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    if (driveLetter.empty()) {
        std::wcerr << L"Drive letter not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return false;
    }

    // Add backslash to drive letter
    //driveLetter += L":\\";

    // Set the new label
    if (SetVolumeLabel(driveLetter.c_str(), newLabel.c_str())) {
        std::wcout << L"Label set to '" << newLabel << L"' for " << driveLetter << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to set label for " << driveLetter << L". Error: " << GetLastError() << std::endl;
        return false;
    }
}


bool SetPartitionActiveDirectly(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, BYTE setActiveValue = 0x80) {

    // Proceed only on MBR disk
    if (GetPartitionStyle(partitions, diskNumber, partitionNumber) == PARTITION_STYLE_MBR) {

        UnmountVolumeIfMounted(partitions, diskNumber, partitionNumber);

        // std::wcout << L"Setting partition " << partitionNumber << L" on Disk " << diskNumber << L" as active with value " << static_cast<unsigned>(setActiveValue) << L"..." << std::endl;

        // Open the disk
        std::wstring diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
        HANDLE hDisk = CreateFileW(diskPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Failed to open disk. Error: " << GetLastError() << std::endl;
            return false;
        }

        // Read the MBR
        BYTE mbr[512];
        DWORD bytesRead;
        if (!ReadFile(hDisk, mbr, sizeof(mbr), &bytesRead, NULL) || bytesRead != sizeof(mbr)) {
            std::wcerr << L"Failed to read MBR. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Check and modify the active partition flag in the MBR
        bool found = false;
        for (int i = 0; i < 4; ++i) {
            int offset = 446 + i * 16; // Each partition entry is 16 bytes
            // The partition number is typically not stored in the MBR entry, so we need to count them
            if (partitionNumber == i + 1) { // Adjust if your numbering is different
                found = true;
                mbr[offset] = setActiveValue; // Set or clear the active flag
                break;
            }
        }

        if (!found) {
            std::wcerr << L"Partition not found in MBR or partition number mismatch." << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Write the modified MBR back to the disk
        DWORD bytesWritten;
        SetFilePointer(hDisk, 0, NULL, FILE_BEGIN);
        if (!WriteFile(hDisk, mbr, sizeof(mbr), &bytesWritten, NULL) || bytesWritten != sizeof(mbr)) {
            std::wcerr << L"Failed to write modified MBR. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Flush the file buffers to ensure changes are written to disk
        // if (!FlushFileBuffers(hDisk)) {
        //     std::wcerr << L"Failed to flush disk buffers. Error: " << GetLastError() << std::endl;
        //     CloseHandle(hDisk);
        //     return false;
        // }

        // Refresh the disk properties to ensure the system acknowledges the changes
        if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesWritten, NULL)) {
            std::wcerr << L"Failed to update disk properties. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        CloseHandle(hDisk);

        //SHChangeNotify(SHCNE_DRIVEADD, SHCNF_FLUSH | SHCNF_FLUSHNOWAIT, NULL, NULL);

        // Remount..?
        //MountVolumeIfNotMounted(diskNumber, partitionNumber);

        std::wcout << L"Partition " << partitionNumber << " on Disk " << diskNumber << L" set as active with value 0x" << std::hex << static_cast<unsigned>(setActiveValue) << std::endl;
        return true;
    }
    else {
        std::wcout << L"This command is valid only for partition on MBR disk" << std::endl;
        return false;
    }
}


bool ChangePartitionTypeDirectly(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, BYTE newPartitionType) {

    // Proceed only on MBR disk
    if (GetPartitionStyle(partitions, diskNumber, partitionNumber) == PARTITION_STYLE_MBR) {

        UnmountVolumeIfMounted(partitions, diskNumber, partitionNumber);


        //std::wcout << L"Changing partition type for Partition " << partitionNumber << L" on Disk " << diskNumber << L" to 0x" << std::hex << static_cast<unsigned>(newPartitionType) << L"..." << std::endl;

        // Open the disk
        std::wstring diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
        HANDLE hDisk = CreateFileW(diskPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Failed to open disk. Error: " << GetLastError() << std::endl;
            return false;
        }

        // Read the MBR
        BYTE mbr[512];
        DWORD bytesRead;
        if (!ReadFile(hDisk, mbr, sizeof(mbr), &bytesRead, NULL) || bytesRead != sizeof(mbr)) {
            std::wcerr << L"Failed to read MBR. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Modify the partition type for the specified partition
        bool found = false;
        for (int i = 0; i < 4; ++i) {
            int offset = 446 + i * 16; // Each partition entry is 16 bytes
            // Check the partition number
            if (partitionNumber == i + 1) { // Adjust if your numbering is different
                found = true;
                mbr[offset + 4] = newPartitionType; // 4th byte of the entry is the partition type
                break;
            }
        }

        if (!found) {
            std::wcerr << L"Partition not found in MBR or partition number mismatch." << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Write the modified MBR back to the disk
        DWORD bytesWritten;
        SetFilePointer(hDisk, 0, NULL, FILE_BEGIN);
        if (!WriteFile(hDisk, mbr, sizeof(mbr), &bytesWritten, NULL) || bytesWritten != sizeof(mbr)) {
            std::wcerr << L"Failed to write modified MBR. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Flush the file buffers to ensure changes are written to disk - Error 1 on Windows 7
        // if (!FlushFileBuffers(hDisk)) {
        //     std::wcerr << L"Failed to flush disk buffers. Error: " << GetLastError() << std::endl;
        //     CloseHandle(hDisk);
        //     return false;
        // }

        // Refresh the disk properties to ensure the system acknowledges the changes
        if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesWritten, NULL)) {
            std::wcerr << L"Failed to update disk properties. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Rescan the disk to make the changes take effect immediately
        // if (!DeviceIoControl(hDisk, IOCTL_DISK_RESCAN_DISK, NULL, 0, NULL, 0, &bytesWritten, NULL)) {
        //     std::wcerr << L"Failed to rescan the disk. Error: " << GetLastError() << std::endl;
        //     CloseHandle(hDisk);
        //     return false;
        // }

        CloseHandle(hDisk);

        // Notify the shell that the drive layout has changed
        //SHChangeNotify(SHCNE_DRIVEADD, SHCNF_FLUSH | SHCNF_FLUSHNOWAIT, NULL, NULL);


        // Remount

        //MountVolumeIfNotMounted(diskNumber, partitionNumber);

        //MountPartition(partitions, diskNumber, partitionNumber, volumeLetter);


        std::wcout << L"Partition type for Partition " << partitionNumber << L" on Disk " << diskNumber << L" changed to 0x" << std::hex << static_cast<unsigned>(newPartitionType) << std::endl;
        return true;
    } else {
        std::wcout << L"This command is valid only for partition on MBR disk" << std::endl;
        return false;
    }
}




bool ChangePartitionTypeWithAPI(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, BYTE newPartitionType) {

    UnmountVolumeIfMounted(partitions, diskNumber, partitionNumber);

    // Open the disk for read/write access
    std::wstring diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
    HANDLE hDisk = CreateFileW(diskPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open disk. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Get the size required for the drive layout information
    DWORD bytesReturned = 0;
    DRIVE_LAYOUT_INFORMATION_EX* driveLayout = NULL;
    DWORD driveLayoutSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 128; // Assume a maximum of 128 partitions
    std::vector<BYTE> driveLayoutBuffer(driveLayoutSize);

    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, driveLayoutBuffer.data(), driveLayoutSize, &bytesReturned, NULL)) {
        std::wcerr << L"Failed to get drive layout. Error: " << GetLastError() << std::endl;
        CloseHandle(hDisk);
        return false;
    }

    driveLayout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(driveLayoutBuffer.data());

    // Modify the partition type for the specified partition
    bool found = false;
    for (DWORD i = 0; i < driveLayout->PartitionCount; ++i) {
        if (driveLayout->PartitionEntry[i].PartitionNumber == partitionNumber) {
            driveLayout->PartitionEntry[i].Mbr.PartitionType = newPartitionType;
            found = true;
            break;
        }
    }

    if (!found) {
        std::wcerr << L"Partition not found." << std::endl;
        CloseHandle(hDisk);
        return false;
    }

    // Write the modified drive layout back to the disk
    if (!DeviceIoControl(hDisk, IOCTL_DISK_SET_DRIVE_LAYOUT_EX, driveLayoutBuffer.data(), driveLayoutSize, NULL, 0, &bytesReturned, NULL)) {
        std::wcerr << L"Failed to set drive layout. Error: " << GetLastError() << std::endl;
        CloseHandle(hDisk);
        return false;
    }

    if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
        std::wcerr << L"Failed to update disk properties. Error: " << GetLastError() << std::endl;
        CloseHandle(hDisk);
        return false;
    }

    CloseHandle(hDisk);
    return true;
}




#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

bool IsWindowsVersionGreaterOrEqual(DWORD majorVersion, DWORD minorVersion) {
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (STATUS_SUCCESS == fxPtr(&rovi)) {
                if (rovi.dwMajorVersion > majorVersion) {
                    return true;
                }
                else if (rovi.dwMajorVersion == majorVersion && rovi.dwMinorVersion >= minorVersion) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::wstring ReplaceSpecialCharacters(const std::wstring& input) {

    // On Windows 8 and newer can be displayed UTF16 in console without issues
    if (IsWindowsVersionGreaterOrEqual(6, 2)) {
        return input;
    }

    static const std::map<wchar_t, wchar_t> replacements = {
        // Latin (basic and extended)
        {L'À', L'A'}, {L'Á', L'A'}, {L'Â', L'A'}, {L'Ã', L'A'}, {L'Ä', L'A'}, {L'Å', L'A'},/* {L'Æ', L'AE'}, */
        {L'à', L'a'}, {L'á', L'a'}, {L'â', L'a'}, {L'ã', L'a'}, {L'ä', L'a'}, {L'å', L'a'},/* {L'æ', L'ae'}, */
        {L'Ç', L'C'}, {L'ç', L'c'},
        {L'È', L'E'}, {L'É', L'E'}, {L'Ê', L'E'}, {L'Ë', L'E'},
        {L'è', L'e'}, {L'é', L'e'}, {L'ê', L'e'}, {L'ë', L'e'},
        {L'Ì', L'I'}, {L'Í', L'I'}, {L'Î', L'I'}, {L'Ï', L'I'},
        {L'ì', L'i'}, {L'í', L'i'}, {L'î', L'i'}, {L'ï', L'i'},
        {L'Ñ', L'N'}, {L'ñ', L'n'},
        {L'Ò', L'O'}, {L'Ó', L'O'}, {L'Ô', L'O'}, {L'Õ', L'O'}, {L'Ö', L'O'}, {L'Ø', L'O'},
        {L'ò', L'o'}, {L'ó', L'o'}, {L'ô', L'o'}, {L'õ', L'o'}, {L'ö', L'o'}, {L'ø', L'o'},
        /*{L'ß', L'ss'}, */
        {L'Ù', L'U'}, {L'Ú', L'U'}, {L'Û', L'U'}, {L'Ü', L'U'},
        {L'ù', L'u'}, {L'ú', L'u'}, {L'û', L'u'}, {L'ü', L'u'},
        {L'Ý', L'Y'}, {L'ý', L'y'}, {L'ÿ', L'y'},

        // Slavic and Eastern European characters
        {L'Ą', L'A'}, {L'ą', L'a'}, {L'Ć', L'C'}, {L'ć', L'c'},
        {L'Ę', L'E'}, {L'ę', L'e'}, {L'Ł', L'L'}, {L'ł', L'l'},
        {L'Ń', L'N'}, {L'ń', L'n'}, {L'Ó', L'O'}, {L'ó', L'o'},
        {L'Ś', L'S'}, {L'ś', L's'}, {L'Ź', L'Z'}, {L'ź', L'z'},
        {L'Ż', L'Z'}, {L'ż', L'z'},

        // Additional characters
        {L'Đ', L'D'}, {L'đ', L'd'}, {L'Ħ', L'H'}, {L'ħ', L'h'},
        {L'Ŀ', L'L'}, {L'ŀ', L'l'}, {L'Ł', L'L'}, {L'ł', L'l'},
        {L'Ŋ', L'N'}, {L'ŋ', L'n'}, /*{L'Œ', L'OE'}, {L'œ', L'oe'}, */
        {L'Š', L'S'}, {L'š', L's'}, {L'Ť', L'T'}, {L'ť', L't'},
        {L'Ž', L'Z'}, {L'ž', L'z'}, {L'Ə', L'A'}, {L'ə', L'a'},
        // ... (extend with more characters as needed)

        // Note: This list is not exhaustive and mainly covers characters with diacritics.

    };

    std::wstring result;
    for (wchar_t ch : input) {
        auto it = replacements.find(ch);
        if (it != replacements.end()) {
            result += it->second;
        }
        else {
            result += ch;
        }
    }

    return result;
}



int main(int argc, char* argv[]) {

    if (_isatty(_fileno(stdout))) {
        if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
            std::wcerr << L"Failed to set the console output UTF16. Unicode output may not work correctly.\n";
            // Handle the error or exit if necessary
            // exit(1); // Uncomment this line if you want to exit the program in case of an error
        }
    }


    auto partitions = GetPartitionInfo();
 
    if (argc > 1) {
        std::string command = argv[1];

        if (command == "/help" || command == "/?") {
            // Handle the /help command
            std::wcout << L"\n";
            std::wcout << L" ListDisk\n\n";
            std::wcout << L" This tool provides detailed information about connected physical disks, \n";
            std::wcout << L" including their partitions and volumes.\n";
            std::wcout << L"\n";
            std::wcout << L" Features include mounting/unmounting partitions, setting partition labels, \n";
            std::wcout << L" and modifying MBR partition attributes.\n";
            std::wcout << L"\n";
            std::wcout << L" Available Commands:\n";
            std::wcout << L"   /mount             - Mounts a partition to a specified drive letter.\n";
            std::wcout << L"   /unmount           - Unmounts a specified partition or drive letter.\n";
            std::wcout << L"   /setlabel          - Sets a new label for a specified partition.\n";
            std::wcout << L"   /setactive         - Sets or clears the bootable flag of a partition.\n";
            std::wcout << L"   /settype           - Changes the MBR type of a partition.\n";
            std::wcout << L"\n";
            std::wcout << L" Usage Examples:\n";
            std::wcout << L"   /mount 0 1 U       - Mounts Partition 1 on Disk 0 as U: drive.\n";
            std::wcout << L"   /unmount 0 1       - Unmounts Partition 1 on Disk 0.\n";
            std::wcout << L"   /unmount U         - Unmounts U: drive.\n";
            std::wcout << L"   /setLabel 0 1 Lbl  - Sets 'Lbl' as the label for Partition 1 on Disk 0.\n";
            std::wcout << L"   /setactive 0 1     - Marks Partition 1 on Disk 0 as bootable.\n";
            std::wcout << L"   /setactive 0 1 80  - Sets the boot flag for Partition 1 on Disk 0.\n";
            std::wcout << L"   /setactive 0 1 00  - Clears the boot flag for Partition 1 on Disk 0.\n";
            std::wcout << L"   /settype 0 1 0x17  - Sets Partition 1 on Disk 0 to type NTFS Hidden (0x17).\n";
            std::wcout << L"   /settype 0 1 0x07  - Sets Partition 1 on Disk 0 to type NTFS (0x07).\n";
            std::wcout << L"\n";
            std::wcout << L" Note: Use this utility with caution. Incorrect usage may affect data integrity.\n";
            return 0;
        }
        else if (command == "/mount") {
            // Handle the /mount command
            if (argc < 5) {
                std::wcout << "  Usage: /mount <disk_number> <partition_number> <drive_letter>\n";
                std::wcout << "Example: /mount 0 1 U - Mounts Partition 1 on Disk 0 as U: drive.\n";

                return 1;
            }

            int diskNumber = std::stoi(argv[2]);
            int partitionNumber = std::stoi(argv[3]);
            std::wstring driveLetter = L"";
            driveLetter += argv[4][0];  // Convert char to wchar_t
            driveLetter += L":\\";      // Append backslash as required by Windows API

            
            if (MountPartition(partitions, diskNumber, partitionNumber, driveLetter.c_str())) {
                std::wcout << L"Mounted Disk " << diskNumber << L", Partition " << partitionNumber << L" to drive letter " << driveLetter << std::endl;
            }
            else {
                std::wcout << L"Failed to mount Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
            }
            return 0;
        }
        else if (command == "/unmount") {
            // Handle the /unmount command
            if (argc < 3) {
                std::wcout << "  Usage: /unmount <disk_number> <partition_number> or /unmount <drive_letter>\n";
                std::wcout << "Example: /unmount 0 1 - Unmounts Partition 1 on Disk 0.\n";
                std::wcout << "Example: /unmount U - Unmounts U: drive.\n";
                return 1;
            }

            std::string target = argv[2];
            if (target.size() == 1 && isalpha(target[0])) {
                // Unmount by drive letter
                char driveLetterChar = target[0];
                std::wstring driveLetter = L"";
                driveLetter += (wchar_t)driveLetterChar;
                driveLetter += L":\\";

                UnMountPartitionByDriveLetter(driveLetter.c_str());
                std::wcout << "Unmounted drive letter " << driveLetterChar << std::endl;
            }
            else {
                // Unmount by disk number and partition number
                int diskNumber = std::stoi(target);
                int partitionNumber = argc > 3 ? std::stoi(argv[3]) : 0; // Default to 0 if not provided

                UnMountPartition(partitions, diskNumber, partitionNumber);
                std::wcout << "Unmounting Disk " << diskNumber << ", Partition " << partitionNumber << std::endl;
            }

            return 0;
        }
        else if (command == "/setactive") {
            if (argc < 4 || argc > 5) {
                std::wcout << "  Usage: /settype <disk_number> <partition_number> <new_partition_type>\n";
                std::wcout << "Example: /settype 0 1 0x17 - Sets Partition 1 on Disk 0 to NTFS Hidden (0x17).\n";
                return 1;
            }

            int diskNumber = std::stoi(argv[2]);
            int partitionNumber = std::stoi(argv[3]);
            BYTE setActiveValue = 0x80; // Default value

            // Check if the active value is provided
            if (argc == 5) {
                setActiveValue = static_cast<BYTE>(std::stoi(argv[4], nullptr, 16)); // Parse as hexadecimal
            }

            SetPartitionActiveDirectly(partitions, diskNumber, partitionNumber, setActiveValue);

            return 0;
        }
        else if (command == "/settype") {
            if (argc != 5) {
                std::wcout <<   "Usage: /settype <disk_number> <partition_number> <new_partition_type>\n";
                std::wcout << "Example: /settype 0 1 0x07 - Sets Partition 1 on Disk 0 to NTFS (0x07).\n";

                return 1;
            }

            int diskNumber = std::stoi(argv[2]);
            int partitionNumber = std::stoi(argv[3]);
            BYTE newPartitionType = static_cast<BYTE>(std::stoi(argv[4], nullptr, 16)); // Correctly parse as hexadecimal

            ChangePartitionTypeDirectly(partitions, diskNumber, partitionNumber, newPartitionType);
            // API method no hangs, no errors, and nothing changed..
            //ChangePartitionTypeWithAPI(partitions, diskNumber, partitionNumber, newPartitionType); 


            return 0;
        }
        else {
            std::wcout << "Unknown command: " << std::wstring(command.begin(), command.end()) << "\n";
            std::wcout << "Use /? or /help for a list of available commands.\n";
            return 1;
        }
    }


    std::wcout << L"\n";

    std::wcout << L"|  Disk  "
        << L"|  Partition  "
        << L"| Style "
        << L"|    Size    "
        << L"|     Type      "
        << L"| System "
        << L"| Ltr "
        << L"|      Label       |";
    std::wcout << std::endl;

    // Add an empty line as a separator
    std::wcout << L" -------- ------------- ------- ------------ --------------- -------- ----- ------------------" << std::endl;

    for (const auto& partition : partitions) {
        std::wcout << L"| Disk " << partition.DiskNumber << L" "
            << L"| Partition " << partition.PartitionNumber << L" "
            << L"| " << FormatColumn(PartitionStyleToWString(partition.PartitionStyle), 5, Align::Left) << L" "
            << L"| " << FormatSize(partition.PartitionLength) << L" "
            << L"| " << FormatColumn(partition.PartitionType, 13, Align::Left) << L" "
            << L"| " << FormatColumn(partition.FileSystem, 7, Align::Left)
            << L"| " << FormatColumn(FormatLetter(partition.DriveLetter), 3, Align::Left) << L" "
            //<< L"| " << partition.MountPointName
            << L"| " << FormatColumn(ReplaceSpecialCharacters(partition.VolumeName), 16, Align::Left) << L" |";



        std::wcout << std::endl;
    }

    return 0;
}
