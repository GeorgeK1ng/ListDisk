#define _WIN32_IE 0x0600
#define NTDDI_VERSION NTDDI_WINXP
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <winioctl.h>
#include <SetupAPI.h>
#include <InitGuid.h>

#include <fcntl.h>
#include <io.h>

#include <map>
#include <string>

#include <ntddscsi.h>
#include <ntdddisk.h>

#include "ListDisk.h"


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

#define STATUS_SUCCESS (0x00000000)

// Define missing bus types if they are not defined by the SDK
#ifndef BusTypeiScsi
#define BusTypeiScsi 9
#endif

#ifndef BusTypeSas
#define BusTypeSas 10
#endif

#ifndef BusTypeSata
#define BusTypeSata 11
#endif

#ifndef BusTypeSd
#define BusTypeSd 12
#endif

#ifndef BusTypeMmc
#define BusTypeMmc 13
#endif

#ifndef BusTypeVirtual
#define BusTypeVirtual 14
#endif

#ifndef BusTypeFileBackedVirtual
#define BusTypeFileBackedVirtual 15
#endif

#ifndef BusTypeSpaces
#define BusTypeSpaces 16
#endif

#ifndef BusTypeNvme
#define BusTypeNvme 17
#endif

#ifndef BusTypeSCM
#define BusTypeSCM 18
#endif

#ifndef BusTypeUfs
#define BusTypeUfs 19
#endif

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

enum class Align {
    Left,
    Right
};

struct PartitionInfo {
    DWORD DiskNumber = 0;            // Physical disk number
    DWORD PartitionNumber = 0;       // Physical partition number
    PARTITION_STYLE PartitionStyle = PARTITION_STYLE_RAW;
    LONGLONG PartitionLength = 0;    // Size
    LONGLONG PartitionOffset = 0;    // Offset of the partition from the start of the disk
    std::wstring VolumeName;         // Label
    std::wstring FileSystem;
    std::wstring DriveLetter;
    std::wstring MountPoint;
    std::wstring MountPointName;     /* Volume mount point like \ ? \Volume{24379f2b-7a05-44b4-a9c3-6ef5fd068451} */
    std::wstring PartitionType;
    GUID PartitionId = {};           // Added for GPT partition information
    std::wstring GptName;            // For GPT partition name - doesn't contain all defined names
    BOOLEAN BootIndicator = FALSE;   // MBR Boot indicator
    // New fields
    std::wstring Vendor;
    std::wstring Product;
    std::wstring SerialNumber;
    std::wstring BusType;
    BOOLEAN Removable = FALSE;
    BOOLEAN TrimSupported = FALSE;
    std::wstring HBTL;               // Host Bus Target LUN

    // New fields for partition attributes
    ULONGLONG GptAttributes = 0;     // GPT-specific attributes
    BYTE MbrPartitionTypeCode = 0;   // New field to store the raw MBR partition type code

    // Additional fields for more comprehensive information
    DWORD PhysicalSectorSize = 0;     // Physical sector size of the disk
    DWORD LogicalSectorSize = 0;      // Logical sector size of the partition
    std::wstring DiskModel;           // Disk model number
    std::wstring PartitionHealthStatus; // Health status of the partition
    BOOLEAN CompressionEnabled = FALSE;  // Is compression enabled?
    std::wstring EncryptionStatus;    // Status of encryption on the partition
    BOOLEAN ReadOnly = FALSE;            // Is the partition read-only?
    BOOLEAN SystemPartition = FALSE;     // Is it a system or boot partition?
    std::wstring RAIDLevel;           // RAID level if applicable
    std::wstring FilesystemVersion;   // Version of the filesystem
    std::wstring FilesystemFlags;     // Filesystem-specific flags
};


// Helper function to convert ANSI string to wstring
std::wstring AnsiToWString(const char* ansiStr) {
    if (!ansiStr) return L"";
    int len = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, &wstr[0], len);
    return wstr;
}


