﻿#ifndef _KERNEL_ROOT_KIT_INIT64_HELPER_H_
#define _KERNEL_ROOT_KIT_INIT64_HELPER_H_
#include <unistd.h>
#include <thread>
#include <atomic>
#include "kernel_root_kit_process64_inject.h"
#include "kernel_root_kit_process_cmdline_utils.h"

namespace kernel_root {
//注入init64进程远程执行命令
static std::string run_init64_cmd_wrapper(
	const char* str_root_key,
	const char *cmd,
	ssize_t & out_err) {
	pid_t target_pid = 1;
	std::string str_cmd_result = inject_process64_run_cmd_wrapper(str_root_key, target_pid,
		cmd, out_err, false);
	if(out_err != ERR_NONE) {
		return {};
	}
	return str_cmd_result;
}

//fork安全版本（可用于安卓APP直接调用）
static std::string safe_run_init64_cmd_wrapper(
	const char* str_root_key,
	const char *cmd,
	ssize_t & out_err) {
	std::string str_cmd_result;
	if (cmd == NULL || strlen(cmd) == 0) { return {}; }

	out_err = ERR_NONE;
	fork_pipe_info finfo;
	if (fork_pipe_child_process(finfo)) {
		str_cmd_result = run_init64_cmd_wrapper(
			str_root_key,
			cmd,
			out_err);
		write_errcode_from_child(finfo, out_err);
		write_string_from_child(finfo, str_cmd_result);
		_exit(0);
		return {};
	}
	if (!wait_fork_child_process(finfo)) {
		out_err = ERR_WAIT_FORK_CHILD;
	} else {
		if (!read_errcode_from_child(finfo, out_err)) {
			out_err = ERR_READ_CHILD_ERRCODE;
		} else if (!read_string_from_child(finfo, str_cmd_result)) {
			out_err = ERR_READ_CHILD_STRING;
		}
	}
	return str_cmd_result;
}
}
#endif /* _KERNEL_ROOT_KIT_INIT64_HELPER_H_ */
