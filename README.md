# ListDisk

 This tool provides detailed information about connected physical disks,
 including their partitions and volumes.

 Features include mounting/unmounting partitions, setting partition labels,
 and modifying MBR partition attributes.

 Available Commands:
   /mount             - Mounts a partition to a specified drive letter.
   /unmount           - Unmounts a specified partition or drive letter.
   /setlabel          - Sets a new label for a specified partition.
   /setactive         - Sets or clears the bootable flag of a partition.
   /settype           - Changes the MBR type of a partition.

 Usage Examples:
   /mount 0 1 U       - Mounts Partition 1 on Disk 0 as U: drive.
   /unmount 0 1       - Unmounts Partition 1 on Disk 0.
   /unmount U         - Unmounts U: drive.
   /setLabel 0 1 Lbl  - Sets 'Lbl' as the label for Partition 1 on Disk 0.
   /setactive 0 1     - Marks Partition 1 on Disk 0 as bootable.
   /setactive 0 1 80  - Sets the boot flag for Partition 1 on Disk 0.
   /setactive 0 1 00  - Clears the boot flag for Partition 1 on Disk 0.
   /settype 0 1 0x17  - Sets Partition 1 on Disk 0 to type NTFS Hidden (0x17).
   /settype 0 1 0x07  - Sets Partition 1 on Disk 0 to type NTFS (0x07).

 Note: Use this utility with caution. Incorrect usage may affect data integrity.

