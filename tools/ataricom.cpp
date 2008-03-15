/*
   ataricom - list and manipulate blocks of a Atari COM file

   Copyright (C) 2008 Matthias Reichl <hias@horus.com>

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

static bool get_range(const char* str, unsigned int& start, unsigned int& end, bool accept_single = false)
{
	long v1, v2;
	char* tmp = strdup(str);
	char* p = strchr(tmp,'-');
	if (!p) {
		free(tmp);
		if (accept_single) {
			v1 = atoi(str);
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
	v1 = atoi(tmp);
	v2 = atoi(p+1);
	free(tmp);
	if (v1 <= 0 || v2 <= 0 || v2 < v1) {
		return false;
	} else {
		start = v1;
		end = v2;
		return true;
	}
}

int main(int argc, char** argv)
{
	RCPtr<FileIO> f;
	RCPtr<FileIO> of;

	char* filename = 0;
	char* out_filename = 0;

	bool list_mode = true;
	bool raw_mode = false;

	bool done = false;

	unsigned int iblk = 1;
	unsigned int oblk = 1;
	unsigned int tmpword;

	bool merge_mode = false;
	std::vector<unsigned int> merge_start;
	std::vector<unsigned int> merge_end;
	unsigned int merge_count = 0;
	unsigned int merge_idx = 0;

	std::vector<unsigned int> block_start;
	std::vector<unsigned int> block_end;
	unsigned int block_count = 0;
	unsigned int block_idx = 0;

	enum EBlockMode {
		eNoBlockProcessing,
		eIncludeBlocks,
		eExcludeBlocks
	};

	EBlockMode block_mode = eNoBlockProcessing;

	int idx = 1;

	RCPtr<AtariComMemory> memory;

	std::cout << "ataricom V" << VERSION_STRING << " (c) 2008 by Matthias Reichl <hias@horus.com>" << std::endl;

	for (idx = 1; idx < argc; idx++) {
		char * arg = argv[idx];
		unsigned int start, end;
		if (*arg == '-') {
			switch (arg[1]) {
			case 'm': // merge mode
				list_mode = false;
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
						std::cout << "blocks must be in ascending order!" << std::endl;
						goto usage;
					}
				}
				merge_start.push_back(start);
				merge_end.push_back(end);
				merge_count++;
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
				list_mode = false;
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
			case 'r': // raw write mode
				std::cout << "writing file in raw mode" << std::endl;
				raw_mode = true;
				list_mode = false;
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
	if ((!list_mode) && (!out_filename)) {
		goto usage;
	}

	f = new StdFileIO;
	if (!f->OpenRead(filename)) {
		std::cout << "error: cannot open " << filename << std::endl;
		return 1;
	}
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

	done = false;

	if (!list_mode) {
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
			if (list_mode) {
				std::cout << "block "
					<< std::setw(4) << iblk
					<< ": "
					<< block->GetDescription()
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
				if (process_block) {
					if (merge_block) {
						std::cout << "       merge block "
							<< std::setw(4) << iblk
							<< " " << block->GetDescription()
							<< std::endl
						;
						memory->WriteComBlockToMemory(block);
					} else {
						std::cout << "       write block "
							<< std::setw(4) << iblk
							<< " " << block->GetDescription()
							<< std::endl
						;
						bool ok;
						if (raw_mode) {
							ok = block->WriteRawToFile(of);
						} else {
							ok = block->WriteToFile(of, oblk==1);
						}
						if (!ok) {
							std::cout << "error writing to output file!" << std::endl;
							done = true;
						} else {
							oblk++;
						}
					}
					if (write_merged_memory) {
						if (memory->ContainsData()) {
							RCPtr<ComBlock> mblock = memory->AsComBlock();
							std::cout << "write merged block      "
								<< mblock->GetDescription() << std::endl;
							bool ok;
							if (raw_mode) {
								ok = mblock->WriteRawToFile(of);
							} else {
								ok = mblock->WriteToFile(of, oblk==1);
							}
							if (!ok) {
								std::cout << "error writing to output file!" << std::endl;
								done = true;
							} else {
								oblk++;
							}
							memory->Clear();
						} else {
							std::cout << "warning: no merged memory to write" << std::endl;
						}
					}
				} else {
					std::cout << "        skip block "
						<< std::setw(4) << iblk
						<< " " << block->GetDescription()
						<< std::endl
					;
				}
			}
			iblk++;
		}
	}
	if (merge_mode) {
		if (memory->ContainsData()) {
			std::cout << "warning: EOF reached but merged data to write" << std::endl;
			RCPtr<ComBlock> mblock = memory->AsComBlock();
			std::cout << "write merged block      "
				<< mblock->GetDescription() << std::endl;
			bool ok;
			if (raw_mode) {
				ok = mblock->WriteRawToFile(of);
			} else {
				ok = mblock->WriteToFile(of, oblk==1);
			}
			if (!ok) {
				std::cout << "error writing to output file!" << std::endl;
			} else {
				oblk++;
			}
		}
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
	std::cout << "usage: ataricom [-bmx range]... file [outfile]" << std::endl
		<<   "       -b start[-end] : only copy specified blocks" << std::endl
		<<   "       -x start[-end] : exclude specified blocks" << std::endl
		<<   "       -m start-end : merge specified blocks" << std::endl
		<<   "       -r : write raw data blocks (without COM header)" << std::endl
	;

	return 1;
}
