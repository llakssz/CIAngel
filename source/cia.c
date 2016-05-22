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
#include "lib.h"
#include "cia.h"
#include <3ds.h>
#define TRUE 1
#define FALSE 0

int generate_cia(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	write_cia_header(tmd_context,tik_context,output);
	write_cert_chain(tmd_context,tik_context,output);
	write_tik(tmd_context,tik_context,output);
	write_tmd(tmd_context,tik_context,output);
	Result res = write_content(tmd_context,tik_context,output);
	fclose(output);
	fclose(tik_context.tik);
	fclose(tmd_context.tmd);
	free(tmd_context.content_struct);
	
	return res;
}

int install_cia(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context)
{
	Handle handle;
	Result res;
	u64 titleId = get_title_id(tmd_context);
	FS_MediaType dest = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

	// Make sure this isn't a N3DS only title being installed on an O3DS
	u8 n3ds = false;
	if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds)
	{
		// 28 bits shift = 2 is a system title for N3ds
		// 24 bits shift = F is a N3DS exclusive game (Seems to always have 0xF7 in the titleid)
		if (((titleId >> 28) & 0xF) == 2 || ((titleId >> 24) & 0xF) == 0xF)
		{
			printf("Title requires a N3DS.\n");
			return -1;
		}
	}

	// Remove the ticket and title content, incase a bad one already exists on the system
	AM_DeleteTicket(titleId);
	AM_DeleteTitle(dest, titleId);

	res = AM_StartCiaInstall(dest, &handle);
	if (R_FAILED(res))
	{
		printf("Error starting CIA install: %ld.\n", res);
		return res;
	}

	u32 offset = 0;

	install_cia_header(tmd_context, tik_context, &offset, handle);
	install_cert_chain(tmd_context, tik_context, &offset, handle);
	install_tik(tmd_context, tik_context, &offset, handle);
	install_tmd(tmd_context, tik_context, &offset, handle);
	res = install_content(tmd_context, tik_context, &offset, handle);
	fclose(tik_context.tik);
	fclose(tmd_context.tmd);
	free(tmd_context.content_struct);

	if (R_FAILED(res))
	{
		printf("Error installing CIA.\n");
		AM_CancelCIAInstall(handle);
		return res;
	}

	res = AM_FinishCiaInstall(handle);
	if (R_FAILED(res))
	{
		printf("Error finishing CIA install.\n");
	}

	return res;
}

TIK_CONTEXT process_tik(FILE *tik)
{
	TIK_CONTEXT tik_context;
	memset(&tik_context,0x0,sizeof(tik_context));
	
	tik_context.tik = tik;
	
	u32 sig_size = get_sig_size(0x0,tik);
	if(sig_size == ERR_UNRECOGNISED_SIG){
		printf("[!] The CETK signature could not be recognised\n");
		tik_context.result = ERR_UNRECOGNISED_SIG;
		return tik_context;
	}
	
	TIK_STRUCT tik_struct = get_tik_struct(sig_size,tik);
	tik_context.tik_size = get_tik_size(sig_size);
	tik_context.title_version = u8_to_u16(tik_struct.title_version,BIG_ENDIAN);
	
	if(tik_context.tik_size == ERR_UNRECOGNISED_SIG){
		tik_context.result = ERR_UNRECOGNISED_SIG;
		return tik_context;
	}
	
	tik_context.cert_offset[0] = tik_context.tik_size;
	tik_context.cert_size[0] = get_cert_size(tik_context.tik_size,tik);
	tik_context.cert_offset[1] = tik_context.tik_size + tik_context.cert_size[0];
	tik_context.cert_size[1] = get_cert_size(tik_context.cert_offset[1],tik);
	
	if(tik_context.cert_size[0] == ERR_UNRECOGNISED_SIG || tik_context.cert_size[1] == ERR_UNRECOGNISED_SIG){
		printf("[!] One or both of the signatures in the CETK 'Cert Chain' are unrecognised\n");
		tik_context.result = ERR_UNRECOGNISED_SIG;
		return tik_context;
	}
	memcpy(tik_context.title_id,tik_struct.title_id,8);
	
	//printf("[+] CETK Title ID: "); u8_hex_print_be(tik_context.title_id,0x8); printf("\n");
	//printf("[+] CETK Size:     0x%x\n",tik_context.tik_size);
	//printf("[+] CERT Size:     0x%x\n",tik_context.cert_size);
	
	return tik_context;
}

