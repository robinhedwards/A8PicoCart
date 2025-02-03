/**
 *    _   ___ ___ _       ___          _   
 *   /_\ ( _ ) _ (_)__ _ / __|__ _ _ _| |_ 
 *  / _ \/ _ \  _/ / _/_\ (__/ _` | '_|  _|
 * /_/ \_\___/_| |_\__\_/\___\__,_|_|  \__|
 *                                         
 * 
 * Atari 8-bit cartridge for Raspberry Pi Pico
 *
 * Robin Edwards 2023
 *
 * Needs to be a release NOT debug build for the cartridge emulation to work
 * 
 * Changes from UnoCart:
 * - Attempts to get S4/S5, RD4/RD5 MMU behaviour correct on 400/800
 *   https://forums.atariage.com/topic/241888-ultimate-cart-sd-multicart-technical-thread/page/10/#comment-4266797
 * - Adds 4k carts (CAR type 58)
 * - Adds Turbsoft carts (CAR types 50,51)
 * - Adds ATRAX 128k carts (CAR type 17)
 * - Adds Microcalc/Utracart (CAR type 52)
 * - Adds Standard 2k cars (CAR type 57)
 * - Adds Phoenix 8k cars (CAR type 39)
 * - Adds Blizzard 4k cars (CAR type 46)
 */

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "ff.h"
#include "fatfs_disk.h"
#include "hardware/clocks.h"

#define ALL_GPIO_MASK   	0x3FFFFFFF
#define ADDR_GPIO_MASK  	0x00001FFF
#define DATA_GPIO_MASK  	0x001FE000
#define CCTL_GPIO_MASK  	0x00200000  // gpio 21
#define PHI2_GPIO_MASK  	0x00400000  // gpio 22
#define RW_GPIO_MASK    	0x00800000  // gpio 23
#define S4_GPIO_MASK    	0x01000000  // gpio 24
#define S5_GPIO_MASK    	0x02000000  // gpio 25

#define S4_S5_GPIO_MASK 	0x03000000
#define CCTL_RW_GPIO_MASK 	0x00A00000

#define RD4_PIN         26
#define RD5_PIN         27

#define RD4_LOW             gpio_put(RD4_PIN, 0)
#define RD4_HIGH            gpio_put(RD4_PIN, 1)
#define RD5_LOW             gpio_put(RD5_PIN, 0)
#define RD5_HIGH            gpio_put(RD5_PIN, 1)
#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_GPIO_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_GPIO_MASK)

#include "rom.h"
#include "osrom.h"

unsigned char cart_ram[128*1024];
unsigned char cart_d5xx[256] = {0};
char errorBuf[40];

#define CART_CMD_OPEN_ITEM			0x00
#define CART_CMD_READ_CUR_DIR		0x01
#define CART_CMD_GET_DIR_ENTRY		0x02
#define CART_CMD_UP_DIR				0x03
#define CART_CMD_ROOT_DIR			0x04
#define CART_CMD_SEARCH				0x05
#define CART_CMD_LOAD_SOFT_OS		0x10
#define CART_CMD_SOFT_OS_CHUNK		0x11
#define CART_CMD_MOUNT_ATR			0x20	// unused, done automatically by firmware
#define CART_CMD_READ_ATR_SECTOR	0x21
#define CART_CMD_WRITE_ATR_SECTOR	0x22
#define CART_CMD_ATR_HEADER			0x23
#define CART_CMD_RESET_FLASH		0xF0
#define CART_CMD_NO_CART			0xFE
#define CART_CMD_ACTIVATE_CART  	0xFF

#define CART_TYPE_NONE				0
#define CART_TYPE_8K				1	// 8k
#define CART_TYPE_16K				2	// 16k
#define CART_TYPE_XEGS_32K			3	// 32k
#define CART_TYPE_XEGS_64K			4	// 64k
#define CART_TYPE_XEGS_128K			5	// 128k
#define CART_TYPE_SW_XEGS_32K		6	// 32k
#define CART_TYPE_SW_XEGS_64K		7	// 64k
#define CART_TYPE_SW_XEGS_128K		8	// 128k
#define CART_TYPE_MEGACART_16K		9	// 16k
#define CART_TYPE_MEGACART_32K		10	// 32k
#define CART_TYPE_MEGACART_64K		11	// 64k
#define CART_TYPE_MEGACART_128K		12	// 128k
#define CART_TYPE_BOUNTY_BOB		13	// 40k
#define CART_TYPE_ATARIMAX_1MBIT	14	// 128k
#define CART_TYPE_WILLIAMS_64K		15	// 32k/64k
#define CART_TYPE_OSS_16K_TYPE_B	16	// 16k
#define CART_TYPE_OSS_8K			17	// 8k
#define CART_TYPE_OSS_16K_034M		18	// 16k
#define CART_TYPE_OSS_16K_043M		19	// 16k
#define CART_TYPE_SIC_128K			20	// 128k
#define CART_TYPE_SDX_64K			21	// 64k
#define CART_TYPE_SDX_128K			22	// 128k
#define CART_TYPE_DIAMOND_64K		23	// 64k
#define CART_TYPE_EXPRESS_64K		24	// 64k
#define CART_TYPE_BLIZZARD_16K		25	// 16k
#define CART_TYPE_4K				26	// 4k
#define CART_TYPE_TURBOSOFT_64K		27	// 64k
#define CART_TYPE_TURBOSOFT_128K	28	// 128k
#define CART_TYPE_ATRAX_128K		29	// 128k
#define CART_TYPE_MICROCALC			30	// 32k
#define CART_TYPE_2K				31  // 2k
#define CART_TYPE_PHOENIX_8K		32	// 8k
#define CART_TYPE_BLIZZARD_4K		33	// 4k
#define CART_TYPE_ATR				254
#define CART_TYPE_XEX				255

typedef struct {
	char isDir;
	char filename[13];
	char long_filename[32];
	char full_path[210];
} DIR_ENTRY;	// 256 bytes = 256 entries in 64k

int num_dir_entries = 0; // how many entries in the current directory

