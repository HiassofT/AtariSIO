/*
   ataricom - list and manipulate blocks of a Atari COM file

   Copyright (C) 2008-2019 Matthias Reichl <hias@horus.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include "AtariComMemory.h"
#include "ComBlock.h"
#include "Error.h"
#include "Version.h"

static bool offsetsInDecimal = true;

static long parse_int(const char* str)
{
	int base = 10;
	if (str[0] == '$') {
		base = 16;
		str++;
	} else {
		if (str[0] == '0') {
			if (str[1] == 'x' || str[1] == 'X') {
				base = 16;
				str += 2;
			}
		}
	}
	return strtol(str, NULL, base);
}

static bool get_list(const char* str, const char* delim, std::vector<unsigned int>& list)
{
	char* str2 = strdup(str);
	char* tok;
	bool ok = true;

	tok = strtok(str2, delim);

	while ( ok && tok ) {
		long tmp = parse_int(tok);
		if (tmp < 0) {
			ok = false;
		} else {
			list.push_back(tmp);
		}
		tok = strtok(NULL, delim);
	}
	free(str2);
	return ok;
}

static bool get_range(const char* str, unsigned int& start, unsigned int& end, bool accept_single = false)
{
	long v1, v2;
	char* tmp = strdup(str);
	char* p = strchr(tmp,'-');
	if (!p) {
		free(tmp);
		if (accept_single) {
			v1 = parse_int(str);
			if (v1 < 0) {
				return false;
			}
			start = v1;
			end = v1;
			return true;
		}
		return false;
	}
	*p = 0;
	v1 = parse_int(tmp);
	if (*(p+1)) {
		v2 = parse_int(p+1);
	} else {
		v2 = LONG_MAX;
	}
	free(tmp);

	if (v1 <= 0 || v2 <= 0 || v2 < v1) {
		return false;
	} else {
		start = v1;
		end = v2;
		return true;
	}
}

static bool write_block(RCPtr<ComBlock>& block, RCPtr<FileIO>& f,
		bool raw, bool with_ffff,
		const char* infotxt,
		int blknum = -1)
{
	std::cout << infotxt;
	if (blknum >= 0) {
		std::cout << " " << std::setw(4) << blknum << " ";
	} else {
		std::cout << "      ";
	}
	std::cout << block->GetDescription(offsetsInDecimal) << std::endl ;

	bool ok;
	if (raw) {
		ok = block->WriteRawToFile(f);
	} else {
		ok = block->WriteToFile(f, with_ffff);
	}
	if (!ok) {
		std::cout << "error writing to output file!" << std::endl;
	}
	return ok;
}

#ifdef ALL_IN_ONE
int ataricom_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	RCPtr<FileIO> f;
	RCPtr<FileIO> of;

	char* filename = 0;
	char* out_filename = 0;

	enum EOperationMode {
		eModeList,
		eModeProcess,
		eModeCreate
	};

	EOperationMode mode = eModeList;

	bool raw_mode = false;

	bool done = false;

	unsigned int iblk = 1;
	unsigned int oblk = 1;
	uint16_t tmpword;

	unsigned int create_address = 0;

	int run_address = -1;
	int init_address = -1;
	int tmp_address;

	bool merge_mode = false;
	std::vector<unsigned int> merge_start;
	std::vector<unsigned int> merge_end;
	unsigned int merge_count = 0;
	unsigned int merge_idx = 0;

	std::vector<unsigned int> block_start;
	std::vector<unsigned int> block_end;
	unsigned int block_count = 0;
	unsigned int block_idx = 0;

	std::vector< std::vector<unsigned int> > split_list;
	unsigned int split_count = 0;
	unsigned int split_idx = 0;

	enum EBlockMode {
		eNoBlockProcessing,
		eIncludeBlocks,
		eExcludeBlocks
	};

	EBlockMode block_mode = eNoBlockProcessing;

	int idx = 1;

	RCPtr<AtariComMemory> memory;

	printf("ataricom %s\n", VERSION_STRING);
	printf("(c) 2008-2019 Matthias Reichl <hias@horus.com>\n");

	for (idx = 1; idx < argc; idx++) {
		char * arg = argv[idx];
		unsigned int start, end;
		if (*arg == '-') {
			switch (arg[1]) {
			case 'm': // merge mode
				if (mode == eModeCreate) {
					std::cout << "error: -m and -c cannot be used together" << std::endl;
					goto usage;
				}
				mode = eModeProcess;

				merge_mode = true;
				idx++;
				if (idx >= argc) {
					goto usage;
				}
				if (!get_range(argv[idx], start, end, true)) {
					std::cout << "invalid range " << argv[idx] << std::endl;
					goto usage;
				}
				if (merge_count) {
					if (start <= merge_end[merge_count-1]) {
						std::cout << "merge blocks must be in ascending order!" << std::endl;
						goto usage;
					}
				}
				merge_start.push_back(start);
				merge_end.push_back(end);
				merge_count++;
				break;
			case 'x': // exclude blocks
			case 'b': // copy blocks
				if (mode == eModeCreate) {
					std::cout << "error: -b/-x and -c cannot be used together" << std::endl;
					goto usage;
				}
				mode = eModeProcess;
				if (arg[1] == 'x') {
					if (block_mode == eIncludeBlocks) {
						std::cout << "-b and -x are mutually exclusive!" << std::endl;
						goto usage;
					}
					block_mode = eExcludeBlocks;
				} else {
					if (block_mode == eExcludeBlocks) {
						std::cout << "-b and -x are mutually exclusive!" << std::endl;
						goto usage;
					}
					block_mode = eIncludeBlocks;
				}
				idx++;
				if (idx >= argc) {
					goto usage;
				}
				if (!get_range(argv[idx], start, end, true)) {
					std::cout << "invalid range " << argv[idx] << std::endl;
					goto usage;
				}
				if (block_count) {
					if (start <= block_end[block_count-1]) {
						std::cout << "blocks must be in ascending order!" << std::endl;
						goto usage;
					}
				}
				block_start.push_back(start);
				block_end.push_back(end);
				block_count++;
				break;
			case 'n': // raw write mode
				if (mode == eModeCreate) {
					std::cout << "error: -n and -c cannot be used together" << std::endl;
					goto usage;
				}
				if ( (run_address >= 0) || (init_address >= 0)) {
					std::cout << "error: -r/-i and -n cannot be used together" << std::endl;
					goto usage;
				}
				mode = eModeProcess;
				std::cout << "writing file in raw mode" << std::endl;
				raw_mode = true;
				break;
			case 'c': // create mode
				if (raw_mode) {
					std::cout << "error: -n and -c cannot be used together" << std::endl;
					goto usage;
				}
				if (mode == eModeProcess) {
					std::cout << "error: -b/-x/-m and -c cannot be used together" << std::endl;
					goto usage;
				}
				mode = eModeCreate;
				idx++;
				if (idx >= argc) {
					goto usage;
				}
				tmp_address = parse_int(argv[idx]);
				if (tmp_address < 0 || tmp_address > 65534) {
					std::cout << "error: starting address must be less than 65534" << std::endl;
					goto usage;
				}
				create_address = tmp_address;
				break;
			case 'r':
			case 'i':
				if (raw_mode) {
					std::cout << "error: -r/-i and -n cannot be used together" << std::endl;
					goto usage;
				}
				idx++;
				if (idx >= argc) {
					goto usage;
				}

				tmp_address = parse_int(argv[idx]);
				if (tmp_address < 0 || tmp_address > 65535) {
					std::cout << "error: invalid ";
					if (arg[1] == 'r') {
						std::cout << "run ";
					} else {
						std::cout << "init ";
					}
					std::cout << "address" << std::endl;
					goto usage;
				}
				if (arg[1] == 'r') {
					if (run_address >= 0) {
						std::cout << "error: run address already set" << std::endl;
						goto usage;
					} else {
						run_address = tmp_address;
					}
				} else {
					if (init_address >= 0) {
						std::cout << "error: init address already set" << std::endl;
						goto usage;
					} else {
						init_address = tmp_address;
					}
				}
				break;
			case 's': // split block mode
				if (raw_mode) {
					std::cout << "error: -s and -n cannot be used together" << std::endl;
					goto usage;
				}
				idx++;
				if (idx >= argc) {
					goto usage;
				} else {
					std::vector<unsigned int> tmplist;
					unsigned int i, len;
					if ((!get_list(argv[idx], ",", tmplist)) || (tmplist.size() < 2) ) {
						std::cout << "error in split block spec \"" << argv[idx] << "\"" << std::endl;
						goto usage;
					}
					if (split_count > 0) {
						if (tmplist[0] <= split_list[split_count-1][0]) {
							std::cout << "split blocks must be in ascending order!" << std::endl;
							goto usage;
						}
					}
					len = tmplist.size();
					for (i=1; i<len; i++) {
						if (tmplist[i] == 0 || tmplist[i] >= 0xffff) {
							std::cout << "invalid address " << tmplist[i] << " in split blocks mode!" << std::endl;
							goto usage;
							if (i > 1) {
								if (tmplist[i] <= tmplist[i-1]) {
									std::cout << "split addresses must be in ascending order!" << std::endl;
									goto usage;
								}
							}

						}
					}
					split_list.push_back(tmplist);
					split_count++;
				}
				break;
			case 'X': // print file offset in hex
				offsetsInDecimal = false;
				break;
			default:
				goto usage;
			}
		} else {
			if (!filename) {
				filename = arg;
			} else {
				if (out_filename) {
					goto usage;
				}
				out_filename = arg;
			}
		}
	}
	if (!filename) {
		goto usage;
	}

	if (mode == eModeList && ( run_address >= 0 || init_address >= 0 || split_count > 0 )) {
		mode = eModeProcess;
	}

	if ((mode != eModeList) && (!out_filename)) {
		goto usage;
	}

	f = new StdFileIO;
	if (!f->OpenRead(filename)) {
		std::cout << "error: cannot open " << filename << std::endl;
		return 1;
	}
	if (mode != eModeCreate) {
		if (!f->ReadWord(tmpword)) {
			std::cout << "error reading " << filename << std::endl;
			f->Close();
			return 1;
		}
		if (tmpword != 0xffff) {
			std::cout << "error: " << filename << "doesn't start with $FF, $FF" << std::endl;
			f->Close();
			return 1;
		}
	}

	done = false;

	if (mode != eModeList) {
		of = new StdFileIO;
		if (!of->OpenWrite(out_filename)) {
			std::cout << "error: cannot create output file " << out_filename << std::endl;
			f->Close();
			return 1;
		}
	}

	if (merge_mode) {
		memory = new AtariComMemory();
	}

	if (mode == eModeCreate) {
		unsigned int len = f->GetFileLength();
		if (create_address + len > 65536) {
			std::cout << "warning: input file too big, truncating to end address $FFFF" << std::endl;
			len = 65536 - create_address;
		}
		uint8_t* buf = new uint8_t[len];
		if (f->ReadBlock(buf, len) != len) {
			std::cout << "error reading input file!" << std::endl;
			f->Close();
			of->Close();
			of->Unlink(out_filename);
			return 1;
		}
		RCPtr<ComBlock> block = new ComBlock(buf, len, create_address);
		if (!write_block(block, of, false, true, "       write block")) {
			f->Close();
			of->Close();
			return 1;
		}
		delete[] buf;
		oblk++;
	} else {
		while (!done) {
			RCPtr<ComBlock> block;

			try {
				block = new ComBlock(f);
			}
			catch (EOFError) {
				//std::cout << "got end of file" << std::endl;
				done = true;
			}
			catch (ReadError) {
				std::cout << "error reading file " << filename << ", terminating" << std::endl;
				done = true;
			}
			catch (ErrorObject& e) {
				std::cout << e.AsString() << std::endl;
				done = true;
			}
			if (!done) {
				if (mode == eModeList) {
					std::cout << "block "
						<< std::setw(4) << iblk
						<< ": "
						<< block->GetDescription(offsetsInDecimal)
						<< std::endl;
					if (block->ContainsAddress(0x2e0) && block->ContainsAddress(0x2e1)) {
						unsigned int adr = block->GetByte(0x2e0) + (block->GetByte(0x2e1) << 8);
						std::cout << "       RUN: "
							<< std::hex << std::setfill('0')
							<< std::setw(4) << adr
							<< std::dec << std::setfill(' ')
							<< std::endl
						;
					}
					if (block->ContainsAddress(0x2e2) && block->ContainsAddress(0x2e3)) {
						unsigned int adr = block->GetByte(0x2e2) + (block->GetByte(0x2e3) << 8);
						std::cout << "      INIT: "
							<< std::hex << std::setfill('0')
							<< std::setw(4) << adr
							<< std::dec << std::setfill(' ')
							<< std::endl
						;
					}
				} else {
					bool process_block = true;
					bool merge_block = false;
					bool split_block = false;
					bool write_merged_memory = false;

					// check if we need to process this block

					switch (block_mode) {
					case eIncludeBlocks:
						process_block = false;
						break;
					case eExcludeBlocks:
					case eNoBlockProcessing:
						process_block = true;
						break;
					}

					if (block_idx < block_count) {
						switch (block_mode) {
						case eIncludeBlocks:
							if (iblk >= block_start[block_idx] && iblk <= block_end[block_idx]) {
								process_block = true;
								if (iblk == block_end[block_idx]) {
									block_idx ++;
								}
							} else {
								process_block = false;
							}
							break;
						case eExcludeBlocks:
							if (iblk >= block_start[block_idx] && iblk <= block_end[block_idx]) {
								process_block = false;
								if (iblk == block_end[block_idx]) {
									block_idx ++;
								}
							} else {
								process_block = true;
							}
							break;
						case eNoBlockProcessing:
							process_block = true;
							break;
						}
					}


					// check if we need to merge blocks
					if (merge_idx < merge_count) {
						if (iblk >= merge_start[merge_idx] && iblk <= merge_end[merge_idx]) {
							merge_block = true;
							if (iblk == merge_end[merge_idx]) {
								write_merged_memory = true;
								merge_idx++;
							}
						}
					}

					if (split_idx < split_count) {
						if (split_list[split_idx][0] == iblk) {
							split_block = true;
						}
					}

					if (process_block) {
						if (merge_block) {
							std::cout << "     merging block "
								<< std::setw(4) << iblk
								<< " " << block->GetDescription(offsetsInDecimal)
								<< std::endl
							;
							memory->WriteComBlockToMemory(block);
						} else {
							if (split_block) {
								unsigned int i;
								unsigned int list_len = split_list[split_idx].size();
								unsigned int blk_adr = block->GetStartAddress();
								unsigned int adr;
								unsigned int blk_len;

								const uint8_t* data = block->GetRawData();

								std::cout << "   splitting block "
									<< std::setw(4) << iblk
									<< " " << block->GetDescription(offsetsInDecimal)
									<< std::endl
								;

								for (i=1; !done && i<=list_len; i++) {
									if (i < list_len) {
										adr = split_list[split_idx][i];
										if (!block->ContainsAddress(adr-1)) {
											std::cout << "error: no address "
												<< std::setw(4) << std::setfill('0') << std::hex
												<< adr-1
												<< std::dec << std::setfill(' ')
												<< " in block " << iblk
												<< std::endl;
											done = true;
										} else {
											blk_len = adr - blk_adr;
										}
									} else {
										if (!block->ContainsAddress(adr)) {
											std::cout << "error: no address "
												<< std::setw(4) << std::setfill('0') << std::hex
												<< adr
												<< std::dec << std::setfill(' ')
												<< " in block " << iblk
												<< std::endl;
											done = true;
										} else {
											blk_len = block->GetEndAddress() - blk_adr + 1;
										}

									}
									if (!done) {
										RCPtr<ComBlock> blk = new ComBlock(data, blk_len, blk_adr);
										if (write_block(blk, of, raw_mode, oblk==1, " write split block")) {
											oblk++;
										} else {
											done = true;
										}
										data += blk_len;
									}
									blk_adr = adr;
								}

							} else {
								if (write_block(block, of, raw_mode, oblk==1, "       write block", iblk)) {
									oblk++;
								} else {
									done = true;
								}
							}
						}
						if (write_merged_memory) {
							if (memory->ContainsData()) {
								RCPtr<ComBlock> mblock = memory->AsComBlock();
								if (write_block(mblock, of, raw_mode, oblk==1, "write merged block")) {
									oblk++;
								} else {
									done = true;
								}
								memory->Clear();
							} else {
								std::cout << "warning: no merged memory to write" << std::endl;
							}
						}
					} else {
						std::cout << "        skip block "
							<< std::setw(4) << iblk
							<< " " << block->GetDescription(offsetsInDecimal)
							<< std::endl
						;
					}
					if (split_block) {
						split_idx++;
					}
				}
				iblk++;
			}
		}
		if (merge_mode) {
			if (memory->ContainsData()) {
				//std::cout << "warning: EOF reached but merged data to write" << std::endl;
				RCPtr<ComBlock> mblock = memory->AsComBlock();
				if (write_block(mblock, of, raw_mode, oblk==1, "write merged block")) {
					oblk++;
				}
			}
		}
	}
	if (run_address >= 0 || init_address >= 0) {
		uint8_t buf[4];
		unsigned int start = 0, len = 0;
		if (run_address >= 0) {
			std::cout << "  create RUN block "
				<< std::hex << std::setfill('0')
				<< std::setw(4) << run_address
				<< std::dec << std::setfill(' ')
				<< std::endl
			;
			start = 0x2e0;
			buf[len] = run_address & 0xff;
			buf[len+1] = run_address >> 8;
			len += 2;
		}
		if (init_address >= 0) {
			std::cout << " create INIT block "
				<< std::hex << std::setfill('0')
				<< std::setw(4) << init_address
				<< std::dec << std::setfill(' ')
				<< std::endl
			;
			if (start == 0) {
				start = 0x2e2;
			}
			buf[len] = init_address & 0xff;
			buf[len+1] = init_address >> 8;
			len += 2;
		}

		RCPtr<ComBlock> blk = new ComBlock(buf, len, start);
		if (!write_block(blk, of, raw_mode, oblk==1 , "       write block")) {
			f->Close();
			of->Close();
			return 1;
		}
		oblk++;
	}
	if (of) {
		std::cout << "wrote a total of "
			<< std::setw(4) << (oblk-1)
			<< " blocks" << std::endl
		;
		of->Close();
	}
	f->Close();
	return 0;

usage:
	std::cout << "usage: ataricom [options]... file [outfile]" << std::endl
		<<   "  -c address      create COM file from raw data file" << std::endl
		<<   "  -r address      add RUN block with specified address at end of file" << std::endl
		<<   "  -i address      add INIT block with specified address at end of file" << std::endl
		<<   "  -b start[-end]  only process specified blocks" << std::endl
		<<   "  -x start[-end]  exclude specified blocks" << std::endl
		<<   "  -m start-end    merge specified blocks" << std::endl
		<<   "  -s block,adr... split block at given addresses" << std::endl
		<<   "  -n              write raw data blocks (no COM headers)" << std::endl
		<<   "  -X              show block length and file offset in hex" << std::endl
	;

	return 1;
}
