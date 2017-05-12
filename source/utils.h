/**
Copyright 2013 3DSGuy

This file is part of make_cdn_cia.

make_cdn_cia is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

make_cdn_cia is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with make_cdn_cia.  If not, see <http://www.gnu.org/licenses/>.
**/
#include <3ds.h>

#define NUS_URL "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download/"
#define SEED_URL "https://kagiya-ctr.cdn.nintendo.net/title/"
#define JSON_URL "https://3ds.titlekeys.com/json_enc"

// Check for updates every 3 days automatically?
#define JSON_UPDATE_INTERVAL_IN_SECONDS (60 * 60 * 24 * 3)

#define JSON_TYPE_WINGS 1
#define JSON_TYPE_ONLINE 2

// C++ only code
#ifdef __cplusplus
#include <string>
std::string GetSerialType(std::string sSerial);
std::string upperCase(std::string input);
#endif

//MISC
#ifdef __cplusplus
extern "C" {
#endif
void char_to_int_array(unsigned char destination[], char source[], int size, u32 endianness, int base);
void endian_memcpy(u8 *destination, u8 *source, u32 size, u32 endianness);
void u8_hex_print_be(u8 *array, int len);
void u8_hex_print_le(u8 *array, int len);
u32 align_value(u32 value, u32 alignment);
void resolve_flag(unsigned char flag, unsigned char *flag_bool);
void resolve_flag_u16(u16 flag, unsigned char *flag_bool);
bool download_JSON();
bool check_JSON();
//IO Related
void PrintProgress(PrintConsole *console, u32 nSize, u32 nCurrent);
void WriteBuffer(void *buffer, u64 size, u64 offset, FILE *output);
void write_align_padding(FILE *output, size_t alignment);
u64 GetFileSize_u64(char *filename);
int TruncateFile_u64(char *filename, u64 filelen);
int fseek_64(FILE *fp, u64 file_pos, int whence);
int makedir(const char* dir);
char *getcwdir(char *buffer,int maxlen);
bool FileExists (const char *name);
Result DownloadFile(const char *url, FILE *os, bool bProgress);
Result DownloadFileInstall(const char *url, Handle *handle, u32* offset);
Result InstallSeed(u64 titleId, const void* seed);
//Data Size conversion
u16 u8_to_u16(u8 *value, u32 endianness);
u32 u8_to_u32(u8 *value, u32 endianness);
u64 u8_to_u64(u8 *value, u32 endianness);
int u16_to_u8(u8 *out_value, u16 in_value, u32 endianness);
int u32_to_u8(u8 *out_value, u32 in_value, u32 endianness);
int u64_to_u8(u8 *out_value, u64 in_value, u32 endianness);
//from ctrtool
void memdump(FILE* fout, const char* prefix, const u8* data, u32 size);
// HID related
u32 wait_key();
u32 wait_key_specific(const char* msg, u32 key);
// graphics functions
void clear_screen(gfxScreen_t screen);

#ifdef __cplusplus
}
#endif