int entry_compare(const void* p1, const void* p2)
{
	DIR_ENTRY* e1 = (DIR_ENTRY*)p1;
	DIR_ENTRY* e2 = (DIR_ENTRY*)p2;
	if (e1->isDir && !e2->isDir) return -1;
	else if (!e1->isDir && e2->isDir) return 1;
	else return strcasecmp(e1->long_filename, e2->long_filename);
}

char *get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int is_valid_file(char *filename) {
	char *ext = get_filename_ext(filename);
	if (strcasecmp(ext, "CAR") == 0 || strcasecmp(ext, "ROM") == 0
			|| strcasecmp(ext, "XEX") == 0 || strcasecmp(ext, "ATR") == 0)
		return 1;
	return 0;
}

FILINFO fno;
char search_fname[FF_LFN_BUF + 1];

// polyfill :-)
char *stristr(const char *str, const char *strSearch) {
    char *sors, *subs, *res = NULL;
    if ((sors = strdup (str)) != NULL) {
        if ((subs = strdup (strSearch)) != NULL) {
            res = strstr (strlwr (sors), strlwr (subs));
            if (res != NULL)
                res = (char*)str + (res - sors);
            free (subs);
        }
        free (sors);
    }
    return res;
}

int scan_files(char *path, char *search)
{
    FRESULT res;
    DIR dir;
    UINT i;

	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		for (;;) {
			if (num_dir_entries == 255) break;
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;
			if (fno.fattrib & (AM_HID | AM_SYS)) continue;
			if (fno.fattrib & AM_DIR) {
				i = strlen(path);
				strcat(path, "/");
				if (fno.altname[0])	// no altname when lfn is 8.3
					strcat(path, fno.altname);
				else
					strcat(path, fno.fname);
				if (strlen(path) >= 210) continue;	// no more room for path in DIR_ENTRY
				res = scan_files(path, search);
				if (res != FR_OK) break;
				path[i] = 0;
			}
			else if (is_valid_file(fno.fname))
			{
				char *match = stristr(fno.fname, search);
				if (match) {
					DIR_ENTRY *dst = (DIR_ENTRY *)&cart_ram[0];
					dst += num_dir_entries;
					// fill out a record
					dst->isDir = (match == fno.fname) ? 1 : 0;	// use this for a "score"
					strncpy(dst->long_filename, fno.fname, 31);
					dst->long_filename[31] = 0;
					// 8.3 name
					if (fno.altname[0])
						strcpy(dst->filename, fno.altname);
					else {	// no altname when lfn is 8.3
						strncpy(dst->filename, fno.fname, 12);
						dst->filename[12] = 0;
					}
					// full path for search results
					strcpy(dst->full_path, path);

					num_dir_entries++;
				}
			}
		}
		f_closedir(&dir);
	}
	return res;
}

