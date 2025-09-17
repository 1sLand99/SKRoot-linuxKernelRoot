﻿#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <set>
#include <mutex>

#include "lib_root_server.h"
#include "lib_root_server_inline.h"
#include "index_html_gz_data.h"
#include "rootkit_umbrella.h"
#include "utils/stringUtils.h"
#include "utils/jsonUtils.h"
#include "utils/randomData.h"

#define MAX_HEARTBEAT_TIME_SEC 20
namespace {
constexpr const char* recommend_files[] = {"libc++_shared.so"};

}
std::atomic<bool> g_heartbeat{true};

class InjectSuInfo{
public:
    std::atomic<bool> working{false};
    std::atomic<bool> success{false};
    void set_app_name(const std::string & app_name) {
        std::lock_guard<std::mutex> guard(m_msgLock);
        m_app_name = app_name;
    }
    std::string getAppName() {
        std::lock_guard<std::mutex> guard(m_msgLock);
        return m_app_name;
    }
    
    void append_console_msg(const std::string & console) {
        std::lock_guard<std::mutex> guard(m_msgLock);
        m_consoleMsg = console + "\n";
    }
    void clearConsoleMsg() {
        std::lock_guard<std::mutex> guard(m_msgLock);
        m_consoleMsg = "";
    }
    std::string getConsoleMsg() {
        std::lock_guard<std::mutex> guard(m_msgLock);
        return m_consoleMsg;
    }

private:
    std::string m_app_name;
    std::string m_consoleMsg;
    std::mutex m_msgLock;
} g_inject_su_info;

std::string convert_2_json(const std::string & str, const std::map<std::string, std::string> & appendParam = {}) {
    std::string strJson;
    cJSON *json = cJSON_CreateObject();
    if(json) {
        cJSON_AddStringToObject(json, "content", str.c_str());
        for(const auto & param: appendParam) {
            cJSON_AddStringToObject(json, param.first.c_str(), param.second.c_str());
        }
        char *jsonString = cJSON_Print(json);
        if(jsonString) {
            strJson = jsonString;
            free(jsonString);
        }
        cJSON_Delete(json);
    }
    return strJson;
}

std::string convert_2_json_m(const std::string & str, const std::map<std::string, std::string> & appendParam = {}) {
    std::string strJson;
    cJSON *json = cJSON_CreateObject();
    if(json) {
        cJSON_AddStringToObject(json, "content", str.c_str());
        cJSON *jsonArray = cJSON_CreateArray();
        for(const auto & param: appendParam) {
            cJSON *jsonMap = cJSON_CreateObject();
            cJSON_AddStringToObject(jsonMap, param.first.c_str(), param.second.c_str());
            cJSON_AddItemToArray(jsonArray, jsonMap);
        }
        cJSON_AddItemToObject(json, "arr_map", jsonArray);

        char *jsonString = cJSON_Print(json);
        if(jsonString) {
            strJson = jsonString;
            free(jsonString);
        }
        cJSON_Delete(json);
    }
    return strJson;
}

std::string convert_2_json_v(const std::vector<std::string> &v, const std::map<std::string, std::string> & appendParam = {}) {
    std::string strJson;
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON *jsonArray = cJSON_CreateArray();
        for (const std::string &str : v) {
            cJSON_AddItemToArray(jsonArray, cJSON_CreateString(str.c_str()));
        }
        cJSON_AddItemToObject(json, "content", jsonArray);
        for(const auto & param: appendParam) {
            cJSON_AddStringToObject(json, param.first.c_str(), param.second.c_str());
        }
        char *jsonString = cJSON_Print(json);
        if (jsonString) {
            strJson = jsonString;
            free(jsonString);
        }

        cJSON_Delete(json);
    }
    return strJson;
}

std::tuple<std::string, bool> handle_index() {
	std::string gzip_html;
	gzip_html.assign(reinterpret_cast<const char*>(lib_root_server::index_html_gz_data), lib_root_server::index_html_gz_size);
	return { gzip_html, true };
}

std::string handle_heartbeat(const std::string & userName) {
    g_heartbeat = true;
    return convert_2_json(userName);
}

std::string handle_test_root() {
    std::stringstream sstr;
    sstr << "get_root:" <<  kernel_root::get_root(ROOT_KEY.c_str()) << std::endl << std::endl;
    sstr << kernel_root::get_capability_info();
    return convert_2_json(sstr.str());
}