// Helper function to map BusType to string
std::wstring GetBusTypeString(STORAGE_BUS_TYPE busType) {
    switch (busType) {
    case BusTypeScsi: return L"SCSI";
    case BusTypeAtapi: return L"ATAPI";
    case BusTypeAta: return L"ATA";
    case BusType1394: return L"1394";
    case BusTypeSsa: return L"SSA";
    case BusTypeFibre: return L"Fibre";
    case BusTypeUsb: return L"USB";
    case BusTypeRAID: return L"RAID";
    case BusTypeiScsi: return L"iSCSI";
    case BusTypeSas: return L"SAS";
    case BusTypeSata: return L"SATA";
    case BusTypeSd: return L"SD";
    case BusTypeMmc: return L"MMC";
    case BusTypeVirtual: return L"Virtual";
    case BusTypeFileBackedVirtual: return L"FileBackedVirtual";
    case BusTypeSpaces: return L"Spaces";
    case BusTypeNvme: return L"NVMe";
    case BusTypeSCM: return L"SCM";
    case BusTypeUfs: return L"UFS";
    default: return L"Unknown";
    }
}


// Function to get partition information
std::vector<PartitionInfo> GetPartitionInfo() {
    std::vector<PartitionInfo> partitions;
    HANDLE hDrive = INVALID_HANDLE_VALUE;
    HANDLE hVolume = INVALID_HANDLE_VALUE;
    DWORD bytesReturned = 0;

    int driveIndex = 0;
    while (true) {
        WCHAR driveName[25];
        swprintf_s(driveName, L"\\\\.\\PhysicalDrive%d", driveIndex);

        hDrive = CreateFileW(driveName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        if (hDrive == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                break; // No more drives to check
            }
            std::cerr << "Failed to open " << driveName << ", Error: " << GetLastError() << std::endl;
            driveIndex++;
            continue;
        }

        DWORD bufferSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 128;
        std::unique_ptr<char[]> buffer(new char[bufferSize]);
        DRIVE_LAYOUT_INFORMATION_EX* driveLayout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.get());

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
                    info.PartitionOffset = partition.StartingOffset.QuadPart;

                    if (partition.PartitionStyle == PARTITION_STYLE_GPT) {
                        PARTITION_INFORMATION_GPT& gptPartition = partition.Gpt;
                        info.PartitionId = gptPartition.PartitionId;
                        info.GptAttributes = gptPartition.Attributes; // Store GPT attributes

                        wchar_t gptNameArray[36] = {};
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
                        else
                            info.PartitionType = L"Unknown";
                    }
                    else if (partition.PartitionStyle == PARTITION_STYLE_MBR) {
                        info.MbrPartitionTypeCode = partition.Mbr.PartitionType; // Store raw MBR partition type code

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

                        if (partition.Mbr.BootIndicator != 0) {
                            info.PartitionType += L" *";
                            info.BootIndicator = partition.Mbr.BootIndicator;
                        }
                    }

                    // Generate the volume path
                    WCHAR volumePath[MAX_PATH];
                    swprintf_s(volumePath, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%d\\Partition%d", info.DiskNumber, info.PartitionNumber);

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
                            info.MountPointName = volumeName;

                            // Get the drive letter associated with this volume.
                            std::vector<WCHAR> buffer(MAX_PATH);
                            DWORD bufferSize = static_cast<DWORD>(buffer.size());

                            if (GetVolumePathNamesForVolumeNameW(volumeName, buffer.data(), bufferSize, &bufferSize)) {
                                info.DriveLetter = buffer.data();
                            }
                        }

                        // Fallback: Check for the drive letter using DefineDosDevice symbolic links
                        if (info.DriveLetter.empty()) {
                            WCHAR dosDeviceList[MAX_PATH * 100] = { 0 };
                            DWORD charCount = QueryDosDeviceW(NULL, dosDeviceList, ARRAYSIZE(dosDeviceList));
                            if (charCount != 0) {
                                for (WCHAR* dev = dosDeviceList; *dev; dev += wcslen(dev) + 1) {
                                    WCHAR devicePath[MAX_PATH] = { 0 };
                                    if (QueryDosDeviceW(dev, devicePath, ARRAYSIZE(devicePath)) != 0) {
                                        if (wcsstr(devicePath, volumePath + 14) != NULL) {
                                            info.DriveLetter = dev;
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        CloseHandle(hVolume);
                    }

                    // Extended information retrieval
                    STORAGE_PROPERTY_QUERY storageQuery = {};
                    storageQuery.PropertyId = StorageDeviceProperty;
                    storageQuery.QueryType = PropertyStandardQuery;

                    STORAGE_DESCRIPTOR_HEADER storageHeader = {};
                    if (DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY, &storageQuery, sizeof(storageQuery), &storageHeader, sizeof(storageHeader), &bytesReturned, NULL)) {
                        std::unique_ptr<BYTE[]> descriptorBuffer(new BYTE[storageHeader.Size]);
                        if (DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY, &storageQuery, sizeof(storageQuery), descriptorBuffer.get(), storageHeader.Size, &bytesReturned, NULL)) {
                            auto deviceDescriptor = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>(descriptorBuffer.get());

                            if (deviceDescriptor->VendorIdOffset) {
                                info.Vendor = AnsiToWString(reinterpret_cast<const char*>(descriptorBuffer.get() + deviceDescriptor->VendorIdOffset));
                            }
                            if (deviceDescriptor->ProductIdOffset) {
                                info.Product = AnsiToWString(reinterpret_cast<const char*>(descriptorBuffer.get() + deviceDescriptor->ProductIdOffset));
                            }
                            if (deviceDescriptor->SerialNumberOffset) {
                                info.SerialNumber = AnsiToWString(reinterpret_cast<const char*>(descriptorBuffer.get() + deviceDescriptor->SerialNumberOffset));
                            }

                            // Get Bus Type
                            info.BusType = GetBusTypeString(deviceDescriptor->BusType);

                            // Check if the media is removable
                            info.Removable = (deviceDescriptor->RemovableMedia != 0);
                        }
                    }

                    // Query for Trim support
                    STORAGE_PROPERTY_QUERY trimQuery = { StorageDeviceTrimProperty, PropertyStandardQuery };
                    DEVICE_TRIM_DESCRIPTOR trimDescriptor = {};
                    if (DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY, &trimQuery, sizeof(trimQuery), &trimDescriptor, sizeof(trimDescriptor), &bytesReturned, NULL)) {
                        info.TrimSupported = (trimDescriptor.TrimEnabled != 0);
                    }

                    // Query for SCSI address
                    SCSI_ADDRESS scsiAddress = {};
                    if (DeviceIoControl(hDrive, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &scsiAddress, sizeof(scsiAddress), &bytesReturned, NULL)) {
                        std::wstringstream ss;
                        ss << scsiAddress.PortNumber << L":" << scsiAddress.PathId << L":" << scsiAddress.TargetId << L":" << scsiAddress.Lun;
                        info.HBTL = ss.str();
                    }

                    // Additional fields (e.g., physical and logical sector size)
                    DISK_GEOMETRY_EX diskGeometry = {};
                    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &diskGeometry, sizeof(diskGeometry), &bytesReturned, NULL)) {
                        info.PhysicalSectorSize = diskGeometry.Geometry.BytesPerSector;
                        info.LogicalSectorSize = diskGeometry.Geometry.BytesPerSector; // Assuming same value for now
                    }

                    // Example: Checking if the partition is read-only
                    if (!DeviceIoControl(hVolume, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
                        info.ReadOnly = true;
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
    return PARTITION_STYLE_RAW;
    // If not found, throw an exception
    //throw std::runtime_error("Partition not found for Disk " + std::to_string(diskNumber) + ", Partition " + std::to_string(partitionNumber));
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


std::wstring GetLogicalName(DWORD DiskNumber, DWORD PartitionNumber, uint64_t PartitionOffset) {
    WCHAR volume_name[MAX_PATH] = { 0 };
    HANDLE hVolume = FindFirstVolumeW(volume_name, ARRAYSIZE(volume_name));

    if (hVolume == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Could not access first GUID volume: " << GetLastError() << std::endl;
        return L"";
    }

    std::wstring volumeName;
    VOLUME_DISK_EXTENTS diskExtents;
    DWORD size = 0;

    do {
        size_t volumePathLength = wcslen(volume_name);
        if (volumePathLength <= 4 || volume_name[volumePathLength - 1] != L'\\') {
            continue;
        }

        // Safely convert size_t to DWORD after ensuring it's within range
        DWORD volumePathLengthDword = static_cast<DWORD>(volumePathLength);
        if (volumePathLengthDword != volumePathLength) {
            std::wcerr << L"Volume path length exceeds DWORD maximum." << std::endl;
            break;
        }

        // Remove trailing backslash to prepare for CreateFileW
        volume_name[volumePathLength - 1] = L'\0';

        HANDLE hDrive = CreateFileW(volume_name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDrive == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Could not open GUID volume '" << volume_name << L"': " << GetLastError() << std::endl;
            continue;
        }

        BOOL result = DeviceIoControl(hDrive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &diskExtents, sizeof(diskExtents), &size, NULL);
        CloseHandle(hDrive);

        if (!result || size == 0) {
            //std::wcerr << L"Could not get Disk Extents for volume '" << volume_name << L"': " << GetLastError() << std::endl;
            // This means we need to go to Fallback logic
            continue;
        }
        else if (diskExtents.NumberOfDiskExtents == 1 && diskExtents.Extents[0].DiskNumber == DiskNumber &&
            (PartitionOffset == 0 || diskExtents.Extents[0].StartingOffset.QuadPart == PartitionOffset)) {
            volumeName = volume_name;
            break;
        }

    } while (FindNextVolumeW(hVolume, volume_name, ARRAYSIZE(volume_name)));

    FindVolumeClose(hVolume);

    if (volumeName.empty()) {
        // Fallback: Use device paths directly if no volume name found
        WCHAR devicePath[MAX_PATH];
        swprintf_s(devicePath, MAX_PATH, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%u\\Partition%u", DiskNumber, PartitionNumber);
        HANDLE hDrive = CreateFileW(devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hDrive != INVALID_HANDLE_VALUE) {
            volumeName = devicePath;  // Use the device path directly if available
            CloseHandle(hDrive);
        }
    }

    return volumeName;
}


bool MountPartition(DWORD DiskNumber, DWORD PartitionNumber, const std::wstring& driveLetter) {
    auto partitions = GetPartitionInfo();
    auto partitionInfo = std::find_if(partitions.begin(), partitions.end(), [&](const PartitionInfo& p) {
        return p.DiskNumber == DiskNumber && p.PartitionNumber == PartitionNumber;
        });

    if (partitionInfo == partitions.end()) {
        std::wcerr << L"Partition not found for Disk " << DiskNumber << L", Partition " << PartitionNumber << std::endl;
        return false;
    }

    std::wstring volumeName = GetLogicalName(DiskNumber, PartitionNumber, partitionInfo->PartitionOffset);

    if (volumeName.empty()) {
        std::wcerr << L"Could not get volume name for Disk " << DiskNumber << L", Partition " << PartitionNumber << std::endl;
        return false;
    }

    // Ensure drive letter is formatted correctly with a backslash
    std::wstring formattedDriveLetter = driveLetter;
    if (formattedDriveLetter.back() != L'\\') {
        formattedDriveLetter += L'\\';
    }

    // Ensure volume name is correctly formatted for SetVolumeMountPointW
    std::wstring formattedVolumeName = volumeName;
    if (formattedVolumeName.back() != L'\\') {
        formattedVolumeName += L'\\';
    }

    // Attempt to mount using SetVolumeMountPointW first
    if (SetVolumeMountPointW(formattedDriveLetter.c_str(), formattedVolumeName.c_str())) {
        //std::wcout << L"Successfully mounted " << volumeName << L" to " << formattedDriveLetter << L" using SetVolumeMountPointW." << std::endl;
        return true;
    }
    else {
        DWORD lastError = GetLastError();
        //std::wcerr << L"Failed to mount " << volumeName << L" to " << formattedDriveLetter << L" using SetVolumeMountPointW. Error: " << lastError << std::endl;

        // If SetVolumeMountPointW fails, check if we should fallback to DefineDosDeviceW
        if (volumeName.find(L"\\\\?\\GLOBALROOT") == 0 || lastError == ERROR_INVALID_PARAMETER) {
            WCHAR dosName[] = L"?:";
            dosName[0] = driveLetter[0];

            // Remove any existing mapping for the drive letter to avoid conflicts
            DefineDosDeviceW(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, dosName, NULL);

            // Format volumeName correctly for DefineDosDeviceW (strip "\\?\GLOBALROOT")
            std::wstring devicePath = volumeName;
            if (devicePath.find(L"\\\\?\\GLOBALROOT") == 0) {
                devicePath = devicePath.substr(14);  // Strip "\\?\GLOBALROOT"
            }

            // Attempt fallback mounting using DefineDosDeviceW
            if (DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM, dosName, devicePath.c_str())) {
                //std::wcout << volumeName << L" was successfully mounted as " << dosName << L" using DefineDosDeviceW." << std::endl;
                return true;
            }
            else {
                //std::wcerr << L"Could not mount " << volumeName << L" as " << dosName << L" using DefineDosDeviceW. Error: " << GetLastError() << std::endl;
                return false;
            }
        }
    }

    return false;
}


bool UnMountPartition(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    // Get the drive letter associated with the partition
    std::wstring volumeLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    if (volumeLetter.empty()) {
        //std::wcerr << L"Drive letter not found for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return false;
    }

    // Attempt to unmount using DeleteVolumeMountPointW
    if (DeleteVolumeMountPointW(volumeLetter.c_str())) {
        //std::wcout << L"Successfully unmounted " << volumeLetter << std::endl;
        return true;
    }
    else {
        //std::wcerr << L"Failed to unmount using DeleteVolumeMountPointW for " << volumeLetter << L". Error: " << GetLastError() << std::endl;

        // If DeleteVolumeMountPointW fails, use DefineDosDeviceW as a fallback
        WCHAR dosName[] = L"?:";
        dosName[0] = volumeLetter[0];  // Set drive letter

        if (!DefineDosDeviceW(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, dosName, NULL)) {
            //std::wcerr << L"Failed to unmount using DefineDosDeviceW for " << dosName << L". Error: " << GetLastError() << std::endl;
            return false;
        }

        //std::wcout << L"Successfully unmounted '" << dosName << L"' using DefineDosDeviceW fallback." << std::endl;
        return true;
    }
}


bool UnMountPartitionByDriveLetter(const wchar_t* driveLetter) {
    std::wstring volumePath = driveLetter;

    // Ensure the drive letter ends with a backslash
    if (!volumePath.empty() && volumePath.back() != L'\\') {
        volumePath += L'\\';
    }

    // Attempt to unmount using DeleteVolumeMountPointW
    if (DeleteVolumeMountPointW(volumePath.c_str())) {
        //std::wcout << L"Successfully unmounted drive " << driveLetter << std::endl;
        return true;
    }
    else {
        // If DeleteVolumeMountPointW fails, use DefineDosDeviceW as a fallback
        //std::wcerr << L"Failed to unmount using DeleteVolumeMountPointW for drive " << driveLetter << L". Error: " << GetLastError() << std::endl;

        // DefineDosDeviceW Fallback
        WCHAR dosName[] = L"?:";
        dosName[0] = driveLetter[0];  // Set drive letter

        if (!DefineDosDeviceW(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, dosName, NULL)) {
            //std::wcerr << L"Failed to unmount using DefineDosDeviceW for drive " << dosName << L". Error: " << GetLastError() << std::endl;
            return false;
        }

        //std::wcout << L"Successfully unmounted '" << dosName << L"' using DefineDosDeviceW fallback." << std::endl;
        return true;
    }
}


// Function to unmount a partition if it is mounted, and return its drive letter if it was mounted
std::wstring UnmountPartitionIfMounted(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    // First, check if the partition is mounted by looking up the drive letter
    std::wstring driveLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    if (driveLetter.empty()) {
        // If no drive letter is found, the partition is likely not mounted
        //std::wcerr << L"Partition not mounted for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return L"";
    }

    // Attempt to unmount using the existing UnMountPartition function
    if (UnMountPartition(partitions, diskNumber, partitionNumber)) {
        //std::wcout << L"Successfully unmounted Disk " << diskNumber << L", Partition " << partitionNumber << L" with drive letter " << driveLetter << std::endl;
        return driveLetter;  // Return the drive letter that was unmounted
    }
    else {
        //std::wcerr << L"Failed to unmount Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
        return L"";  // Return an empty string if the unmount failed
    }
}


// Function to get available drive letters
std::vector<std::wstring> GetAvailableDriveLetters() {
    // Buffer to hold the list of all defined DOS devices
    WCHAR deviceNames[65536] = { 0 }; // 64KB buffer
    DWORD charsReturned = QueryDosDeviceW(NULL, deviceNames, 65536);
    std::vector<std::wstring> usedLetters;

    if (charsReturned != 0) {
        // Parse the buffer returned by QueryDosDevice
        WCHAR* device = deviceNames;
        while (*device) {
            if (wcslen(device) == 2 && device[1] == L':') {
                usedLetters.push_back(std::wstring(device)); // Collect used drive letters
            }
            device += wcslen(device) + 1;
        }
    }

    // Additionally, check volume mount points using GetVolumePathNamesForVolumeNameW
    WCHAR volumeName[MAX_PATH] = { 0 };
    HANDLE hVolume = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));
    if (hVolume != INVALID_HANDLE_VALUE) {
        do {
            WCHAR volumePaths[MAX_PATH] = { 0 };
            DWORD pathLength = 0;
            GetVolumePathNamesForVolumeNameW(volumeName, volumePaths, MAX_PATH, &pathLength);

            WCHAR* path = volumePaths;
            while (*path) {
                if (wcslen(path) == 3 && path[1] == L':' && path[2] == L'\\') {
                    usedLetters.push_back(std::wstring(path, 2)); // Add the drive letter (e.g., "C:")
                }
                path += wcslen(path) + 1;
            }
        } while (FindNextVolumeW(hVolume, volumeName, ARRAYSIZE(volumeName)));

        FindVolumeClose(hVolume);
    }

    // List of available drive letters in reverse order (Z to C)
    std::vector<std::wstring> availableLetters;
    for (wchar_t letter = L'Z'; letter >= L'C'; --letter) {
        std::wstring driveLetter = std::wstring(1, letter) + L":";
        if (std::find(usedLetters.begin(), usedLetters.end(), driveLetter) == usedLetters.end()) {
            // Check also with GetDriveType to avoid conflicts with other types like floppy, CD/DVD, etc.
            WCHAR drivePath[] = L"X:\\";
            drivePath[0] = letter;
            UINT driveType = GetDriveTypeW(drivePath);

            if (driveType == DRIVE_NO_ROOT_DIR) { // DRIVE_NO_ROOT_DIR means no volume is mounted to that letter
                availableLetters.push_back(driveLetter + L"\\"); // Add available drive letter
            }
        }
    }

    return availableLetters;
}


