/*
   ataricom - list and manipulate blocks of a Atari COM file

   Copyright (C) 2008-2021 Matthias Reichl <hias@horus.com>

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

struct Range {
	unsigned int start;
	unsigned int end;
};

static std::vector<struct Range> block_range;
static unsigned int block_idx = 0;

enum EBlockMode {
	eNoBlockProcessing,
	eIncludeBlocks,
	eExcludeBlocks
};

static EBlockMode block_mode = eNoBlockProcessing;

bool check_process_block(unsigned int iblk)
{
	bool process_block = false;

	switch (block_mode) {
	case eNoBlockProcessing:
		return true;
	case eIncludeBlocks:
		break;
	case eExcludeBlocks:
		process_block = true;
		break;
	}

	if (block_idx < block_range.size()) {
		switch (block_mode) {
		case eIncludeBlocks:
			if (iblk >= block_range[block_idx].start && iblk <= block_range[block_idx].end) {
				process_block = true;
				if (iblk == block_range[block_idx].end) {
					block_idx ++;
				}
			} else {
				process_block = false;
			}
			break;
		case eExcludeBlocks:
			if (iblk >= block_range[block_idx].start && iblk <= block_range[block_idx].end) {
				process_block = false;
				if (iblk == block_range[block_idx].end) {
					block_idx ++;
				}
			} else {
				process_block = true;
			}
			break;
		default:
			break;
		}
	}
	return process_block;
}

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

static bool get_range(const char* str, struct Range& range, bool accept_single = false)
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
			range.start = v1;
			range.end = v1;
			return true;
		}
		return false;
	}
	*p = 0;
	v1 = parse_int(tmp);
	if (*(p+1)) {
		v2 = parse_int(p+1);
	} else {
		v2 = INT_MAX;
	}
	free(tmp);

	if (v1 <= 0 || v2 <= 0 || v2 < v1) {
		return false;
	} else {
		range.start = v1;
		range.end = v2;
		return true;
	}
}

static bool get_ranges(const char* str, std::vector<struct Range>& ranges, bool accept_single = false)
{
	if (!*str) {
		return false;
	}

	char* tmpstr = strdup(str);
	char *s;
	struct Range range;

	for ((s = strtok(tmpstr, ",")); s; (s = strtok(NULL, ","))) {
		if (!*s) {
			free(tmpstr);
			return false;
		}
		if (!get_range(s, range, accept_single)) {
			std::cout << "invalid range " << s << std::endl;
			free(tmpstr);
			return false;
		}
		if (ranges.size()) {
			if (range.start <= ranges.back().end) {
				std::cout << "blocks must be in ascending order!" << std::endl;
				free(tmpstr);
				return false;
			}
		}
		ranges.push_back(range);
	}
	free(tmpstr);
	return true;
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
		eModeCreate,
		eModeExtract,
		eModeExtractWithAddress
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

	std::vector<struct Range> merge_range;
	unsigned int merge_idx = 0;

	std::vector< std::vector<unsigned int> > split_list;
	unsigned int split_idx = 0;

	int idx = 1;

	RCPtr<AtariComMemory> memory;

	printf("ataricom %s\n", VERSION_STRING);
	printf("(c) 2008-%d Matthias Reichl <hias@horus.com>\n", CURRENT_YEAR);

	for (idx = 1; idx < argc; idx++) {
		char * arg = argv[idx];
		if (*arg == '-') {
			switch (arg[1]) {
			case 'm': // merge mode
				idx++;
				if (idx >= argc) {
					goto usage;
				}
				if (!get_ranges(argv[idx], merge_range, false)) {
					goto usage;
				}
				break;
			case 'x': // exclude blocks
			case 'b': // copy blocks
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
				if (!get_ranges(argv[idx], block_range, true)) {
					goto usage;
				}
				break;
			case 'n': // raw write mode
				raw_mode = true;
				break;
			case 'c': // create mode
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
					if (split_list.size()) {
						if (tmplist[0] <= split_list[split_list.size()-1][0]) {
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
				}
				break;
			case 'X': // print file offset in hex
				offsetsInDecimal = false;
				break;
			case 'e':
				mode = eModeExtract;
				break;
			case 'E':
				mode = eModeExtractWithAddress;
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

	if (mode == eModeList &&
            (run_address >= 0 || init_address >= 0 || merge_range.size() ||
             split_list.size() || block_mode != eNoBlockProcessing || out_filename)) {
		mode = eModeProcess;
	}

	if (raw_mode &&
            (run_address >= 0 || init_address >= 0 || split_list.size() || mode == eModeCreate)) {
		std::cout << "error: -n and -c/-n/-r/-s cannot be used together" << std::endl;
		goto usage;
	}

	switch (mode) {
	case eModeCreate:
		if (split_list.size() || merge_range.size() || block_mode != eNoBlockProcessing || raw_mode) {
			std::cout << "error: -c and -b/-x/-m/-n/-s cannot be used together" << std::endl;
			goto usage;
		}
		break;
	case eModeExtract:
	case eModeExtractWithAddress:
		if (split_list.size() || merge_range.size() || run_address >= 0 || init_address >= 0) {
			std::cout << "error: -e/-E and -i/-m/-r/-s cannot be used together" << std::endl;
			goto usage;
		}
	default:
		break;
	}

	if ((mode != eModeList) && (!out_filename)) {
		std::cout << "error: output filename missing" << std::endl;
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

	if (mode == eModeCreate || mode == eModeProcess) {
		of = new StdFileIO;
		if (!of->OpenWrite(out_filename)) {
			std::cout << "error: cannot create output file " << out_filename << std::endl;
			f->Close();
			return 1;
		}
	}

	if (merge_range.size()) {
		memory = new AtariComMemory();
	}

	if (mode == eModeCreate) {
		unsigned int len = f->GetFileLength();
		if (!len) {
			std::cout << "error: input file is empty" << std::endl;
			f->Close();
			of->Close();
			of->Unlink(out_filename);
			return 1;
		}
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
			catch (EOFError&) {
				//std::cout << "got end of file" << std::endl;
				done = true;
			}
			catch (ReadError&) {
				std::cout << "error reading file " << filename << ", terminating" << std::endl;
				done = true;
			}
			catch (ErrorObject& e) {
				std::cout << e.AsString() << std::endl;
				done = true;
			}
			if (!done) {
				bool process_block = false;
				bool merge_block = false;
				bool split_block = false;
				bool write_merged_memory = false;

				switch (mode) {
				case eModeList:
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
					break;
				case eModeProcess:
					process_block = check_process_block(iblk);

					// check if we need to merge blocks
					if (merge_idx < merge_range.size()) {
						if (iblk >= merge_range[merge_idx].start && iblk <= merge_range[merge_idx].end) {
							merge_block = true;
							if (iblk == merge_range[merge_idx].end) {
								write_merged_memory = true;
								merge_idx++;
							}
						}
					}

					if (split_idx < split_list.size()) {
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
					break;
				case eModeExtract:
				case eModeExtractWithAddress:
					if (check_process_block(iblk)) {
						char outname[PATH_MAX];
						const char* ext;
						if (raw_mode) {
							ext = "bin";
						} else {
							ext = "obj";
						}
						if (mode == eModeExtract) {
							snprintf(outname, PATH_MAX, "%s%04d.%s", out_filename, iblk, ext);
						} else {
							snprintf(outname, PATH_MAX, "%s%04d_%04X_%04X.%s", out_filename, iblk,
								 block->GetStartAddress(), block->GetEndAddress(), ext);
						}
						outname[PATH_MAX-1] = 0;

						of = new StdFileIO;
						if (!of->OpenWrite(outname)) {
							std::cout << "error: cannot create output file " << outname << std::endl;
							of->Close();
							of.SetToNull();
							done = true;
							continue;
						}
						bool ok;
						if (raw_mode) {
							ok = block->WriteRawToFile(of);
						} else {
							ok = block->WriteToFile(of, true);
						}
						if (!ok) {
							std::cout << "error: cannot write block to " << outname << std::endl;
							of->Close();
							of->Unlink(outname);
							of.SetToNull();
							done = true;
							continue;
						}
						of->Close();
						of.SetToNull();
						std::cout << "write block "
							<< std::setw(4) << iblk
							<< " " << block->GetShortDescription()
							<< " to " << outname
							<< std::endl
						;
					} else {
						std::cout << " skip block "
							<< std::setw(4) << iblk
							<< " " << block->GetShortDescription()
							<< std::endl
						;
					}
					break;
				default:
					break;
				}
				iblk++;
			}
		}
		if (merge_range.size()) {
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
		<<   "  -e              extract blocks to outfileBBBB.ext" << std::endl
		<<   "  -E              extract blocks to outfileBBBB_SADR_EADR.ext" << std::endl
		<<   "  -r address      add RUN block with specified address at end of file" << std::endl
		<<   "  -i address      add INIT block with specified address at end of file" << std::endl
		<<   "  -b range[,...]  only process specified blocks" << std::endl
		<<   "  -x range[,...]  exclude specified blocks" << std::endl
		<<   "  -m range[,...]  merge specified blocks" << std::endl
		<<   "  -s block,adr... split block at given addresses" << std::endl
		<<   "  -n              write raw data blocks (no COM headers)" << std::endl
		<<   "  -X              show block length and file offset in hex" << std::endl
	;

	return 1;
}
