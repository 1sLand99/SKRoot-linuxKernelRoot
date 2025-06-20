﻿#include "symbol_analyze.h"

SymbolAnalyze::SymbolAnalyze(const std::vector<char>& file_buf) : m_file_buf(file_buf), m_kernel_sym_parser(file_buf)
{
}

SymbolAnalyze::~SymbolAnalyze()
{
}

bool SymbolAnalyze::analyze_kernel_symbol() {
	if (!m_kernel_sym_parser.init_kallsyms_lookup_name()) {
		std::cout << "Failed to initialize kallsyms lookup name" << std::endl;
		return false;
	}
	if (!find_symbol_offset()) {
		std::cout << "Failed to find symbol offset" << std::endl;
		return false;
	}
	return true;
}

KernelSymbolOffset SymbolAnalyze::get_symbol_offset() {
	return m_kernel_sym_offset;
}

bool SymbolAnalyze::is_kernel_version_less(const std::string& ver) const {
	return m_kernel_sym_parser.is_kernel_version_less(ver);
}

bool SymbolAnalyze::find_symbol_offset() {
	m_kernel_sym_offset._text = kallsyms_matching("_text");
	m_kernel_sym_offset._stext = kallsyms_matching("_stext");

	m_kernel_sym_offset.die = kallsyms_matching("die");
	m_kernel_sym_offset.arm64_notify_die = kallsyms_matching("arm64_notify_die");
	m_kernel_sym_offset.kernel_restart = kallsyms_matching("kernel_restart");

	m_kernel_sym_offset.__do_execve_file = kallsyms_matching("__do_execve_file");

	m_kernel_sym_offset.do_execveat_common = kallsyms_matching("do_execveat_common");
	if (m_kernel_sym_offset.do_execveat_common == 0) {
		m_kernel_sym_offset.do_execveat_common = kallsyms_matching("do_execveat_common", true);
	}

	m_kernel_sym_offset.do_execve_common = kallsyms_matching("do_execve_common");
	if (m_kernel_sym_offset.do_execve_common == 0) {
		m_kernel_sym_offset.do_execve_common = kallsyms_matching("do_execve_common", true);
	}

	m_kernel_sym_offset.do_execveat = kallsyms_matching("do_execveat");
	m_kernel_sym_offset.do_execve = kallsyms_matching("do_execve");


	m_kernel_sym_offset.avc_denied = kallsyms_matching("avc_denied");
	if (m_kernel_sym_offset.avc_denied == 0) {
		m_kernel_sym_offset.avc_denied = kallsyms_matching("avc_denied", true);
	}
	m_kernel_sym_offset.filldir64 = kallsyms_matching("filldir64", true);
	m_kernel_sym_offset.freeze_task = kallsyms_matching("freeze_task");

	m_kernel_sym_offset.revert_creds = kallsyms_matching("revert_creds");
	m_kernel_sym_offset.prctl_get_seccomp = kallsyms_matching("prctl_get_seccomp"); // backup: seccomp_filter_release
	 
	
	m_kernel_sym_offset.__cfi_check = kallsyms_matching("__cfi_check");
	m_kernel_sym_offset.__cfi_check_fail = kallsyms_matching("__cfi_check_fail");
	m_kernel_sym_offset.__cfi_slowpath_diag = kallsyms_matching("__cfi_slowpath_diag");
	m_kernel_sym_offset.__cfi_slowpath = kallsyms_matching("__cfi_slowpath");
	m_kernel_sym_offset.__ubsan_handle_cfi_check_fail_abort = kallsyms_matching("__ubsan_handle_cfi_check_fail_abort");
	m_kernel_sym_offset.__ubsan_handle_cfi_check_fail = kallsyms_matching("__ubsan_handle_cfi_check_fail");
	m_kernel_sym_offset.report_cfi_failure = kallsyms_matching("report_cfi_failure");
	return (m_kernel_sym_offset.do_execve || m_kernel_sym_offset.do_execveat || m_kernel_sym_offset.do_execveat_common) 
		&& m_kernel_sym_offset.avc_denied
		&& m_kernel_sym_offset.filldir64
		&& m_kernel_sym_offset.freeze_task
		&& m_kernel_sym_offset.revert_creds
		&& m_kernel_sym_offset.prctl_get_seccomp;
}

uint64_t SymbolAnalyze::kallsyms_matching(const char* name, bool fuzzy) {
	if (fuzzy) {
		auto map = m_kernel_sym_parser.kallsyms_lookup_names_like(name);
		if (map.size()) {
			return map.begin()->second;
		}
		return 0;
	}
	return m_kernel_sym_parser.kallsyms_lookup_name(name);
}