std::string handle_run_root_cmd(const std::string & cmd) {
    ssize_t err = ERR_NONE;
    std::string result = kernel_root::run_root_cmd(ROOT_KEY.c_str(), cmd.c_str(), err);

    std::stringstream sstr;
    sstr << "run_root_cmd err:" << err << ", result:" << result;
    return convert_2_json(sstr.str());
}

std::string handle_root_exec_process(const std::string & path) {
    ssize_t err = kernel_root::root_exec_process(ROOT_KEY.c_str(), path.c_str());

    std::stringstream sstr;
    sstr << "root_exec_processs err:" << err;
    return convert_2_json(sstr.str());
}

std::string handle_install_su() {
    ssize_t err = ERR_NONE;
    std::string su_hide_full_path = kernel_root::install_su(ROOT_KEY.c_str(), err);
    std::stringstream sstr;
    sstr << "install su err:" << err<<", su_hide_full_path:" << su_hide_full_path << std::endl;
    
    if (err == ERR_NONE) {
        sstr << "installSu done."<< std::endl;
    }
    std::map<std::string, std::string> param;
    param["su_hide_full_path"] = su_hide_full_path;
    param["err"] = std::to_string(err);
    return convert_2_json(sstr.str(), param);
}

std::string handle_uninstall_su() {
   
    ssize_t err = kernel_root::uninstall_su(ROOT_KEY.c_str());
    std::stringstream sstr;
    sstr << "uninstallSu err:" << err << std::endl;
    if (err != ERR_NONE) {
        return convert_2_json(sstr.str());
    }
    sstr << "uninstallSu done.";
    
    std::map<std::string, std::string> param;
    param["err"] = std::to_string(err);
    return convert_2_json(sstr.str(), param);
}



std::string handle_get_app_list(bool isShowSystemApp, bool isShowThirtyApp, bool isShowRunningAPP) {
    std::vector<std::string> packageNames;
    std::string cmd;
    if(isShowSystemApp && isShowThirtyApp) {
        cmd = "pm list packages";
    } else if(isShowSystemApp) {
        cmd = "pm list packages -s";
    } else if(isShowThirtyApp) {
        cmd = "pm list packages -3";
    }
    ssize_t err = ERR_NONE;
    std::string packages = kernel_root::run_root_cmd(ROOT_KEY.c_str(), cmd.c_str(), err);
    if(err != 0) {
        return convert_2_json_v(packageNames);
    }

    std::map<pid_t, std::string> pid_map;
    if(isShowRunningAPP) {
        err = kernel_root::get_all_cmdline_process(ROOT_KEY.c_str(), pid_map);
        if(err != 0) {
            return convert_2_json_v(packageNames);
        }
        
    }    
    // remove "package:" flag
    std::istringstream iss(packages);
    std::string line;
    while (getline(iss, line)) {
        size_t pos = line.find("package:");
        if (pos != std::string::npos) {
            line.erase(pos, std::string("package:").length());
        }
        if(isShowRunningAPP) {

            bool isFound = false;
            for(auto & item : pid_map) {
                if(item.second.find(line) != std::string::npos) {
                    isFound = true;
                    break;
                }
            }
            if(!isFound) {
                continue;
            }
        }
        packageNames.push_back(line);
    }
    return convert_2_json_v(packageNames);
}

void inject_su_thread() {
    writeToLog("inject_su_thread enter");

	std::string su_folder = kernel_root::get_su_hide_folder_path_string(ROOT_KEY.c_str());
    g_inject_su_info.append_console_msg("su_folder ret val:" + su_folder);

    // 1.杀光所有历史进程
    std::set<pid_t> out;
    ssize_t err = kernel_root::find_all_cmdline_process(ROOT_KEY.c_str(), g_inject_su_info.getAppName().c_str(), out);
    g_inject_su_info.append_console_msg("find_all_cmdline_process err:" + std::to_string(err) + ", cnt:" + std::to_string(out.size()));
    if (err) {
        g_inject_su_info.working = false;
        return;
    }

    for (pid_t pid : out) { kernel_root::kill_process(ROOT_KEY.c_str(), pid); }

	// 2.注入su环境变量到指定进程
	g_inject_su_info.append_console_msg("waiting for process creation:" + g_inject_su_info.getAppName());

    pid_t pid;
	err = kernel_root::wait_and_find_cmdline_process(
		ROOT_KEY.c_str(), g_inject_su_info.getAppName().c_str(), 60 * 1000, pid);
    g_inject_su_info.append_console_msg("waiting for process creation err:" + std::to_string(err));
    if (err) {
        g_inject_su_info.working = false;
        return;
    }
	err = kernel_root::inject_process_env64_PATH_wrapper(ROOT_KEY.c_str(), pid,
														 su_folder.c_str(), kernel_root::api_offset_read_mode::only_read_file);
    g_inject_su_info.append_console_msg("inject su err:" + std::to_string(err) + ", errmsg:" + strerror(errno));
    g_inject_su_info.success = true;
    g_inject_su_info.working = false;
}

