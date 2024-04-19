# ListDisk

This tool provides detailed information about connected physical disks, including their partitions and volumes.

## Features
- Mounting/unmounting partitions
- Setting partition labels
- Modifying MBR partition attributes

## Available Commands
- `/mount` - Mounts a partition to a specified drive letter.
- `/unmount` - Unmounts a specified partition or drive letter.
- `/setlabel` - Sets a new label for a specified partition.
- `/setactive` - Sets or clears the bootable flag of a partition.
- `/settype` - Changes the MBR type of a partition.

## Usage Examples
- `/mount 0 1 U` - Mounts Partition 1 on Disk 0 as U: drive.
- `/unmount 0 1` - Unmounts Partition 1 on Disk 0.
- `/unmount U` - Unmounts U: drive.
- `/setLabel 0 1 Lbl` - Sets 'Lbl' as the label for Partition 1 on Disk 0.
- `/setactive 0 1` - Marks Partition 1 on Disk 0 as bootable.
- `/setactive 0 1 80` - Sets the boot flag for Partition 1 on Disk 0.
- `/setactive 0 1 00` - Clears the boot flag for Partition 1 on Disk 0.
- `/settype 0 1 0x17` - Sets Partition 1 on Disk 0 to type NTFS Hidden (0x17).
- `/settype 0 1 0x07` - Sets Partition 1 on Disk 0 to type NTFS (0x07).

## Requirements
- **Operating System:** Windows XP and newer.
  - **Note for Windows XP Users:** This tool requires the installation of the Microsoft Visual C++ Redistributable. Please install it from [this link](https://github.com/abbodi1406/vcredist/releases/tag/v0.35.0).
- **Compatible with Windows Preinstallation Environment (WinPE).**
- **Place ListDisk in %WinDir%\system32 to make it easy accessible from commandline!**


> **Note:** Use this utility with caution. Incorrect usage may affect data integrity.

![image](https://github.com/GeorgeK1ng/ListDisk/assets/98261225/bcdf04ba-070b-4ec3-846f-9b1a5fcbc8ed)

![image](https://github.com/GeorgeK1ng/ListDisk/assets/98261225/af4299e5-412d-42e5-9cee-b111e785cc1b)

![image](https://github.com/GeorgeK1ng/ListDisk/assets/98261225/b850d681-b6da-4bb3-88a8-29ad4c1fd104)

![image](https://github.com/GeorgeK1ng/ListDisk/assets/98261225/038d9428-523d-4bb7-8e9d-c025e055a25e)

