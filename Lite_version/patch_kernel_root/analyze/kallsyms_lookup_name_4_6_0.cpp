﻿#include "kallsyms_lookup_name_4_6_0.h"
#include "base_func.h"

#include <set>

#ifndef MIN
#define MIN(x, y)(x < y) ? (x) : (y)
#endif // !MIN

#define R_AARCH64_RELATIVE 1027
#define MAX_FIND_RANGE 0x1000
namespace {
	const int KSYM_NAME_LEN = 128;
	struct Elf64_Rela {
		uint64_t r_offset = 0;
		uint64_t r_info = 0;
		uint64_t r_addend = 0;
	};

	static inline uint32_t rd32_le(const std::vector<char>& buf, size_t off) {
		uint32_t v = 0;
		std::memcpy(&v, buf.data() + off, sizeof(v));
		return v;
	}

	static inline bool looks_kernel_va(uint64_t v) {
		static const uint64_t starts[] = {
			0xFFFFFFC000000000ULL, // VA_BITS=39
			0xFFFFFE0000000000ULL, // VA_BITS=42
			0xFFFF800000000000ULL, // VA_BITS=48
			0xFFF8000000000000ULL  // VA_BITS=52
		};
		for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
			if ((v & starts[i]) == starts[i]) return true;
		}
		return false;
	}
}
KallsymsLookupName_4_6_0::KallsymsLookupName_4_6_0(const std::vector<char>& file_buf) : m_file_buf(file_buf)
{
}

KallsymsLookupName_4_6_0::~KallsymsLookupName_4_6_0()
{
}