std::string handle_inject_su_in_temp_app(const std::string & app_name) {
    std::map<std::string, std::string> param;
    param["errcode"] = "0";
    if(g_inject_su_info.working) {
        param["errcode"] = "-1";
        return convert_2_json("inject su thread already running.", param);
    }
    
    if(app_name.empty()) {
        param["errcode"] = "-2";
        return convert_2_json("app name is empty.", param);
    }
    g_inject_su_info.set_app_name(app_name);
    writeToLog("start inject su thread, app name: " + app_name);
    g_inject_su_info.working = true;
    g_inject_su_info.success = false;
    std::thread td(inject_su_thread);
    td.detach();
    return convert_2_json("ok", param);
}

std::string handle_get_inject_su_in_temp_app_result() {
    //writeToLog("handle_get_inject_su_result enter");
    std::map<std::string, std::string> param;
    param["working"] = g_inject_su_info.working ? "1" : "0";
    param["success"] = g_inject_su_info.success ? "1" : "0";

    std::string console = g_inject_su_info.getConsoleMsg();
    g_inject_su_info.clearConsoleMsg();

    return convert_2_json(console, param);
}


std::string handle_get_precheck_app_file_list(const std::string & app_name) {
    std::stringstream errmsg;
    std::map<std::string, std::string> output_file_path;
    std::set<pid_t> pid_arr;
	ssize_t err = kernel_root::find_all_cmdline_process(ROOT_KEY.c_str(), app_name.c_str(), pid_arr);
	if (err) {
        errmsg << "find_all_cmdline_process err:" << err << std::endl;
		return convert_2_json_m(errmsg.str());
	}
	if (pid_arr.size() == 0) {
        errmsg << "请先运行目标APP: " << app_name.c_str() << std::endl;
		return convert_2_json_m(errmsg.str());
	}

    std::map<std::string, kernel_root::app_so_status> so_path_list;
	err = kernel_root::parasite_precheck_app(ROOT_KEY.c_str(), app_name.c_str(), so_path_list);
    if (err) {
        errmsg << "parasite_precheck_app error:" << err << std::endl;
		if(err == ERR_EXIST_32BIT) {
            errmsg << "此目标APP为32位应用，无法寄生" << err << std::endl;
		}
        return convert_2_json_m(errmsg.str());
	}
	if (!so_path_list.size()) {
        errmsg << "无法检测到目标APP的JNI环境，目标APP暂不可被寄生；您可重新运行目标APP后重试；或将APP进行手动加固(加壳)，因为加固(加壳)APP后，APP会被产生JNI环境，方可寄生！" << err << std::endl;
		return convert_2_json_m(errmsg.str());
	}
	
	std::vector<std::tuple<std::string, kernel_root::app_so_status>> sort_printf;
	for (const auto& item : so_path_list) {
		if(item.second != kernel_root::app_so_status::running) {
			continue;
		}
		sort_printf.push_back({item.first, item.second});
	}
	for (const auto& item : so_path_list) {
		if(item.second != kernel_root::app_so_status::not_running) {
			continue;
		}
		sort_printf.push_back({item.first, item.second});
	}
	for (const auto& item : sort_printf) {
		auto file_path = std::get<0>(item);
		auto app_so_status = std::get<1>(item);
		std::filesystem::path filePath(file_path);
		std::string status = app_so_status == kernel_root::app_so_status::running ? " (正在运行)" : " (未运行)";
		if(app_so_status == kernel_root::app_so_status::running) {
            std::string file_name = filePath.filename().string();
			for(auto x = 0; x < sizeof(recommend_files) / sizeof(recommend_files[0]); x++) {
				if(file_name == recommend_files[x]) {
					status = " (推荐， 正在运行)";
				}
			}
		}
        output_file_path[file_path] = status;
	}
    return convert_2_json_m(errmsg.str(), output_file_path);
}

