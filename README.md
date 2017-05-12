# CIAngel


Now we can get games directly on the 3DS!
Using a Title ID and an encrypted title key, or searching for a title by name, GOOD CIAs will be produced or directly installed that can be redownloaded from eshop and updated from eshop if new content comes out. These CIAs will not interfere with content from eshop.

You can choose to create a CIA, install the game directly, or install only the ticket.



Many thanks for machinamentum and FirmwareDownloader! Thanks to make_cdn_cia!

License is GPL to comply with the usage of make_cdn_cia code.


This will be improved, updated, I look forward to people contributing to this project!

# Latest Version
You can scan the following QRCode in FBI to install the latest version of CIAngel, this will always point at the latest release's CIAngel.cia file:  
![CIAngel.cia](https://thedgtl.net/3ds/CIAngel.php?cachebust=2)

# Usage
## Search by name
CIAngel utilizes HBKBlib to search for titles by name. Data is read from /CIAngel/wings.json (Which is downloaded automatically on first launch) to search for the name entered.

## Download queue
When viewing the search result list, you can press X to add the title to the download queue. Selecting "Process download queue" will allow you to then download or install all of the queued titles one after another. This uses the currently selected download/install mode.

## Input.txt support
CIAngel can read a text file (sd:/CIAngel/input.txt) that has 2 lines.

The first line must be the title id.
The second line must be the encrypted title key.

# Thanks!
CIAngel has been developed with many contributions from the community, both ideas and code contributions. Many thanks to the following (non-exhaustive) list of contributors!
* Cearp
* Drakia
* superbudvar
* mysamdog
* cerea1killer
* machinamentum for the original FirmwareDownload code
* Steveice10 for many insights into CIA handling in FBI

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
