## microFAT

FAT32 file system driver for embedded applications. Designed to be small and use little RAM.

An example usage can be found in `fattest.c`. This library is written for the [fpdoom](https://github.com/ilyakurdyukov/fpdoom) project.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, USE AT YOUR OWN RISK!

#### Features supported

* open existing file
* read long filename
* create file/directory with short 8.3 filename
* delete file or empty directory
* read
* write
* set file position
* truncate

#### Not implemented

- documentation
- create long filename
- file time updates
- multithreading use
- sync handles to the same file