std::string handle_inject_su_in_forever_app(const std::string & app_name, const std::string & so_path) {
    std::stringstream errmsg;
    std::map<std::string, std::string> param;
    param["errcode"] = "0";
    
	ssize_t err;
	std::string su_hide_path = kernel_root::install_su(ROOT_KEY.c_str(), err);
	if (su_hide_path.empty()) {
        param["errcode"] = "-1";
        return convert_2_json_m("su_hide_path is empty");
    }

    std::set<pid_t> pid_arr;
	err = kernel_root::find_all_cmdline_process(ROOT_KEY.c_str(), app_name.c_str(), pid_arr);
	if (err) {
        param["errcode"] = "-2";
        errmsg << "find_all_cmdline_process err:" << err << std::endl;
		return convert_2_json(errmsg.str());
	}
	if (pid_arr.size() == 0) {
        param["errcode"] = "-3";
        errmsg << "请先运行目标APP: " << app_name.c_str() << std::endl;
		return convert_2_json(errmsg.str());
	}

	err = kernel_root::parasite_implant_su_env(ROOT_KEY.c_str(), app_name.c_str(), so_path.c_str(), su_hide_path);
	printf("parasite_implant_su_env err:%zd\n", err);
	if(err) {
        param["errcode"] = "-4";
        std::string msg = "parasite_implant_su_env err:" + std::to_string(err);
        return convert_2_json(msg);
    }
	for (pid_t pid : pid_arr) { kernel_root::kill_process(ROOT_KEY.c_str(), pid); }
    return convert_2_json("ok", param);
}

std::string handle_unknow_type() {
    return convert_2_json("unknow command type.");
}

std::string handle_post_action(std::string_view post_data) {
    std::string type, cmd, userName, name, subname;
    bool showSystemApp = false, showThirdApp = false, showRunningApp = false;
    std::string client_json = GetMiddleJsonString(post_data);
	// printf("responseJson:%s\n", responseJson.c_str());

    cJSON* parsed_json = cJSON_Parse(client_json.c_str());
    if (!parsed_json) {
        return handle_unknow_type();
    }
    cJSON* j_type = cJSON_GetObjectItem(parsed_json, "type");
    cJSON* j_userName = cJSON_GetObjectItem(parsed_json, "userName");
    if (!j_type || !j_userName) {
        return handle_unknow_type();
    }
    type = j_type->valuestring;
    userName = j_userName->valuestring;


    cJSON* j_cmd = cJSON_GetObjectItem(parsed_json, "cmd");
    if(j_cmd) {
        cmd = j_cmd->valuestring;
    }

    cJSON* j_showSystemApp = cJSON_GetObjectItem(parsed_json, "showSystemApp");
    if(j_showSystemApp) {
        showSystemApp = !!j_showSystemApp->valueint;
    }

    cJSON* j_showThirdApp = cJSON_GetObjectItem(parsed_json, "showThirdApp");
    if(j_showThirdApp) {
        showThirdApp = !!j_showThirdApp->valueint;
    }

    cJSON* j_showRunningApp = cJSON_GetObjectItem(parsed_json, "showRunningApp");
    if(j_showRunningApp) {
        showRunningApp = !!j_showRunningApp->valueint;
    }
    
    cJSON* j_name = cJSON_GetObjectItem(parsed_json, "name");
    if(j_name) {
        name = j_name->valuestring;
    }
    
    cJSON* j_subname = cJSON_GetObjectItem(parsed_json, "subname");
    if(j_subname) {
        subname = j_subname->valuestring;
    }

    if(type == "heartbeat") {
        return handle_heartbeat(userName);
    }

    writeToLog("Handle post action:" + type);
    if(type == "testRoot") {
        return handle_test_root();
    }
    if(type == "runRootCmd") {
        return handle_run_root_cmd(cmd);
    }
    if(type == "rootExecProc") {
        return handle_root_exec_process(cmd);
    }
    if(type == "installSu") {
        return handle_install_su();
    }
    if(type == "uninstallSu") {
        return handle_uninstall_su();
    }
    if(type == "getAppList") {
        return handle_get_app_list(showSystemApp, showThirdApp, showRunningApp);
    }
    if(type == "injectSuInTempApp") {
        return handle_inject_su_in_temp_app(name);
    }
    if(type == "getInjectSuInTempAppResult") {
        return handle_get_inject_su_in_temp_app_result();
    }
    if(type == "getPrecheckAppFileList") {
        return handle_get_precheck_app_file_list(name);
    }
    if(type == "injectSuInForeverApp") {
        return handle_inject_su_in_forever_app(name, subname);
    }
    return handle_unknow_type();
}