bool KallsymsLookupName_4_6_0::init() {
	size_t code_static_start = find_static_code_start();
	std::cout << std::hex << "code_static_start: 0x" << code_static_start << std::endl;

	size_t offset_list_start = 0, offset_list_end = 0;
	size_t kallsyms_num_offset = 0;
	if (find_kallsyms_offsets_list(offset_list_start, offset_list_end)) {
		m_kallsyms_num = find_kallsyms_num((offset_list_end - offset_list_start) / sizeof(int), offset_list_end, 10, kallsyms_num_offset);
		if (!m_kallsyms_num) {
			std::cout << "Unable to find the num of kallsyms offset list" << std::endl;
			return false;
		}
		std::cout << std::hex << "kallsyms_num: 0x" << m_kallsyms_num << ", offset: 0x" << kallsyms_num_offset << std::endl;

		// revise the offset list offset again
		const int offset_list_var_len = sizeof(long);
		offset_list_start = offset_list_end - m_kallsyms_num * offset_list_var_len;
		long test_first_offset_list_val;
		do {
			test_first_offset_list_val = *(long*)&m_file_buf[offset_list_start];
			if (test_first_offset_list_val) {
				offset_list_start -= offset_list_var_len;
				offset_list_end -= offset_list_var_len;
			}
		} while (test_first_offset_list_val);

		std::cout << std::hex << "kallsyms_offset_start: 0x" << offset_list_start << std::endl;
		std::cout << std::hex << "kallsyms_offset_end: 0x" << offset_list_end << std::endl;
		m_kallsyms_offsets.offset = offset_list_start;
		CONFIG_KALLSYMS_BASE_RELATIVE = true;

	} else if (find_kallsyms_addresses_list(m_kallsyms_addresses.addresses)) {
		m_kallsyms_addresses.printf();
		const auto& addresses = m_kallsyms_addresses.addresses;
		uint64_t virtual_base = addresses[0].second;
		uint64_t ptr_table_end = addresses[addresses.size() - 1].first;
		uint64_t offset_table_end = ptr_table_end - virtual_base;
		m_kallsyms_num = find_kallsyms_num(addresses.size(), offset_table_end, 0, kallsyms_num_offset);
		if (!m_kallsyms_num) {
			std::cout << "Unable to find the num of kallsyms offset list" << std::endl;
			return false;
		}
		std::cout << std::hex << "kallsyms_num: 0x" << m_kallsyms_num << ", offset: 0x" << kallsyms_num_offset << std::endl;
		CONFIG_KALLSYMS_BASE_RELATIVE = false;
	} else {
		std::cout << "Unable to find the list of 'kallsyms offsets' and 'kallsyms addresses'" << std::endl;
		return false;
	}

	size_t kallsyms_num_end_offset = kallsyms_num_offset + sizeof(m_kallsyms_num);
	size_t name_list_start = 0, name_list_end = 0;
	if (!find_kallsyms_names_list(m_kallsyms_num, kallsyms_num_end_offset, name_list_start, name_list_end)) {
		std::cout << "Unable to find the list of kallsyms names list" << std::endl;
		return false;
	}
	std::cout << std::hex << "kallsyms_names_start: 0x" << name_list_start << std::endl;
	std::cout << std::hex << "kallsyms_names_end: 0x" << name_list_end << std::endl;
	m_kallsyms_names.offset = name_list_start;

	size_t markers_list_start = 0;
	size_t markers_list_end = 0;
	if (!find_kallsyms_markers_list(m_kallsyms_num, name_list_end, markers_list_start, markers_list_end)) {
		std::cout << "Unable to find the list of kallsyms markers list" << std::endl;
		return false;
	}
	std::cout << std::hex << "kallsyms_markers_start: 0x" << markers_list_start << std::endl;
	std::cout << std::hex << "kallsyms_markers_end: 0x" << markers_list_end << std::endl;
	m_kallsyms_markers.offset = markers_list_start;

	size_t token_table_start = 0;
	size_t token_table_end = 0;
	if (!find_kallsyms_token_table(markers_list_end, token_table_start, token_table_end)) {
		std::cout << "Unable to find the list of kallsyms token table" << std::endl;
		return false;
	}
	std::cout << std::hex << "kallsyms_token_table_start: 0x" << token_table_start << std::endl;
	std::cout << std::hex << "kallsyms_token_table_end: 0x" << token_table_end << std::endl;
	m_kallsyms_token_table.offset = token_table_start;

	size_t token_index_start = 0;
	if (!find_kallsyms_token_index(token_table_end, token_index_start)) {
		std::cout << "Unable to find the list of kallsyms token index" << std::endl;
		return false;
	}
	std::cout << std::hex << "kallsyms_token_index_start: 0x" << token_index_start << std::endl;
	m_kallsyms_token_index.offset = token_index_start;

	if (!CONFIG_KALLSYMS_BASE_RELATIVE) {
		if (!resolve_kallsyms_addresses_symbol_base(code_static_start, m_kallsyms_addresses.base_address)) {
			std::cout << "Unable to find the list of kallsyms addresses symbol base" << std::endl;
			return false;
		}
	} else {
		if (!resolve_kallsyms_offset_symbol_base(code_static_start, m_kallsyms_offsets.base_off)) {
			std::cout << "Unable to find the list of kallsyms sym function entry offset" << std::endl;
			return false;
		}
	}

	m_kallsyms_symbols_cache.clear();
	m_inited = true;
	return true;
}

bool KallsymsLookupName_4_6_0::is_inited() {
	return m_inited;
}

int KallsymsLookupName_4_6_0::get_kallsyms_num() {
	return m_kallsyms_num;
}