int search_directory(char *path, char *search) {
	char pathBuf[256];
	strcpy(pathBuf, path);
	num_dir_entries = 0;
	int i;
	FATFS FatFs;
	if (f_mount(&FatFs, "", 1) == FR_OK) {
		if (scan_files(pathBuf, search) == FR_OK) {
			// sort by score, name
			qsort((DIR_ENTRY *)&cart_ram[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
			DIR_ENTRY *dst = (DIR_ENTRY *)&cart_ram[0];
			// reset the "scores" back to 0
			for (i=0; i<num_dir_entries; i++)
				dst[i].isDir = 0;
			return 1;

		}
	}
	strcpy(errorBuf, "Problem searching flash");
	return 0;
}

int read_directory(char *path) {
	int ret = 0;
	num_dir_entries = 0;
	DIR_ENTRY *dst = (DIR_ENTRY *)&cart_ram[0];

    if (!fatfs_is_mounted())
       mount_fatfs_disk();

	FATFS FatFs;
	if (f_mount(&FatFs, "", 1) == FR_OK) {
		DIR dir;
		if (f_opendir(&dir, path) == FR_OK) {
			while (num_dir_entries < 255) {
				if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
					break;
				if (fno.fattrib & (AM_HID | AM_SYS))
					continue;
				dst->isDir = fno.fattrib & AM_DIR ? 1 : 0;
				if (!dst->isDir)
					if (!is_valid_file(fno.fname)) continue;
				// copy file record to first ram block
				// long file name
				strncpy(dst->long_filename, fno.fname, 31);
				dst->long_filename[31] = 0;
				// 8.3 name
				if (fno.altname[0])
		            strcpy(dst->filename, fno.altname);
				else {	// no altname when lfn is 8.3
					strncpy(dst->filename, fno.fname, 12);
					dst->filename[12] = 0;
				}
				dst->full_path[0] = 0; // path only for search results
	            dst++;
				num_dir_entries++;
			}
			f_closedir(&dir);
		}
		else
			strcpy(errorBuf, "Can't read directory");
		f_mount(0, "", 1);
		qsort((DIR_ENTRY *)&cart_ram[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
		ret = 1;
	}
	else
		strcpy(errorBuf, "Can't read flash memory");
	return ret;
}

/* ATR Handling */

// ATR format
#define ATR_HEADER_SIZE 16
#define ATR_SIGNATURE 0x0296
typedef struct {
  uint16_t signature;
  uint16_t pars;
  uint16_t secSize;
  uint16_t parsHigh;
  uint8_t flags;
  uint16_t protInfo;
  uint8_t unused[5];
} ATRHeader;

typedef struct {
	char path[256];
	ATRHeader atrHeader;
	int	filesize;
	FIL fil;
} MountedATR;

MountedATR mountedATRs[1] = {0};

FATFS FatFs;
int doneFatFsInit = 0;

int mount_atr(char *filename) {
	// returns 0 for success or error code
	// 1 = no media, 2 = no file, 3 = bad atr
	if (!doneFatFsInit) {
		if (f_mount(&FatFs, "", 1) != FR_OK)
			return 1;
		doneFatFsInit = 1;
	}
	MountedATR *mountedATR = &mountedATRs[0];
	if (f_open(&mountedATR->fil, filename, FA_READ|FA_WRITE) != FR_OK)
		return 2;
	UINT br;
	if (f_read(&mountedATR->fil, &mountedATR->atrHeader, ATR_HEADER_SIZE, &br) != FR_OK || br != ATR_HEADER_SIZE ||
			mountedATR->atrHeader.signature != ATR_SIGNATURE) {
		f_close(&mountedATR->fil);
		return 3;
	}
	// success
	strcpy(mountedATR->path, filename);
	mountedATR->filesize = f_size(&mountedATR->fil);
	return 0;
}

int read_atr_sector(uint16_t sector, uint8_t page, uint8_t *buf) {
	// returns 0 for success or error code
	// 1 = no ATR mounted, 2 = invalid sector
	MountedATR *mountedATR = &mountedATRs[0];
	if (!mountedATR->path[0]) return 1;
	if (sector == 0) return 2;

	int offset = ATR_HEADER_SIZE;
	// first 3 sectors are always 128 bytes
	if (sector <=3)
		offset += (sector - 1) * 128;
	else
		offset += (3 * 128) + ((sector - 4) * mountedATR->atrHeader.secSize) + (page * 128);
	// check we're not reading beyond the end of the file..
	if (offset > (mountedATR->filesize - 128)) {
		memset(buf, 0 , 128);	// return blank sector?
		return 0;
	}
	UINT br;
	if (f_lseek(&mountedATR->fil, offset) != FR_OK || f_read(&mountedATR->fil, buf, 128, &br) != FR_OK || br != 128)
		return 2;
	return 0;
}

int write_atr_sector(uint16_t sector, uint8_t page, uint8_t *buf) {
	// returns 0 for success or error code
	// 1 = no ATR mounted, 2 = write error
	MountedATR *mountedATR = &mountedATRs[0];
	if (!mountedATR->path[0]) return 1;
	if (sector == 0) return 2;

	int offset = ATR_HEADER_SIZE;
	// first 3 sectors are always 128 bytes
	if (sector <=3)
		offset += (sector - 1) * 128;
	else
		offset += (3 * 128) + ((sector - 4) * mountedATR->atrHeader.secSize) + (page * 128);
	// check we're not writing beyond the end of the file..
	if (offset > (mountedATR->filesize - 128))
		return 2;
	UINT bw;
	if (f_lseek(&mountedATR->fil, offset) != FR_OK || f_write(&mountedATR->fil, buf, 128, &bw) != FR_OK || f_sync(&mountedATR->fil) != FR_OK  || bw != 128)
		return 2;
	return 0;
}

/* CARTRIDGE/XEX HANDLING */

int load_file(char *filename) {
	FATFS FatFs;
	int cart_type = CART_TYPE_NONE;
	int car_file = 0, xex_file = 0, expectedSize = 0;
	unsigned char carFileHeader[16];
	UINT br, size = 0;

	if (strncasecmp(filename+strlen(filename)-4, ".CAR", 4) == 0)
		car_file = 1;
	if (strncasecmp(filename+strlen(filename)-4, ".XEX", 4) == 0)
		xex_file = 1;

	if (f_mount(&FatFs, "", 1) != FR_OK) {
		strcpy(errorBuf, "Can't read flash memory");
		return 0;
	}
	FIL fil;
	if (f_open(&fil, filename, FA_READ) != FR_OK) {
		strcpy(errorBuf, "Can't open file");
		goto cleanup;
	}

	// read the .CAR file header?
	if (car_file) {
		if (f_read(&fil, carFileHeader, 16, &br) != FR_OK || br != 16) {
			strcpy(errorBuf, "Bad CAR file");
			goto closefile;
		}
		int car_type = carFileHeader[7];
		if (car_type == 1)			{ cart_type = CART_TYPE_8K; expectedSize = 8192; }
		else if (car_type == 2)		{ cart_type = CART_TYPE_16K; expectedSize = 16384; }
		else if (car_type == 3) 	{ cart_type = CART_TYPE_OSS_16K_034M; expectedSize = 16384; }
		else if (car_type == 8)		{ cart_type = CART_TYPE_WILLIAMS_64K; expectedSize = 65536; }
		else if (car_type == 9)		{ cart_type = CART_TYPE_EXPRESS_64K; expectedSize = 65536; }
		else if (car_type == 10)	{ cart_type = CART_TYPE_DIAMOND_64K; expectedSize = 65536; }
		else if (car_type == 11)	{ cart_type = CART_TYPE_SDX_64K; expectedSize = 65536; }
		else if (car_type == 12) 	{ cart_type = CART_TYPE_XEGS_32K; expectedSize = 32768; }
		else if (car_type == 13) 	{ cart_type = CART_TYPE_XEGS_64K; expectedSize = 65536; }
		else if (car_type == 14) 	{ cart_type = CART_TYPE_XEGS_128K; expectedSize = 131072; }
		else if (car_type == 15) 	{ cart_type = CART_TYPE_OSS_16K_TYPE_B; expectedSize = 16384; }
		else if (car_type == 17) 	{ cart_type = CART_TYPE_ATRAX_128K; expectedSize = 131072; }
		else if (car_type == 18) 	{ cart_type = CART_TYPE_BOUNTY_BOB; expectedSize = 40960; }
		else if (car_type == 22)	{ cart_type = CART_TYPE_WILLIAMS_64K; expectedSize = 32768; }
		else if (car_type == 26)	{ cart_type = CART_TYPE_MEGACART_16K; expectedSize = 16384; }
		else if (car_type == 27)	{ cart_type = CART_TYPE_MEGACART_32K; expectedSize = 32768; }
		else if (car_type == 28)	{ cart_type = CART_TYPE_MEGACART_64K; expectedSize = 65536; }
		else if (car_type == 29)	{ cart_type = CART_TYPE_MEGACART_128K; expectedSize = 131072; }
		else if (car_type == 33)	{ cart_type = CART_TYPE_SW_XEGS_32K; expectedSize = 32768; }
		else if (car_type == 34)	{ cart_type = CART_TYPE_SW_XEGS_64K; expectedSize = 65536; }
		else if (car_type == 35)	{ cart_type = CART_TYPE_SW_XEGS_128K; expectedSize = 131072; }
		else if (car_type == 39)	{ cart_type = CART_TYPE_PHOENIX_8K;  expectedSize = 8192; }
		else if (car_type == 40)	{ cart_type = CART_TYPE_BLIZZARD_16K; expectedSize = 16384; }
		else if (car_type == 41)	{ cart_type = CART_TYPE_ATARIMAX_1MBIT; expectedSize = 131072; }
		else if (car_type == 43)	{ cart_type = CART_TYPE_SDX_128K; expectedSize = 131072; }
		else if (car_type == 44)	{ cart_type = CART_TYPE_OSS_8K; expectedSize = 8192; }
		else if (car_type == 45) 	{ cart_type = CART_TYPE_OSS_16K_043M; expectedSize = 16384; }
		else if (car_type == 46) 	{ cart_type = CART_TYPE_BLIZZARD_4K; expectedSize = 4096; }
		else if (car_type == 50)	{ cart_type = CART_TYPE_TURBOSOFT_64K; expectedSize = 65536; }
		else if (car_type == 51)	{ cart_type = CART_TYPE_TURBOSOFT_128K; expectedSize = 131072; }
		else if (car_type == 52)	{ cart_type = CART_TYPE_MICROCALC; expectedSize = 32768; }
		else if (car_type == 54)	{ cart_type = CART_TYPE_SIC_128K; expectedSize = 131072; }
		else if (car_type == 57)	{ cart_type = CART_TYPE_2K; expectedSize = 2048; }
		else if (car_type == 58)	{ cart_type = CART_TYPE_4K; expectedSize = 4096; }
		else {
			strcpy(errorBuf, "Unsupported CAR type");
			goto closefile;
		}
	}

	// set a default error
	strcpy(errorBuf, "Can't read file");

	unsigned char *dst = &cart_ram[0];
	int bytes_to_read = 128 * 1024;
	if (xex_file) {
		dst += 4;	// leave room for the file length at the start of sram
		bytes_to_read -= 4;
	}
	// read the file to SRAM
	if (f_read(&fil, dst, bytes_to_read, &br) != FR_OK) {
		cart_type = CART_TYPE_NONE;
		goto closefile;
	}
	size += br;
	if (br == bytes_to_read) {
		// that's 128k read, is there any more?
		if (f_read(&fil, carFileHeader, 1, &br) != FR_OK) {
			cart_type = CART_TYPE_NONE;
			goto closefile;
		}
		if	(br == 1) {
			strcpy(errorBuf, "Cart file/XEX too big (>128k)");
			cart_type = CART_TYPE_NONE;
			goto closefile;
		}
	}

	if (car_file) {
		if (size != expectedSize) {
			strcpy(errorBuf, "CAR file is wrong size");
			cart_type = CART_TYPE_NONE;
			goto closefile;
		}
	}
	else if (xex_file) {
		cart_type = CART_TYPE_XEX;
		// stick the size of the file as the first 4 bytes (little endian)
		cart_ram[0] = size & 0xFF;
		cart_ram[1] = (size >> 8) & 0xFF;
		cart_ram[2] = (size >> 16) & 0xFF;
		cart_ram[3] = 0;	// has to be zero!
	}
	else {	// not a car/xex file - guess the type based on size
		if (size == 8*1024) cart_type = CART_TYPE_8K;
		else if (size == 16*1024) cart_type = CART_TYPE_16K;
		else if (size == 32*1024) cart_type = CART_TYPE_XEGS_32K;
		else if (size == 64*1024) cart_type = CART_TYPE_XEGS_64K;
		else if (size == 128*1024) cart_type = CART_TYPE_XEGS_128K;
		else {
			strcpy(errorBuf, "Unsupported ROM size ");
			cart_type = CART_TYPE_NONE;
			goto closefile;
		}
	}

	// special case for 4K carts to allow the standard 8k emulation to be used
	if (cart_type == CART_TYPE_4K) {
		memcpy(&cart_ram[4096], &cart_ram[0], 4096);
		memset(&cart_ram[0], 0xFF, 4096);
	}

	// special case for 2K carts to allow the standard 8k emulation to be used
	if (cart_type == CART_TYPE_2K) {
		memcpy(&cart_ram[6144], &cart_ram[0], 6144);
		memset(&cart_ram[0], 0xFF, 6144);
	}

	// special case for 4K carts to allow the phoenix 8k emulation to be used
	if (cart_type == CART_TYPE_BLIZZARD_4K) {
		memcpy(&cart_ram[4096], &cart_ram[0], 4096);
	}

closefile:
	f_close(&fil);
cleanup:
	f_mount(0, "", 1);

	return cart_type;
}


/*
 Theory of Operation
 -------------------
 Atari sends command to mcu on cart by writing to $D5DF ($D5E0-$D5FF = SDX)
 (extra paramters for the command in $D500-$D5DE)
 Atari must be running from RAM when it sends a command, since the mcu on the cart will
 go away at that point.
 Atari polls $D500 until it reads $11. At this point it knows the mcu is back
 and it is safe to rts back to code in cartridge ROM again.
 Results of the command are in $D501-$D5DF
*/

int __not_in_flash_func(emulate_boot_rom)(int atrMode) {
	if (atrMode) RD5_LOW; else RD5_HIGH;
	RD4_LOW;
    cart_d5xx[0x00] = 0x11;	// signal that we are here
    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
    while (1)
    {
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            if (pins & RW_GPIO_MASK)
            {   // atari is reading
                SET_DATA_MODE_OUT;
                addr = pins & ADDR_GPIO_MASK;
                gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(cart_d5xx[addr&0xFF])) << 13);
                // wait for phi2 low
                while (gpio_get_all() & PHI2_GPIO_MASK) ;
                SET_DATA_MODE_IN;
            }
            else
            {   // atari is writing
                addr = pins & 0xFF;
				last = pins;
                // read data bus on falling edge of phi2
                while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
                    last = pins;
                data = (last & DATA_GPIO_MASK) >> 13;
                cart_d5xx[addr] = data;
                if (addr == 0xDF)	// write to $D5DF
                    break;
            }
        }
        else if (!(pins & S5_GPIO_MASK))
        {   // normal cartridge read
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(A8PicoCart_rom[addr])) << 13);
            // wait for phi2 low
            while (gpio_get_all() & PHI2_GPIO_MASK) ;
            SET_DATA_MODE_IN;
        }
    }
    return data;
}