TMD_CONTEXT process_tmd(FILE *tmd)
{
	TMD_CONTEXT tmd_context;
	memset(&tmd_context,0x0,sizeof(tmd_context));
	
	tmd_context.tmd = tmd;
	
	u32 sig_size = get_sig_size(0x0,tmd);
	if(sig_size == ERR_UNRECOGNISED_SIG){
		printf("[!] The TMD signature could not be recognised\n");
		tmd_context.result = ERR_UNRECOGNISED_SIG;
		return tmd_context;
	}
	
	
	TMD_STRUCT tmd_struct = get_tmd_struct(sig_size,tmd);
	tmd_context.content_count = u8_to_u16(tmd_struct.content_count,BIG_ENDIAN);
	tmd_context.tmd_size = get_tmd_size(sig_size,tmd_context.content_count);
	tmd_context.title_version = u8_to_u16(tmd_struct.title_version,BIG_ENDIAN);
	
	tmd_context.cert_offset[0] = tmd_context.tmd_size;
	tmd_context.cert_size[0] = get_cert_size(tmd_context.tmd_size,tmd);
	tmd_context.cert_offset[1] = tmd_context.tmd_size + tmd_context.cert_size[0];
	tmd_context.cert_size[1] = get_cert_size(tmd_context.cert_offset[1],tmd);
	
	if(tmd_context.cert_size[0] == ERR_UNRECOGNISED_SIG || tmd_context.cert_size[1] == ERR_UNRECOGNISED_SIG){
		printf("[!] One or both of the signatures in the TMD 'Cert Chain' are unrecognised\n");
		tmd_context.result = ERR_UNRECOGNISED_SIG;
		return tmd_context;
	}
	memcpy(tmd_context.title_id,tmd_struct.title_id,8);
	
	tmd_context.content_struct = malloc(sizeof(TMD_CONTENT_CHUNK_STRUCT)*tmd_context.content_count);
	for(u8 i = 0; i < tmd_context.content_count; i++){
		tmd_context.content_struct[i] = get_tmd_content_struct(sig_size,i,tmd);
	}
	return tmd_context;
}

CIA_HEADER set_cia_header(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context)
{
	CIA_HEADER cia_header;
	memset(&cia_header,0,sizeof(cia_header));
	cia_header.header_size = sizeof(CIA_HEADER);
	cia_header.type = 0;
	cia_header.version = 0;
	cia_header.cert_size = get_total_cert_size(tmd_context,tik_context);
	cia_header.tik_size = tik_context.tik_size;
	cia_header.tmd_size = tmd_context.tmd_size;
	cia_header.meta_size = 0;
	cia_header.content_size = get_content_size(tmd_context);
	for(int i = 0; i < tmd_context.content_count; i++) {
		u16 index = u8_to_u16(tmd_context.content_struct[i].content_index, BIG_ENDIAN);
		cia_header.content_index[index / 8] |= 0x80 >> (index & 7);
	}
	return cia_header;
}

u32 get_tik_size(u32 sig_size)
{
	return (0x4 + sig_size + sizeof(TIK_STRUCT));
}

u32 get_tmd_size(u32 sig_size, u16 content_count)
{
	return (0x4 + sig_size + sizeof(TMD_STRUCT) + sizeof(TMD_CONTENT_CHUNK_STRUCT)*content_count);
}