// Function to check if a partition is already mounted and return its drive letter if so
std::wstring IsPartitionAlreadyMounted(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber) {
    for (const auto& partition : partitions) {
        if (partition.DiskNumber == diskNumber && partition.PartitionNumber == partitionNumber) {
            // Check if a drive letter is already assigned
            if (!partition.DriveLetter.empty()) {
                return partition.DriveLetter;  // Partition is already mounted
            }
        }
    }

    return L"";  // Partition is not mounted
}


bool SetLabel(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, const std::wstring& newLabel) {
    // Retrieve the drive letter using your existing functions
    std::wstring driveLetter = GetDriveLetter(partitions, diskNumber, partitionNumber);

    bool wasMounted = !driveLetter.empty();
    bool tempMount = false;

    if (!wasMounted) {
        // If the partition is not mounted, mount it temporarily
        std::vector<std::wstring> availableLetters = GetAvailableDriveLetters();
        if (!availableLetters.empty()) {
            driveLetter = availableLetters.front();
            if (MountPartition(diskNumber, partitionNumber, driveLetter)) {
                //std::wcout << L"Temporarily mounted Disk " << diskNumber << L", Partition " << partitionNumber << L" to " << driveLetter << std::endl;
                tempMount = true;
            }
            else {
                //std::wcerr << L"Failed to temporarily mount Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
                return false;
            }
        }
        else {
            //std::wcerr << L"No available drive letters to mount Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
            return false;
        }
    }

    // Set the new label
    if (SetVolumeLabelW(driveLetter.c_str(), newLabel.c_str())) {
        //std::wcout << L"Label set to '" << newLabel << L"' for " << driveLetter << std::endl;
        if (tempMount) {
            UnMountPartitionByDriveLetter(driveLetter.c_str());
        }
        return true;
    }
    else {
        //std::wcerr << L"Failed to set label for " << driveLetter << L". Error: " << GetLastError() << std::endl;
        if (tempMount) {
            UnMountPartitionByDriveLetter(driveLetter.c_str());
        }
        return false;
    }
}