void __not_in_flash_func(emulate_standard_8k)() {
	// 8k
	RD4_LOW;
	RD5_HIGH;

    uint32_t pins;
    uint16_t addr;
	while (1)
	{      
		// wait for s5 low
        while ((pins = gpio_get_all()) & S5_GPIO_MASK) ;
        SET_DATA_MODE_OUT;
		// while s5 low
		while(!((pins = gpio_get_all()) & S5_GPIO_MASK)) {
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
		}
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_standard_16k)() {
	// 16k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins;
    uint16_t addr;
	while (1)
	{
		// wait for either s4 or s5 low
		while (((pins = gpio_get_all()) & S4_S5_GPIO_MASK) == S4_S5_GPIO_MASK) ;
		SET_DATA_MODE_OUT;
		if (!(pins & S4_GPIO_MASK)) {
			// while s4 low
			while(!((pins = gpio_get_all()) & S4_GPIO_MASK)) {
				addr = pins & ADDR_GPIO_MASK;
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
			}
		}
		else {
			// while s5 low
			while(!((pins = gpio_get_all()) & S5_GPIO_MASK)) {
				addr = pins & ADDR_GPIO_MASK;
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x2000|addr]) << 13);
			}
		}
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_XEGS_32k)(char switchable) {
	// 32k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true;	// 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
		{	// s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr+addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{	// s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x6000|addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{	// CCTL low + write
            last = pins;
            // read data bus on falling edge of phi2
            while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
            	last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 2 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192*(data & 3));
			if (switchable) {
				if (data & 0x80) {
					RD4_LOW; RD5_LOW;
					rd4_high = rd5_high = false;
				} else {
					RD4_HIGH; RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_XEGS_64k)(char switchable) {
	// 64k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true;	// 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
		{	// s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr+addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{	// s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0xE000|addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{	// CCTL low + write
            last = pins;
            // read data bus on falling edge of phi2
            while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
            	last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 3 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192*(data & 7));
			if (switchable) {
				if (data & 0x80) {
					RD4_LOW; RD5_LOW;
					rd4_high = rd5_high = false;
				} else {
					RD4_HIGH; RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_XEGS_128k)(char switchable) {
	// 128k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true;	// 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
		{	// s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr+addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{	// s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x1E000|addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{	// CCTL low + write
            last = pins;
            // read data bus on falling edge of phi2
            while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
            	last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 4 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192*(data & 0xF));
			if (switchable) {
				if (data & 0x80) {
					RD4_LOW; RD5_LOW;
					rd4_high = rd5_high = false;
				} else {
					RD4_HIGH; RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_bounty_bob)() {
	// 40k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins;
    uint16_t addr;
	unsigned char *bankPtr1 = &cart_ram[0], *bankPtr2 =  &cart_ram[0x4000];
	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;
		
        if (!(pins & S4_GPIO_MASK))
		{	// s4 low
			SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000) {
	            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr2+(addr&0xFFF)))) << 13);
				if (addr == 0x1FF6) bankPtr2 = &cart_ram[0x4000];
				else if (addr == 0x1FF7) bankPtr2 = &cart_ram[0x5000];
				else if (addr == 0x1FF8) bankPtr2 = &cart_ram[0x6000];
				else if (addr == 0x1FF9) bankPtr2 = &cart_ram[0x7000];
			}
			else {
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr1+(addr&0xFFF)))) << 13);
				if (addr == 0x0FF6) bankPtr1 = &cart_ram[0];
				else if (addr == 0x0FF7) bankPtr1 = &cart_ram[0x1000];
				else if (addr == 0x0FF8) bankPtr1 = &cart_ram[0x2000];
				else if (addr == 0x0FF9) bankPtr1 = &cart_ram[0x3000];
			}
		}
		else if (!(pins & S5_GPIO_MASK))
		{	// s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x8000|addr]) << 13);
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK) ;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_atarimax_128k)() {
	// 128k
	RD4_LOW;
	RD5_HIGH;

    uint32_t bank = 0;
    unsigned char *ramPtr;
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // select the right SRAM base, based on the cartridge bank
		ramPtr = &cart_ram[0] + (8192 * (bank & 0xF));

        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
            if ((addr & 0xE0) == 0) {
				bank = addr & 0xF;
				if (addr & 0x10)
					{ RD5_LOW; rd5_high = false; }
				else
					{ RD5_HIGH; rd5_high = true; }
            }
        }
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_williams)() {
	// williams 32k, 64k
	RD4_LOW;
	RD5_HIGH;

    uint32_t bank = 0;
    unsigned char *bankPtr;
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
            if ((addr & 0xF0) == 0) {
				bank = addr & 0x07;
				if (addr & 0x08)
					{ RD5_LOW; rd5_high = false; }
				else
					{ RD5_HIGH; rd5_high = true; }
            }
        }
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_OSS_B)() {
	// OSS type B
	RD5_HIGH;
	RD4_LOW;
    uint32_t pins;
    uint16_t addr;
	uint32_t bank = 1;
	unsigned char *bankPtr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
		// select the right SRAM block, based on the cartridge bank
		bankPtr = &cart_ram[0] + (4096*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000)
	            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr&0xFFF]) << 13);
			else
	            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr+addr))) << 13);
		}
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
			int a0 = addr & 1, a3 = addr & 8;
			if (a3 && !a0) { RD5_LOW; rd5_high = false; }
			else {
				RD5_HIGH;
				rd5_high = true;
				if (!a3 && !a0) bank = 1;
				else if (!a3 && a0) bank = 3;
				else if (a3 && a0) bank = 2;
			}
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_OSS_A)(char is034M) {
	// OSS type A (034M, 043M)
	RD5_HIGH;
	RD4_LOW;
    uint32_t pins;
    uint16_t addr;
	uint32_t bank = 0;
	unsigned char *bankPtr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
		// select the right SRAM block, based on the cartridge bank
		bankPtr = &cart_ram[0] + (4096*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000)
	            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr|0x2000]) << 13);	// 4k bank #3 always mapped to $Bxxx
			else
	            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr+addr))) << 13);
		}
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & 0xF;
			if (addr & 0x8) { RD5_LOW; rd5_high = false; }
			else {
				RD5_HIGH; rd5_high = true;
				if (addr == 0x0) bank = 0;
				if (addr == 0x3 || addr == 0x7) bank = is034M ? 1 : 2;
				if (addr == 0x4) bank = is034M ? 2 : 1;
			}
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_megacart)(int size) {
	// 16k - 128k
	RD4_HIGH;
	RD5_HIGH;

    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	uint32_t bank_mask = 0x00;
	if (size == 32) bank_mask = 0x1;
	else if (size == 64) bank_mask = 0x3;
	else if (size == 128) bank_mask = 0x7;

	bool rd4_high = true, rd5_high = true;	// 400/800 MMU

	unsigned char *ramPtr = &cart_ram[0];
	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
		{	// s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr+addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
        else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{	// s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr+(addr|0x2000)))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{	// CCTL low + write
            last = pins;
            // read data bus on falling edge of phi2
            while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
            	last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low n bits written to $D5xx
			int bank = data & bank_mask;
			ramPtr = &cart_ram[0] + 16384 * (bank&0x7);
			if (data & 0x80) {
				RD4_LOW; RD5_LOW;
				rd4_high = rd5_high = false;
			} else {
				RD4_HIGH; RD5_HIGH;
				rd4_high = rd5_high = true;
			}
		}
	}
}