u32 get_sig_size(u32 offset, FILE *file)
{
	fseek(file,offset,SEEK_SET);
	u32 sig_type;
	fread(&sig_type,0x4,1,file);
	switch(sig_type){
		/**
		case(RSA_4096_SHA1): return 0x200;
		case(RSA_2048_SHA1): return 0x100;
		case(Elliptic_Curve_0): return 0x3C;
		**/
		case(RSA_4096_SHA256): return 0x200;
		case(RSA_2048_SHA256): return 0x100;
		//case(Elliptic_Curve_1): return 0x3C;
	}
	return ERR_UNRECOGNISED_SIG;
}

u32 get_cert_size(u32 offset, FILE *file)
{
	u32 sig_size = get_sig_size(offset,file);
	if(sig_size == ERR_UNRECOGNISED_SIG)
		return ERR_UNRECOGNISED_SIG;
	return (0x4+sig_size+sizeof(CERT_2048KEY_DATA_STRUCT));
}

u32 get_total_cert_size(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context)
{
	return (tik_context.cert_size[1] + tik_context.cert_size[0] + tmd_context.cert_size[0]);
}

u64 get_content_size(TMD_CONTEXT tmd_context)
{
	u64 content_size = 0x0;
	for(int i = 0; i < tmd_context.content_count; i++)
		content_size += read_content_size(tmd_context.content_struct[i]);
	return content_size;
}

u64 read_content_size(TMD_CONTENT_CHUNK_STRUCT content_struct)
{
	return u8_to_u64(content_struct.content_size,BIG_ENDIAN);
}

u32 get_content_id(TMD_CONTENT_CHUNK_STRUCT content_struct)
{
	return u8_to_u32(content_struct.content_id,BIG_ENDIAN);
}

u64 get_title_id(TMD_CONTEXT content_struct)
{
	return u8_to_u64(content_struct.title_id,BIG_ENDIAN);
}

int write_cia_header(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	CIA_HEADER cia_header = set_cia_header(tmd_context,tik_context);
	fseek(output,0x0,SEEK_SET);
	fwrite(&cia_header,sizeof(cia_header),1,output);

	// Make sure we end on a 64-byte boundry
	write_align_padding(output, 64);

	return 0;
}

int write_cert_chain(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	u8 cert[0x1000];
	//The order of Certs in CIA goes, Root Cert, Cetk Cert, TMD Cert. In CDN format each file has it's own cert followed by a Root cert
	
	//Taking Root Cert from Cetk Cert chain(can be taken from TMD Cert Chain too)
	memset(cert,0x0,tik_context.cert_size[1]);
	fseek(tik_context.tik,tik_context.cert_offset[1],SEEK_SET);
	fread(&cert,tik_context.cert_size[1],1,tik_context.tik);
	fwrite(&cert,tik_context.cert_size[1],1,output);
	
	//Writing Cetk Cert
	memset(cert,0x0,tik_context.cert_size[0]);
	fseek(tik_context.tik,tik_context.cert_offset[0],SEEK_SET);
	fread(&cert,tik_context.cert_size[0],1,tik_context.tik);
	fwrite(&cert,tik_context.cert_size[0],1,output);
	
	//Writing TMD Cert
	memset(cert,0x0,tmd_context.cert_size[0]);
	fseek(tmd_context.tmd,tmd_context.cert_offset[0],SEEK_SET);
	fread(&cert,tmd_context.cert_size[0],1,tmd_context.tmd);
	fwrite(&cert,tmd_context.cert_size[0],1,output);

	// Make sure we end on a 64-byte boundry
	write_align_padding(output, 64);
	
	return 0;
}

int write_tik(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	u8 tik[tik_context.tik_size];
	
	memset(tik,0x0,tik_context.tik_size);
	fseek(tik_context.tik,0x0,SEEK_SET);
	fread(&tik,tik_context.tik_size,1,tik_context.tik);
	fwrite(&tik,tik_context.tik_size,1,output);
	
	// Make sure we end on a 64-byte boundry
	write_align_padding(output, 64);

	return 0;
}