void handle_client(int client_socket) {
    if (client_socket < 0) { return; } 

    // Set a timeout for the receive operation
    struct timeval timeout;
    timeout.tv_sec = 3;  // 0 seconds
    timeout.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(client_socket);
        return;
    }

    char buffer[4096] = {0};
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
     	std::string request(buffer, bytes_read);
        std::string response_body;
        bool append_gzip = false;
        if (request.length() > 3 && request.substr(0, 3) == "GET") {
            std::tie(response_body, append_gzip) = handle_index();
        } else if (request.length() > 4 && request.substr(0, 4) == "POST") {
            response_body = handle_post_action(request);
        }
		
		if(response_body.length()) {
            // Note: I'm assuming that the first part of each "handle_" function result
            // is the header, and the rest is the body. This could be modified based on actual structure.
            std::string response_header = GetHttpHead_200(response_body.length(), append_gzip);

            // Send header
            send(client_socket, response_header.c_str(), response_header.length(), 0);

            // Send body
            send(client_socket, response_body.c_str(), response_body.length(), 0);
		}
    }

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
}

int server_main() {
    writeToLog("server_main enter");
	int server_socket, client_socket;
	struct sockaddr_in server_addr;
	socklen_t client_len;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		// std::cerr << "Could not create socket." << std::endl;
        writeToLog("Could not create socket.");
		return -1;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(PORT);

   	int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(server_socket, (struct sockaddr*)&server_addr,
			 sizeof(server_addr)) < 0) {
        writeToLog("Bind failed.");
		return -1;
	}
	listen(server_socket, 5);

    writeToLog("Server listening");
    for (;;) {
        client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }
    writeToLog("Server close");
	close(server_socket);
	return 0;
}

void run_server_thread() {
    server_main();
}

void open_server_url(int port) {
    ssize_t err = ERR_NONE;
    std::string cmd = "am start -a android.intent.action.VIEW -d http://127.0.0.1:" + std::to_string(port);
    FILE * fp = popen(cmd.c_str(), "r");
    if(fp) {
        pclose(fp);
    }
}

void open_url_thread() {
    sleep(1);
    open_server_url(PORT);
}

void ready_server_main() {
    writeToLog("ready_server_main server enter");
    if (kernel_root::get_root(ROOT_KEY.c_str()) != ERR_NONE) {
        writeToLog("ready_server_main root error");
		return;
	}
    std::thread td1(run_server_thread);
    td1.detach();
    std::thread td2(open_url_thread);
    td2.detach();
    while (g_heartbeat) {
        g_heartbeat = false;
        usleep(1000 * 1000 * MAX_HEARTBEAT_TIME_SEC);
    }
}

static inline void printf_random_data() {
    if(time(NULL) == 0) {
        for(size_t i = 0; i < random_bin::random_size; i++) {
            const char* c = reinterpret_cast<const char*>(random_bin::random_data);
            std::cout << c[i];
        }
    }
}

static bool isLoaded = false;
extern "C" void __attribute__((constructor)) root_server_entry() {
    if (isLoaded) { return; }
    isLoaded = true;

    srand(time(NULL));
    PORT = rand() % 40001 + 20000;
    ROOT_KEY = const_cast<char*>(static_inline_root_key);
    {
        pid_t pid = fork();
        if (pid == 0) {
            writeToLog("root server process running");
            ready_server_main();
            _exit(0);
            printf_random_data();
        }
    }
}

// int main(int argc, char* argv[]) {
//     std::string key = "u24kKoPVSAG1tnwlcs1PJ1qp6HtVymj60CoTgsjmMd1UALve";
//     memset((void*)&static_inline_root_key, 0, sizeof(static_inline_root_key));
//     memcpy((void*)&static_inline_root_key, (void*)key.c_str(), key.length() + 1);
//     PORT = 3168;

//     std::thread td1(run_server_thread);
//     td1.detach();
//     std::thread td2(open_url_thread);
//     td2.detach();
//     while (g_heartbeat) {
//         g_heartbeat = false;
//         usleep(1000 * 1000 * (MAX_HEARTBEAT_TIME_SEC / 2));
//     }
//     return 0;
// }