size_t KallsymsLookupName_4_6_0::find_static_code_start() {
	const uint32_t A64_NOP = 0xD503201F;
	const size_t   N = m_file_buf.size();
	if (N < 0x200) return 0;

	const size_t SCAN_LIMIT = std::min(N, static_cast<size_t>(0x1000000));
	const size_t START_OFF = 0x100;

	const size_t ALLOW_NOISE_BYTES = 0x32;

	size_t j = (START_OFF + 3) & ~size_t(3);  // 4-byte alignment
	while (j + 4 <= SCAN_LIMIT) {
		uint32_t w = rd32_le(m_file_buf, j);

		// Normal filling: 0 or NOP, proceed directly
		if (w == 0 || w == A64_NOP) {
			j += 4;
			continue;
		}

		// Encountering non filled: Try to find the next 0/NOP in [j+4, j+ALLOW-NOISE-BYTES]
		size_t k = j + 4;
		size_t k_limit = std::min(j + ALLOW_NOISE_BYTES, SCAN_LIMIT);
		bool resumed = false;

		while (k + 4 <= k_limit) {
			uint32_t u = rd32_le(m_file_buf, k);
			if (u == 0 || u == A64_NOP) {
				// Regarded as small noise, continuous segments without interruption, continue from here
				j = k + 4;
				resumed = true;
				break;
			}
			k += 4;
		}

		if (!resumed) {
			// Within the allowed noise window, 0/NOP was not encountered again,
			// Identifying J as the starting point of 'real code'
			return j;
		}
		// If recovered, while the outer layer continues
	}
	// Scan to the upper limit but still unable to find the code starting point
	return 0;
}

static bool __find_kallsyms_addresses_list(const std::vector<char>& file_buf, size_t max_cnt, size_t& start, size_t& end) {
	auto var_len = sizeof(Elf64_Rela);
	for (auto x = 0; x + var_len < file_buf.size(); x += 8) {
		Elf64_Rela *rela = (Elf64_Rela*)&file_buf[x];
		if (!looks_kernel_va(rela->r_offset) || !looks_kernel_va(rela->r_addend) || (rela->r_info & 0xFFFFFFFFu) != R_AARCH64_RELATIVE) {
			continue;
		}
		int cnt = 0;
		auto j = x;
		for (; j + var_len < file_buf.size(); j += var_len) {
			Elf64_Rela* child = (Elf64_Rela*)&file_buf[j];
			if (!looks_kernel_va(child->r_offset) || !looks_kernel_va(child->r_addend) || (child->r_info & 0xFFFFFFFFu) != R_AARCH64_RELATIVE) {
				break;
			}
			cnt++;
		}
		if (cnt >= max_cnt) {
			start = x;
			end = j;
			return true;
		}
	}
	return false;
}

static void __convert_kallsyms_addresses_list(const std::vector<char>& file_buf, size_t start, size_t end, std::vector<std::pair<uint64_t, uint64_t>>& addresses) {
	std::unordered_map<uint64_t, Elf64_Rela> virtual_mapper;
	for (auto x = start; x < end; x += sizeof(Elf64_Rela)) {
		Elf64_Rela* rela = (Elf64_Rela*)&file_buf[x];
		virtual_mapper[rela->r_offset] = *rela;
	}
	addresses.clear();
	addresses.reserve(virtual_mapper.size());
	for (auto& kv : virtual_mapper) {
		addresses.emplace_back(kv.first, kv.second.r_addend);
	}
}

static bool __shrink_kallsyms_addresses_list(const std::vector<char>& file_buf,
	std::vector<std::pair<uint64_t, uint64_t>>& addresses) {

	std::sort(addresses.begin(), addresses.end(),
		[](const auto& a, const auto& b) { return a.first < b.first; });
	auto shrink_to_range = [&](size_t i0, size_t i1) {
		if (i0 > 0) addresses.erase(addresses.begin(), addresses.begin() + i0);
		if (i1 > i0) addresses.erase(addresses.begin() + (i1 - i0), addresses.end());
		};

	const size_t MIN_LEN = 4096;
	size_t best_i = 0, best_len = 0;
	double best_score = 0.0;
	for (size_t i = 0; i < addresses.size(); ) {
		// Starting point of runway
		size_t j = i + 1;
		uint64_t prev_off = addresses[i].first;
		uint64_t prev_val = addresses[i].second;

		size_t good = looks_kernel_va(prev_val) ? 1 : 0; 
		// Continuous slot position: offset+8 per step
		while (j < addresses.size()) {
			if (addresses[j].first != prev_off + 8) break;
			uint64_t v = addresses[j].second;
			if (looks_kernel_va(v) && v >= prev_val) ++good; // Allow equality (non descending)
			prev_off = addresses[j].first;
			prev_val = v;
			++j;
		}

		size_t len = j - i;
		double score = (len ? (double)good / (double)len : 0.0);

		// Choose the longest; If the length is the same, choose the one with a higher "quality score"
		if ((len > best_len) || (len == best_len && score > best_score)) {
			best_len = len;
			best_i = i;
			best_score = score;
		}

		i = j; // Jump to the next paragraph
	}

	// If you find a suitable runway, shrink the addresses to this section in place
	if (best_len >= MIN_LEN && best_score > 0.90) {
		shrink_to_range(best_i, best_i + best_len);
		return true;
	}
	return false;
}