int write_tmd(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	u8 tmd[tmd_context.tmd_size];
	memset(tmd,0x0,tmd_context.tmd_size);
	fseek(tmd_context.tmd,0x0,SEEK_SET);
	fread(&tmd,tmd_context.tmd_size,1,tmd_context.tmd);
	fwrite(&tmd,tmd_context.tmd_size,1,output);
	
	// Make sure we end on a 64-byte boundry
	write_align_padding(output, 64);

	return 0;
}

int write_content(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, FILE *output)
{
	Result res = 0;
	for(int i = 0; i < tmd_context.content_count; i++) {
		printf("Downloading content %d of %d\n", i + 1, tmd_context.content_count);
		char content_id[16];
		char title_id[32];
		sprintf(content_id,"%08lx",get_content_id(tmd_context.content_struct[i]));
		sprintf(title_id,"%016llx",get_title_id(tmd_context));

		char *url = malloc(48 + strlen(NUS_URL) + 1);
		sprintf(url, "%s%s/%s", NUS_URL, title_id, content_id);
		res = DownloadFile(url, output, true);
		free(url);

		if (R_FAILED(res))
		{
			break;
		}

	}
	return res;
}


int install_cia_header(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, u32* offset, Handle handle)
{
	u32 bytesWritten;
	CIA_HEADER cia_header = set_cia_header(tmd_context,tik_context);

	FSFILE_Write(handle, &bytesWritten, *offset, &cia_header, sizeof(cia_header), 0);
	*offset += bytesWritten;

	// Make sure we end on a 64-byte boundry
	install_write_align_padding(handle, offset, 64);

	return 0;
}

int install_cert_chain(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, u32* offset, Handle handle)
{
	u32 bytesWritten;
	u8 cert[0x1000];
	//The order of Certs in CIA goes, Root Cert, Cetk Cert, TMD Cert. In CDN format each file has it's own cert followed by a Root cert
	
	//Taking Root Cert from Cetk Cert chain(can be taken from TMD Cert Chain too)
	memset(cert,0x0,tik_context.cert_size[1]);
	fseek(tik_context.tik,tik_context.cert_offset[1],SEEK_SET);
	fread(&cert,tik_context.cert_size[1],1,tik_context.tik);
	FSFILE_Write(handle, &bytesWritten, *offset, &cert, tik_context.cert_size[1], 0);
	*offset += bytesWritten;
	
	//Writing Cetk Cert
	memset(cert,0x0,tik_context.cert_size[0]);
	fseek(tik_context.tik,tik_context.cert_offset[0],SEEK_SET);
	fread(&cert,tik_context.cert_size[0],1,tik_context.tik);
	FSFILE_Write(handle, &bytesWritten, *offset, &cert, tik_context.cert_size[0], 0);
	*offset += bytesWritten;
	
	//Writing TMD Cert
	memset(cert,0x0,tmd_context.cert_size[0]);
	fseek(tmd_context.tmd,tmd_context.cert_offset[0],SEEK_SET);
	fread(&cert,tmd_context.cert_size[0],1,tmd_context.tmd);
	FSFILE_Write(handle, &bytesWritten, *offset, &cert, tmd_context.cert_size[0], 0);
	*offset += bytesWritten;

	// Make sure we end on a 64-byte boundry
	install_write_align_padding(handle, offset, 64);
	
	return 0;
}

int install_tik(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, u32* offset, Handle handle)
{
	u32 bytesWritten;
	u8 tik[tik_context.tik_size];
	
	memset(tik,0x0,tik_context.tik_size);
	fseek(tik_context.tik,0x0,SEEK_SET);
	fread(&tik,tik_context.tik_size,1,tik_context.tik);
	FSFILE_Write(handle, &bytesWritten, *offset, &tik, tik_context.tik_size, 0);
	*offset += bytesWritten;
	
	// Make sure we end on a 64-byte boundry
	install_write_align_padding(handle, offset, 64);

	return 0;
}

