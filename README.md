# CIAngel

**Place wings.json file in sd:/CIAngel/ - in CIAngel, press L to enter search mode, type name of game, then select the result's number you want**

wings.json file can **NOT** be found here, sorry.
It's early and experiemental at the moment, this will be improved.
-----------------------------

Now we can build CIAs on the 3DS!
Using a Title ID and an encrypted title key, a GOOD CIAs will be produced that can be redownloaded from eshop and updated from eshop if new content comes out. These CIAs will not interfere with content from eshop.

Many thanks for machinamentum and FirmwareDownloader! Thanks to make_cdn_cia!

License is GPL to comply with the usage of make_cdn_cia code.


This will be improved, updated, I look forward to people contributing to this project!


CIAngel can read a text file (sd:/CIAngel/input.txt) that has 2 lines.

The first line must be the title id.
The second line must be the encrypted title key.

CIAngel has a keyboard (HBKBLib) that can be used to enter a title id and key on the device!

# Building
CIAngel has a few dependencies required to build, as well as a git submodule that will need to be fetched.

When initially fetching the project, the easiest way to get the code and submodules for building is the following:

`git clone --recursive https://github.com/llakssz/CIAngel.git`

If you have already checked out the code without the submodules, you can run the following command in the project directory to fetch the submodule data:

`git submodule update --init --recursive`

CIAngel depends on the following projects:
- The latest version of [libctru](https://github.com/smealum/ctrulib)
- The latest version of [hbkb](https://gbatemp.net/threads/hbkblib-a-3ds-keyboard-library.397568/)

## Building and installing HBKB
HBKB doesn't currently have a 'make install' command, and has to be manually installed once built.

1. You will need to modify the Makefile to remove the DEVKITARM define at the top
2. Once you have run 'make', you will need to copy the resulting lib and header to your DEVKITARM directory:

   `cp hbkb/lib/libhbkb.a $DEVKITPRO/libctru/lib`  
   `cp hbkb_include_header/hbkb.h $DEVKITPRO/libctru/include/`  
3. Hopefully CIAngel now builds for you!