bool KallsymsLookupName_4_6_0::find_kallsyms_addresses_list(std::vector<std::pair<uint64_t, uint64_t>>& addresses) {
	for (auto i = 60000; i > 30000; i -= 5000) {
		size_t start = 0, end = 0;
		if (__find_kallsyms_addresses_list(m_file_buf, i, start, end)) {
			__convert_kallsyms_addresses_list(m_file_buf, start, end, addresses);
			return __shrink_kallsyms_addresses_list(m_file_buf, addresses);
		}
	}
	return false;
}

static bool __find_kallsyms_offsets_list(const std::vector<char>& file_buf, size_t max_cnt, size_t& start, size_t& end) {
	const int var_len = sizeof(long);
	for (auto x = 0; x + var_len < file_buf.size(); x += var_len) {
		long val1 = *(long*)&file_buf[x];
		long val2 = *(long*)&file_buf[x + var_len];
		if (val1 != 0 || val1 >= val2) {
			continue;
		}
		int cnt = 0;
		auto j = x + var_len;
		for (; j + var_len < file_buf.size(); j += var_len) {
			val1 = *(long*)&file_buf[j];
			val2 = *(long*)&file_buf[j + var_len];
			if (val1 > val2 || val2 == 0 || (val2 - val1) > 0x1000000) {
				j += var_len;
				break;
			}
			cnt++;
		}
		if (cnt >= max_cnt) {
			start = x;
			end = j;
			return true;
		}
	}
	return false;
}

bool KallsymsLookupName_4_6_0::find_kallsyms_offsets_list(size_t& start, size_t& end) {
	for (auto i = 60000; i > 5000; i -= 5000) {
		if (__find_kallsyms_offsets_list(m_file_buf, i, start, end)) {
			return true;
		}
	}
	return false;
}

int KallsymsLookupName_4_6_0::find_kallsyms_num(size_t size, size_t offset_list_end, size_t fuzzy_range, size_t& kallsyms_num_offset) {
	size_t allow_min_size = size - fuzzy_range;
	size_t allow_max_size = size + fuzzy_range;
	auto _min = MIN(m_file_buf.size(), MAX_FIND_RANGE);
	int cnt = 10;
	for (size_t x = 0; (x + sizeof(int)) < _min; x++) {
		auto pos = offset_list_end + x * sizeof(int);
		int val = *(int*)&m_file_buf[pos];
		if (val == 0) {
			continue;
		}
		if (val >= allow_min_size && val <= allow_max_size) {
			kallsyms_num_offset = pos;
			return val;
		}
		if (--cnt == 0) {
			break;
		}
	}
	return 0;
}


bool KallsymsLookupName_4_6_0::find_kallsyms_names_list(int kallsyms_num, size_t kallsyms_num_end_offset, size_t& name_list_start, size_t& name_list_end) {

	name_list_start = 0;
	name_list_end = 0;
	size_t x = kallsyms_num_end_offset;
	auto _min = MIN(m_file_buf.size(), x + MAX_FIND_RANGE);
	for (; (x + sizeof(char)) < _min; x++) {
		char val = *(char*)&m_file_buf[x];
		if (val == '\0') {
			continue;
		}
		name_list_start = x;
		break;
	}
	size_t off = name_list_start;
	for (int i = 0; i < kallsyms_num; i++) {
		unsigned char ch = (unsigned char)m_file_buf[off++];
		off += ch;
	}
	name_list_end = off;
	return true;
}