int install_tmd(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, u32* offset, Handle handle)
{
	u32 bytesWritten;
	u8 tmd[tmd_context.tmd_size];
	memset(tmd,0x0,tmd_context.tmd_size);
	fseek(tmd_context.tmd,0x0,SEEK_SET);
	fread(&tmd,tmd_context.tmd_size,1,tmd_context.tmd);
	FSFILE_Write(handle, &bytesWritten, *offset, &tmd, tmd_context.tmd_size, 0);
	*offset += bytesWritten;
	
	// Make sure we end on a 64-byte boundry
	install_write_align_padding(handle, offset, 64);

	return 0;
}

Result install_content(TMD_CONTEXT tmd_context, TIK_CONTEXT tik_context, u32* offset, Handle handle)
{
	Result res = 0;
	for(int i = 0; i < tmd_context.content_count; i++) {
		printf("Installing content %d of %d\n", i + 1, tmd_context.content_count);
		char content_id[16];
		char title_id[32];
		sprintf(content_id,"%08lx",get_content_id(tmd_context.content_struct[i]));
		sprintf(title_id,"%016llx",get_title_id(tmd_context));

		char *url = malloc(48 + strlen(NUS_URL) + 1);
		sprintf(url, "%s%s/%s", NUS_URL, title_id, content_id);

		res = DownloadFileInstall(url, &handle, offset);
		free(url);

		if (R_FAILED(res))
		{
			break;
		}
	}

	return res;
}

void install_write_align_padding(Handle handle, u32* offset, size_t alignment)
{
	long int usedbytes = *offset & (alignment - 1);
	if (usedbytes)
	{
		u32 bytesWritten;
		// Create the padding strings
		long int padbytes = (alignment - usedbytes);
		char* pad = (char*)malloc(padbytes);
		memset(pad, 0, padbytes);

		// Write it, and increase the offset
		FSFILE_Write(handle, &bytesWritten, *offset, pad, padbytes, 0);
		*offset += bytesWritten;
		free(pad);
	}
}


TIK_STRUCT get_tik_struct(u32 sig_size, FILE *tik)
{
	TIK_STRUCT tik_struct;
	fseek(tik,(0x4+sig_size),SEEK_SET);
	fread(&tik_struct,sizeof(tik_struct),1,tik);
	return tik_struct;
}

TMD_STRUCT get_tmd_struct(u32 sig_size, FILE *tmd)
{
	TMD_STRUCT tmd_struct;
	fseek(tmd,(0x4+sig_size),SEEK_SET);
	fread(&tmd_struct,sizeof(tmd_struct),1,tmd);
	return tmd_struct;
}

TMD_CONTENT_CHUNK_STRUCT get_tmd_content_struct(u32 sig_size, u8 index, FILE *tmd)
{
	fseek(tmd,(0x4+sig_size+sizeof(TMD_STRUCT)+sizeof(TMD_CONTENT_CHUNK_STRUCT)*index),SEEK_SET);
	TMD_CONTENT_CHUNK_STRUCT content_struct;
	fread(&content_struct,sizeof(content_struct),1,tmd);
	return content_struct;
}

void print_content_chunk_info(TMD_CONTENT_CHUNK_STRUCT content_struct)
{
	printf("\n[+] Content ID:    %08lx\n",u8_to_u32(content_struct.content_id,BIG_ENDIAN));
	printf("[+] Content Index: %d\n",u8_to_u16(content_struct.content_index,BIG_ENDIAN));
	printf("[+] Content Type:  %d\n",u8_to_u16(content_struct.content_type,BIG_ENDIAN));
	printf("[+] Content Size:  0x%llx\n",u8_to_u64(content_struct.content_size,BIG_ENDIAN));
	printf("[+] SHA-256 Hash:  "); u8_hex_print_be(content_struct.sha_256_hash,0x20); printf("\n");
}

int check_tid(u8 *tid_0, u8 *tid_1)
{
	for(int i = 0; i < 8; i++){
		if(tid_0[i] != tid_1[i])
			return FALSE;
	}
	return TRUE;
}