void __not_in_flash_func(emulate_SIC)() {
	// 128k
	RD5_HIGH;
	RD4_LOW;

    uint32_t pins, last;
    uint16_t addr;
	uint8_t SIC_byte = 0;
	unsigned char *ramPtr = &cart_ram[0];
	bool rd4_high = false, rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
        {   // s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
        }
        else if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + (addr|0x2000)))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xE0) == 0)
			{
				if (pins & RW_GPIO_MASK)
				{   // read from $D5xx
		            SET_DATA_MODE_OUT;
					gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)SIC_byte) << 13);
					// wait for phi2 low
					while (gpio_get_all() & PHI2_GPIO_MASK) ;
					SET_DATA_MODE_IN;
				}
				else
				{	// write to $D5xx
					last = pins;
					// read data bus on falling edge of phi2
					while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
						last = pins;
					SIC_byte = (last & DATA_GPIO_MASK) >> 13;
					// switch bank
					ramPtr = &cart_ram[0] + 16384 * (SIC_byte&0x7);
					if (SIC_byte & 0x40) { RD5_LOW; rd5_high = false; } else { RD5_HIGH; rd5_high = true; }
					if (SIC_byte & 0x20) { RD4_HIGH; rd4_high = true; } else { RD4_LOW; rd4_high = false; }
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_SDX)(int size) {
	// 64k/128k
	RD5_HIGH;
	RD4_LOW;

	unsigned char *ramPtr = &cart_ram[0];
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xF0) == 0xE0) {
				// 64k & 128k versions
				if (size == 64) ramPtr = &cart_ram[0]; else ramPtr = &cart_ram[65536];
				ramPtr += ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
					{ RD5_LOW; rd5_high = false; }
				else
					{ RD5_HIGH; rd5_high = true; }
			}
			if (size == 128 && (addr & 0xF0) == 0xF0) {
				// 128k version only
				ramPtr = &cart_ram[0] + ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
					{ RD5_LOW; rd5_high = false; }
				else
					{ RD5_HIGH; rd5_high = true; }
			}
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_diamond_express)(uint8_t cctlAddr) {
	// 64k
	RD5_HIGH;
	RD4_LOW;

	unsigned char *ramPtr = &cart_ram[0];
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xF0) == cctlAddr) {
				ramPtr = &cart_ram[0] + ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
					{ RD5_LOW; rd5_high = false; }
				else
					{ RD5_HIGH;  rd5_high = true; }
			}
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_blizzard)() {
	//16k
	RD4_HIGH;
	RD5_HIGH;
    uint32_t pins;
    uint16_t addr;
	bool rd4_high = true, rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S4_GPIO_MASK) && rd4_high)
        {   // s4 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
		}
        else if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x2000|addr]) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
			RD4_LOW; RD5_LOW;
			rd4_high = rd5_high = false;
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_turbosoft)(int size) {
	// 64k/128k
	RD4_LOW;
	RD5_HIGH;

    uint32_t bank = 0;
    unsigned char *bankPtr;
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	uint32_t bank_mask = 0x00;
	if (size == 64) bank_mask = 0x7;
	else if (size == 128) bank_mask = 0xF;

	while (1)
	{
        // select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            addr = pins & ADDR_GPIO_MASK;
			bank = addr & bank_mask;
			if (addr & 0x10)
				{ RD5_LOW; rd5_high = false; }
			else
				{ RD5_HIGH; rd5_high = true; }
        }
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_atrax)() {
	// 128k
	RD4_LOW;
	RD5_HIGH;

    uint32_t bank = 0;
    unsigned char *bankPtr;
    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK) ;
			SET_DATA_MODE_IN;
        }
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{	// CCTL low + write
            last = pins;
            // read data bus on falling edge of phi2
            while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
            	last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 4 bits written to $D5xx
			bank = data & 0xF;
			if (data & 0x80)
				{ RD5_LOW; rd5_high = false; }
			else
				{ RD5_HIGH; rd5_high = true; }
        }
	}
}