bool KallsymsLookupName_4_6_0::find_kallsyms_markers_list(int kallsyms_num, size_t name_list_end_offset, size_t& markers_list_start, size_t& markers_list_end) {
	size_t start = align_up<8>(name_list_end_offset);
	const int var_len = sizeof(long);
	for (auto x = start; x + var_len < m_file_buf.size(); x += var_len) {
		long val1 = *(long*)&m_file_buf[x];
		long val2 = *(long*)&m_file_buf[x + var_len];
		if (val1 == 0 && val2 > 0) {
			markers_list_start = x;
			break;
		} else if (val1 == 0 && val2 == 0) {
			continue;
		}
		return false;
	}
	
	auto exist_val_start = markers_list_start + var_len;

	bool is_align8 = false;
	int cnt = 5;
	long last_second_var_val = 0;
	for (auto y = markers_list_start + var_len; y + var_len < m_file_buf.size(); y += var_len * 2) {
		long val1 = *(long*)&m_file_buf[y];
		long val2 = *(long*)&m_file_buf[y + var_len];
		if (val2 != last_second_var_val) {
			break;
		}
		last_second_var_val = val2;
		cnt--;
		if (cnt == 0) {
			is_align8 = true;
			break;
		}
	}
	if (is_align8) {
		size_t back_val = align_up<8>(markers_list_start) - markers_list_start;
		if (back_val == 0) {
			markers_list_start -= 8;
		} else {
			markers_list_start -= back_val; // 4
		}
		markers_list_end = markers_list_start + ((kallsyms_num + 255) >> 8) * sizeof(long) * 2;
	} else {
		markers_list_end = markers_list_start + ((kallsyms_num + 255) >> 8) * sizeof(long);
	}
	
	return true;
}

bool KallsymsLookupName_4_6_0::find_kallsyms_token_table(size_t markers_list_end_offset, size_t& kallsyms_token_table_start, size_t& kallsyms_token_table_end) {
	size_t start = align_up<8>(markers_list_end_offset);
	const int var_len = sizeof(long);
	for (auto x = start; x + var_len < m_file_buf.size(); x += var_len) {
		long val1 = *(long*)&m_file_buf[x];
		if (val1 == 0) {
			continue;
		}
		size_t off = x;
		for (unsigned int i = 0; i < 256; i++) {
			const char* str = (const char*)&m_file_buf[off];
			off += strlen(str) + 1;
		}
		kallsyms_token_table_start = x;
		kallsyms_token_table_end = off;
		return true;
	}
	return false;
}

bool KallsymsLookupName_4_6_0::find_kallsyms_token_index(size_t kallsyms_token_table_end, size_t& kallsyms_token_index_start) {
	size_t start = align_up<8>(kallsyms_token_table_end);
	const int var_len = sizeof(short);
	for (auto x = start; x + var_len < m_file_buf.size(); x += var_len) {
		short val1 = *(short*)&m_file_buf[x];
		short val2 = *(short*)&m_file_buf[x + var_len];
		if (val1 == 0 && val2 > 0) {
			kallsyms_token_index_start = x;
			break;
		}
		else if (val1 == 0 && val2 == 0) {
			continue;
		}
		return false;
	}
	return true;
}

bool KallsymsLookupName_4_6_0::resolve_kallsyms_addresses_symbol_base(size_t code_static_start, uint64_t& base_address) {
	if (!has_kallsyms_symbol("_stext")) {
		return false;
	}
	if (has_kallsyms_symbol("_text")) {
		base_address = kallsyms_lookup_name("_text");
		return true;
	}
	uint64_t _stext_addr = kallsyms_lookup_name("_stext");
	base_address = _stext_addr - code_static_start;
	return true;
}