bool SetPartitionActiveDirectly(const std::vector<PartitionInfo>& partitions, DWORD diskNumber, DWORD partitionNumber, BYTE setActiveValue = 0x80) {

    // Proceed only on MBR disk
    if (GetPartitionStyle(partitions, diskNumber, partitionNumber) == PARTITION_STYLE_MBR) {

        // Unmount the volume and get the drive letter if it was mounted
        std::wstring mountedDriveLetter = UnmountPartitionIfMounted(partitions, diskNumber, partitionNumber);


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

        // Refresh the disk properties to ensure the system acknowledges the changes
        if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesWritten, NULL)) {
            std::wcerr << L"Failed to update disk properties. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Introduce a short delay to allow the system to recognize the changes
        Sleep(1000); // Wait for 1 second

        CloseHandle(hDisk);

        // Remount the volume if it was previously mounted
        if (!mountedDriveLetter.empty()) {
            MountPartition(diskNumber, partitionNumber, mountedDriveLetter);
        }


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

        // Unmount the volume and get the drive letter if it was mounted
        std::wstring mountedDriveLetter = UnmountPartitionIfMounted(partitions, diskNumber, partitionNumber);


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

        // Refresh the disk properties to ensure the system acknowledges the changes
        if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesWritten, NULL)) {
            std::wcerr << L"Failed to update disk properties. Error: " << GetLastError() << std::endl;
            CloseHandle(hDisk);
            return false;
        }

        // Introduce a short delay to allow the system to recognize the changes
        Sleep(1000); // Wait for 1 second

        CloseHandle(hDisk);

        // Remount the volume if it was previously mounted
        if (!mountedDriveLetter.empty()) {
            MountPartition(diskNumber, partitionNumber, mountedDriveLetter);
        }


        std::wcout << L"Partition type for Partition " << partitionNumber << L" on Disk " << diskNumber << L" changed to 0x" << std::hex << static_cast<unsigned>(newPartitionType) << std::endl;
        return true;
    } else {
        std::wcout << L"This command is valid only for partition on MBR disk" << std::endl;
        return false;
    }
}


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