void __not_in_flash_func(emulate_microcalc)() {
	// 32k
	RD4_LOW;
	RD5_HIGH;

    uint32_t bank = 0;
    unsigned char *bankPtr;
    uint32_t pins;
    uint16_t addr;
	bool rd5_high = true;	// 400/800 MMU

	while (1)
	{
        // select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192*bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
			bank = (bank + 1) % 5;
			if (bank == 4)	// disable
				{ RD5_LOW; rd5_high = false; }
			else
				{ RD5_HIGH; rd5_high = true; }
        }
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_phoenix_8k)() {
    // 8k
    RD4_LOW;
    RD5_HIGH;

    uint32_t pins;
    uint16_t addr;
    bool rd5_high = true;	// 400/800 MMU

    while (1)
    {
        // wait for phi2 high
        while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & S5_GPIO_MASK) && rd5_high)
        {   // s5 low
            SET_DATA_MODE_OUT;
            addr = pins & ADDR_GPIO_MASK;
            gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
        }
        else if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            RD5_LOW;
            rd5_high = false;
        }
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(feed_XEX_loader)(void) {
	RD4_LOW;
	RD5_LOW;

    uint32_t pins, last;
    uint16_t addr;
    uint8_t data;
	
	uint32_t bank = 0;
	unsigned char *ramPtr = &cart_ram[0];
	while (1)
	{
        // wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK)) ;

        if (!(pins & CCTL_GPIO_MASK))
        {   // CCTL low
            if (pins & RW_GPIO_MASK)
            {   // atari is reading
                SET_DATA_MODE_OUT;
                addr = pins & ADDR_GPIO_MASK;
                gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(ramPtr[addr&0xFF])) << 13);
            }
			else
            {   // atari is writing
                addr = pins & 0xFF;
                last = pins;
                // read data bus on falling edge of phi2
                while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
                    last = pins;
				data = (last & DATA_GPIO_MASK) >> 13;
				if (addr == 0)
					bank = (bank&0xFF00) | data;
				else if (addr == 1)
					bank = (bank&0x00FF) | ((data<<8) & 0xFF00);
				ramPtr = &cart_ram[0] + 256 * (bank & 0x01FF);
			}
		}
        // wait for phi2 low
        while (gpio_get_all() & PHI2_GPIO_MASK) ;
        SET_DATA_MODE_IN;
	}
}