bool KallsymsLookupName_4_6_0::resolve_kallsyms_offset_symbol_base(size_t code_static_start, int& base_off) {
	if (!has_kallsyms_symbol("_stext")) {
		return false;
	}
	if (has_kallsyms_symbol("_text")) {
		base_off = kallsyms_lookup_name("_text");
		return true;
	}
	size_t _stext_offset = kallsyms_lookup_name("_stext");
	base_off = code_static_start - _stext_offset;
	return true;
}

/*
 * Expand a compressed symbol data into the resulting uncompressed string,
 * if uncompressed string is too long (>= maxlen), it will be truncated,
 * given the offset to where the symbol is in the compressed stream.
 */
unsigned int KallsymsLookupName_4_6_0::kallsyms_expand_symbol(unsigned int off, char* result, size_t maxlen)
{
	int len, skipped_first = 0;
	const char* tptr;
	const uint8_t* data;

	/* Get the compressed symbol length from the first symbol byte. */

	data = (uint8_t*)&m_file_buf[m_kallsyms_names.offset + off * sizeof(uint8_t)];

	len = *data;
	data++;

	/*
	 * Update the offset to return the offset for the next symbol on
	 * the compressed stream.
	 */
	off += len + 1;

	/*
	 * For every byte on the compressed symbol data, copy the table
	 * entry for that byte.
	 */

	while (len) {
		uint8_t x = *data;
		short y = *(short*)&m_file_buf[m_kallsyms_token_index.offset + x * sizeof(uint16_t)];

		tptr = &m_file_buf[m_kallsyms_token_table.offset + y * sizeof(unsigned char)];
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (maxlen <= 1)
					goto tail;
				*result = *tptr;
				result++;
				maxlen--;
			}
			else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (maxlen)
		*result = '\0';

	/* Return to offset to the next symbol. */
	return off;
}

uint64_t KallsymsLookupName_4_6_0::kallsyms_sym_address(int idx) {
	if (!CONFIG_KALLSYMS_BASE_RELATIVE) {
		return m_kallsyms_addresses.addresses[idx].second;
	}

	///* values are unsigned offsets if --absolute-percpu is not in effect */
	//if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
	//	return kallsyms_relative_base + (u32)kallsyms_offsets[idx];


	/* ...otherwise, positive offsets are absolute values */
	int* kallsyms_offsets = (int*)&m_file_buf[m_kallsyms_offsets.offset];

	if (kallsyms_offsets[idx] >= 0)
		return kallsyms_offsets[idx];

	/* ...and negative offsets are relative to kallsyms_relative_base - 1 */
	//return m_kallsyms_relative_base - 1 - kallsyms_offsets[idx];
	return 0;
}

/* Lookup the address for this symbol. Returns 0 if not found. */
uint64_t KallsymsLookupName_4_6_0::kallsyms_lookup_name(const char* name) {
	std::unordered_map<std::string, uint64_t> syms = kallsyms_on_each_symbol();
	auto iter = syms.find(name);
	if (iter == syms.end()) {
		return 0;
	}
	return iter->second;
}

std::unordered_map<std::string, uint64_t> KallsymsLookupName_4_6_0::kallsyms_on_each_symbol() {
	if (!m_kallsyms_symbols_cache.size()) {
		for (auto i = 0, off = 0; i < m_kallsyms_num; i++) {
			char namebuf[KSYM_NAME_LEN] = { 0 };
			off = kallsyms_expand_symbol(off, namebuf, sizeof(namebuf));

			uint64_t offset = kallsyms_sym_address(i);
			
			if (!CONFIG_KALLSYMS_BASE_RELATIVE) {
				offset -= m_kallsyms_addresses.base_address;
			} else {
				offset += m_kallsyms_offsets.base_off;
			}

			m_kallsyms_symbols_cache[namebuf] = offset;
		}
	}
	return m_kallsyms_symbols_cache;
}

bool KallsymsLookupName_4_6_0::has_kallsyms_symbol(const char* name) {
	std::unordered_map<std::string, uint64_t> syms = kallsyms_on_each_symbol();
	auto iter = syms.find(name);
	return iter != syms.end();
}