bool IsRunningAsAdmin() {
    bool isAdmin = false;
    PSID adminGroupSID = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    // Allocate and initialize a SID for the administrators group.
    if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroupSID)) {
        std::cerr << "AllocateAndInitializeSid Error: " << GetLastError() << std::endl;
        return false;
    }

    // Check if the SID is valid and if the user belongs to the admin group.
    BOOL isInAdminGroup = FALSE;
    if (!CheckTokenMembership(NULL, adminGroupSID, &isInAdminGroup)) {
        std::cerr << "CheckTokenMembership Error: " << GetLastError() << std::endl;
    }
    else {
        isAdmin = isInAdminGroup == TRUE;
    }

    // Free the SID after use.
    FreeSid(adminGroupSID);

    return isAdmin;
}


int main(int argc, char* argv[]) {

    if (!IsRunningAsAdmin()) {
        std::wcerr << L"This tool needs to be launched with administrator privileges.\n";
        return 1; // Exit if not admin
    }

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
            std::wcout << L" ListDisk " << VER_PRODUCTVERSION_STR << "\n\n";
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
            std::wcout << L"   /mount 0 1         - Mounts Partition 1 on Disk 0 to empty drive letter.\n";
            std::wcout << L"   /mount 0 1 U       - Mounts Partition 1 on Disk 0 as U: drive.\n";
            std::wcout << L"   /unmount 0 1       - Unmounts Partition 1 on Disk 0.\n";
            std::wcout << L"   /unmount U         - Unmounts U: drive.\n";
            std::wcout << L"   /setlabel 0 1 Lbl  - Sets 'Lbl' as the label for Partition 1 on Disk 0.\n";
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
            if (argc < 4) {
                std::wcout << "  Usage: /mount <disk_number> <partition_number> <drive_letter>\n";
                std::wcout << "Example: /mount 0 1 U - Mounts Partition 1 on Disk 0 as U: drive.\n";
                std::wcout << "Or: /mount 0 1 - Automatically mounts Partition 1 on Disk 0 to the first available drive letter.\n";
                return 1;
            }

            int diskNumber = std::stoi(argv[2]);
            int partitionNumber = std::stoi(argv[3]);

            // Check if the partition is already mounted
            std::wstring currentDriveLetter = IsPartitionAlreadyMounted(partitions, diskNumber, partitionNumber);
            if (!currentDriveLetter.empty()) {
                //if (currentDriveLetter == requestedDriveLetter) {
                std::wcout << L"Partition " << partitionNumber << L" on Disk " << diskNumber << L" is already mounted to " << currentDriveLetter << std::endl;
                return 0;
                //}
            }

            std::wstring requestedDriveLetter;

            // Check if the user provided a drive letter
            if (argc >= 5) {
                requestedDriveLetter = argv[4][0];  // Convert char to wchar_t
                requestedDriveLetter += L":\\";      // Append backslash as required by Windows API
            }
            else {
                // No drive letter provided; find the first available drive letter
                auto availableLetters = GetAvailableDriveLetters();
                if (!availableLetters.empty()) {
                    requestedDriveLetter = availableLetters.front();
                    std::wcout << L"No drive letter specified. Automatically mounting to " << requestedDriveLetter << std::endl;
                }
                else {
                    std::wcout << L"No available drive letters found." << std::endl;
                    return 1;
                }
            }

            if (MountPartition(diskNumber, partitionNumber, requestedDriveLetter)) {
                std::wcout << L"Successfully mounted Disk " << diskNumber << L", Partition " << partitionNumber << L" to drive letter " << requestedDriveLetter << std::endl;
            }
            else {
                std::wcout << L"Failed to mount Disk " << diskNumber << L", Partition " << partitionNumber << L" to drive letter " << requestedDriveLetter << std::endl;
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

                if (UnMountPartitionByDriveLetter(driveLetter.c_str())) {
                    std::wcout << "Successuflly unmounted partition assigned to drive letter " << driveLetterChar << ":\\" << std::endl;
                }
                else {
                    std::wcout << "Failed to unmount partition assigned to drive letter " << driveLetterChar << ":\\" << std::endl;
                }
                
            }
            else {
                // Unmount by disk number and partition number
                int diskNumber = std::stoi(target);
                int partitionNumber = argc > 3 ? std::stoi(argv[3]) : 0; // Default to 0 if not provided

                if (UnMountPartition(partitions, diskNumber, partitionNumber)) {
                    std::wcout << "Successfully unmounted Disk " << diskNumber << ", Partition " << partitionNumber << std::endl;
                }
                else {
                    std::wcout << "Failed to unmount Disk " << diskNumber << ", Partition " << partitionNumber << std::endl;
                }
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

            return 0;
        }
        else if (command == "/setlabel") {
            // Handle the /setlabel command
            if (argc < 5) {
                std::wcout << L"Usage: /setlabel <disk_number> <partition_number> <new_label>\n";
                std::wcout << L"Example: /setlabel 0 1 NewLabel - Sets 'NewLabel' as the label for Partition 1 on Disk 0.\n";
                return 1;
            }

            int diskNumber = std::stoi(argv[2]);
            int partitionNumber = std::stoi(argv[3]);
            std::string newLabelUtf8 = argv[4];

            // Convert UTF-8 to wide string
            std::wstring newLabel(newLabelUtf8.begin(), newLabelUtf8.end());

            if (SetLabel(partitions, diskNumber, partitionNumber, newLabel)) {
                std::wcout << L"Label set to '" << newLabel << L"' for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
            }
            else {
                std::wcout << L"Failed to set label for Disk " << diskNumber << L", Partition " << partitionNumber << std::endl;
            }

            return 0;
        }
        else {
            std::wcout << "Unknown command: " << std::wstring(command.begin(), command.end()) << "\n";
            std::wcout << "Use /? or /help for a list of available commands.\n";
            return 1;
        }
    }


    // Print header
    std::wcout << L"\n|  Disk   "
        << L"|  Partition  "
        << L"| Style "
        << L"|    Size    "
        << L"|     Type      "
        << L"| System "
        << L"| Ltr "
        << L"|         Label          |";
    std::wcout << std::endl;

    // Add an empty line as a separator
    std::wcout << L" --------- ------------- ------- ------------ --------------- -------- ----- ------------------------" << std::endl;

    for (const auto& partition : partitions) {
        std::wcout << FormatColumn(L"| Disk " + std::to_wstring(partition.DiskNumber), 9, Align::Left) << L" "
            << FormatColumn(L"| Partition " + std::to_wstring(partition.PartitionNumber), 13, Align::Left) << L" "
            << L"| " << FormatColumn(PartitionStyleToWString(partition.PartitionStyle), 5, Align::Left) << L" "
            << L"| " << FormatSize(partition.PartitionLength) << L" "
            << L"| " << FormatColumn(partition.PartitionType, 13, Align::Left) << L" "
            << L"| " << FormatColumn(partition.FileSystem, 7, Align::Left)
            << L"| " << FormatColumn(FormatLetter(partition.DriveLetter), 3, Align::Left) << L" "
            //<< L"| " << partition.MountPointName
            << L"| " << FormatColumn(ReplaceSpecialCharacters(partition.VolumeName), 22, Align::Left) << L" |";

        std::wcout << std::endl;
    }

    return 0;
}