void emulate_cartridge(int cartType) {
	if (cartType == CART_TYPE_8K) emulate_standard_8k();
	else if (cartType == CART_TYPE_16K) emulate_standard_16k();
	else if (cartType == CART_TYPE_XEGS_32K) emulate_XEGS_32k(0);
	else if (cartType == CART_TYPE_XEGS_64K) emulate_XEGS_64k(0);
	else if (cartType == CART_TYPE_XEGS_128K) emulate_XEGS_128k(0);
	else if (cartType == CART_TYPE_SW_XEGS_32K) emulate_XEGS_32k(1);
	else if (cartType == CART_TYPE_SW_XEGS_64K) emulate_XEGS_64k(1);
	else if (cartType == CART_TYPE_SW_XEGS_128K) emulate_XEGS_128k(1);
	else if (cartType == CART_TYPE_BOUNTY_BOB) emulate_bounty_bob();
	else if (cartType == CART_TYPE_ATARIMAX_1MBIT) emulate_atarimax_128k();
	else if (cartType == CART_TYPE_WILLIAMS_64K) emulate_williams();
	else if (cartType == CART_TYPE_OSS_16K_TYPE_B) emulate_OSS_B();
	else if (cartType == CART_TYPE_OSS_8K) emulate_OSS_B();
	else if (cartType == CART_TYPE_OSS_16K_034M) emulate_OSS_A(1);
	else if (cartType == CART_TYPE_OSS_16K_043M) emulate_OSS_A(0);
	else if (cartType == CART_TYPE_MEGACART_16K) emulate_megacart(16);
	else if (cartType == CART_TYPE_MEGACART_32K) emulate_megacart(32);
	else if (cartType == CART_TYPE_MEGACART_64K) emulate_megacart(64);
	else if (cartType == CART_TYPE_MEGACART_128K) emulate_megacart(128);
	else if (cartType == CART_TYPE_SIC_128K) emulate_SIC();
	else if (cartType == CART_TYPE_SDX_64K) emulate_SDX(64);
	else if (cartType == CART_TYPE_SDX_128K) emulate_SDX(128);
	else if (cartType == CART_TYPE_DIAMOND_64K) emulate_diamond_express(0xD0);
	else if (cartType == CART_TYPE_EXPRESS_64K) emulate_diamond_express(0x70);
	else if (cartType == CART_TYPE_BLIZZARD_16K) emulate_blizzard();
	else if (cartType == CART_TYPE_4K) emulate_standard_8k();	// patch in load_file()
	else if (cartType == CART_TYPE_TURBOSOFT_64K) emulate_turbosoft(64);
	else if (cartType == CART_TYPE_TURBOSOFT_128K) emulate_turbosoft(128);
	else if (cartType == CART_TYPE_ATRAX_128K) emulate_atrax();
	else if (cartType == CART_TYPE_MICROCALC) emulate_microcalc();
	else if (cartType == CART_TYPE_2K) emulate_standard_8k();
	else if (cartType == CART_TYPE_PHOENIX_8K) emulate_phoenix_8k();
	else if (cartType == CART_TYPE_BLIZZARD_4K) emulate_phoenix_8k();
	else if (cartType == CART_TYPE_XEX) feed_XEX_loader();
	else
	{	// no cartridge (cartType = 0)
		RD4_LOW;
		RD5_LOW;
		while (1) ;
	}
}

void __not_in_flash_func(atari_cart_main)()
{
    gpio_init_mask(ALL_GPIO_MASK);

    gpio_set_dir_in_masked(ADDR_GPIO_MASK|DATA_GPIO_MASK|CCTL_GPIO_MASK|PHI2_GPIO_MASK|RW_GPIO_MASK|S4_GPIO_MASK|S5_GPIO_MASK);
    gpio_set_dir(RD4_PIN, GPIO_OUT);
    gpio_set_dir(RD5_PIN, GPIO_OUT);

	// overclocking isn't necessary for most functions - but XEGS carts weren't working without it
	// I guess we might as well have it on all the time.
	set_sys_clock_khz(250000, true);

	int cartType = 0, atrMode = 0;
	char curPath[256] = "";
	char path[256];

    while (1) {
        int cmd = emulate_boot_rom(atrMode);

        // OPEN ITEM n
        if (cmd == CART_CMD_OPEN_ITEM) 
        {
			int n = cart_d5xx[0x00];
			DIR_ENTRY *entry = (DIR_ENTRY *)&cart_ram[0];
			if (entry[n].isDir)
			{	// directory
				strcat(curPath, "/");
				strcat(curPath, entry[n].filename);
				cart_d5xx[0x01] = 0; // path changed
			}
			else
			{	// file/search result
				if (entry[n].full_path[0])
					strcpy(path, entry[n].full_path);	// search result
				else
					strcpy(path, curPath); // file in current directory
				strcat(path, "/");
				strcat(path, entry[n].filename);
				if (strcasecmp(get_filename_ext(entry[n].filename), "ATR")==0)
				{	// ATR
					cart_d5xx[0x01] = 3;	// ATR
					cartType = CART_TYPE_ATR;
				}
				else
				{	// ROM,CAR or XEX
					cartType = load_file(path);
					if (cartType)
						cart_d5xx[0x01] = (cartType != CART_TYPE_XEX ? 1 : 2);	// file was loaded
					else
					{
						cart_d5xx[0x01] = 4;	// error
						strcpy((char*)&cart_d5xx[0x02], errorBuf);
					}
				}
			}
		}
		// READ DIR
		else if (cmd == CART_CMD_READ_CUR_DIR)
        {
 			int ret = read_directory(curPath);
			if (ret) {
				cart_d5xx[0x01] = 0;	// ok
				cart_d5xx[0x02] = num_dir_entries;
			}
			else
			{
				cart_d5xx[0x01] = 1;	// error
				strcpy((char*)&cart_d5xx[0x02], errorBuf);
			}           
        }
		// GET DIR ENTRY n
		else if (cmd == CART_CMD_GET_DIR_ENTRY)
		{
			int n = cart_d5xx[0x00];
			DIR_ENTRY *entry = (DIR_ENTRY *)&cart_ram[0];
			cart_d5xx[0x01] = entry[n].isDir;
			strcpy((char*)&cart_d5xx[0x02], entry[n].long_filename);
		}
		// UP A DIRECTORY LEVEL
		else if (cmd == CART_CMD_UP_DIR)
		{
			int len = strlen(curPath);
			while (len && curPath[--len] != '/');
			curPath[len] = 0;
		}
		// ROOT DIR (when atari reset pressed)
		else if (cmd == CART_CMD_ROOT_DIR)
			curPath[0] = 0;
		// SEARCH str
		else if (cmd == CART_CMD_SEARCH)
		{
			char searchStr[32];
			strcpy(searchStr, (char*)&cart_d5xx[0x00]);
			int	ret = search_directory(curPath, searchStr);
			if (ret) {
				cart_d5xx[0x01] = 0;	// ok
				cart_d5xx[0x02] = num_dir_entries;
			}
			else
			{
				cart_d5xx[0x01] = 1;	// error
				strcpy((char*)&cart_d5xx[0x02], errorBuf);
			}
		}
		// LOAD PATCHED ATARI OS
		else if (cmd == CART_CMD_LOAD_SOFT_OS)
		{
			int ret = load_file("UNO_OS.ROM");
			if (!ret) {
				for (int i=0; i<16384; i++)
					cart_ram[i] = os_rom[i];
			}
			cart_d5xx[0x01] = 0;	// ok
		}
		// COPY OS CHUNK
		else if (cmd == CART_CMD_SOFT_OS_CHUNK)
		{
			int n = cart_d5xx[0x00];
			for (int i=0; i<128; i++)
				cart_d5xx[0x01+i] = cart_ram[n*128+i];
		}
		// READ ATR SECTOR
		else if (cmd == CART_CMD_READ_ATR_SECTOR)
		{
			//uint8_t device = cart_d5xx[0x00];
			uint16_t sector = (cart_d5xx[0x02] << 8) | cart_d5xx[0x01];
			uint8_t offset = cart_d5xx[0x03];	// 0 = first 128 byte "page", 1 = second, etc
			int ret = read_atr_sector(sector, offset, &cart_d5xx[0x02]);
			cart_d5xx[0x01] = ret;
		}
		// WRITE ATR SECTOR
		else if (cmd == CART_CMD_WRITE_ATR_SECTOR)
		{
			//uint8_t device = cart_d5xx[0x00];
			uint16_t sector = (cart_d5xx[0x02] << 8) | cart_d5xx[0x01];
			uint8_t offset = cart_d5xx[0x03];	// 0 = first 128 byte "page", 1 = second, etc
			int ret = write_atr_sector(sector, offset, &cart_d5xx[0x04]);
			cart_d5xx[0x01] = ret;
		}
		// GET ATR HEADER
		else if (cmd == CART_CMD_ATR_HEADER)
		{
			//uint8_t device = cart_d5xx[0x00];
			if (!&mountedATRs[0].path[0])
				cart_d5xx[0x01] = 1;
			else
			{
				memcpy(&cart_d5xx[0x02], &mountedATRs[0].atrHeader, 16);
				cart_d5xx[0x01] = 0;
			}
		}
		// RESET FLASH FS (when boot with joystick 0 fire pressed)
		else if (cmd == CART_CMD_RESET_FLASH)
			create_fatfs_disk();
		// NO CART
		else if (cmd == CART_CMD_NO_CART)
			cartType = 0;
		// REBOOT TO CART
		else if (cmd == CART_CMD_ACTIVATE_CART)
		{
			if (cartType == CART_TYPE_ATR) {
				atrMode = 1;
				int ret = mount_atr(path);
				if (ret == 0)
					memcpy(&cart_d5xx[0x02], &mountedATRs[0].atrHeader, 16);
				cart_d5xx[0x01] = ret;
			}
			else
				emulate_cartridge(cartType);
		}
    }
}
