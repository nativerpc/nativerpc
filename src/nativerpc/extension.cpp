/**
 *  Native RPC C++ Language Extensions
 *
 *      Any
 *      ClassType
 *      Typing
 *      terminateWithTrace
 *      getTempFileName
 *      execProcess
 *      ChunkManager
 *      ChunkClass
 *      leftTrim
 *      rightTrim
 *      findStringIC
 *      toLowerStr
 *      getSubString
 *      parseInt
 *      getSocketHost
 *      getTime
 *      findIndex
 *      replaceAll
 *      getHeaderMap
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <Ws2tcpip.h>
#include <crtdbg.h>
#include <winsock2.h>

#pragma pack(1)
#include "extension.h"
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stacktrace>
#include <string.h>
#include <string>

namespace nativerpc {
ClassType::ClassType() {
    className = "";
    isComplex = true;
    dataSize = 0;
}

ClassType::ClassType(std::string name, bool complex, int size) {
    className = name;
    isComplex = complex;
    dataSize = size;
}

std::map<std::string, std::shared_ptr<Any> (*)()> Typing::_typeCreators;

void terminateWithTrace() {
    try {
        throw;
    } catch (const std::exception &e) {
        std::cerr << "Unhandled Exception: " << e.what() << std::endl;
        std::stacktrace st = std::stacktrace::current(0);
        std::cerr << st << std::endl;
    } catch (...) {
        std::cerr << "Unhandled Exception: " << std::endl;
        std::stacktrace st = std::stacktrace::current(0);
        std::cerr << st << std::endl;
    }
    std::exit(-1);
}

class TerminateWithTrace {
  public:
    TerminateWithTrace() {
        _CrtSetReportHook(&TerminateWithTrace::terminateWithException);
        _CrtSetReportMode(_CRT_ASSERT, 0);
        _CrtSetReportMode(_CRT_ERROR, 0);
        _CrtSetReportMode(_CRT_WARN, 0);
        std::set_terminate(terminateWithTrace);
    }

    static int terminateWithException(int reportType, char *message, int *returnValue) {
        if (returnValue) {
            *returnValue = 0;
        }
        throw std::runtime_error(message);
        return TRUE;
    }
} g_terminateWithTrace;

std::string getTempFileName(std::string name) {
    std::ostringstream ss;
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    temp_dir += name;
    ss << temp_dir.string();
    return ss.str();
}

void execShell(std::string command) {
    auto result = system(command.c_str());
    if (result != 0) {
        exit(result);
    }
}

std::string execProcess(std::string command, std::string cwd, bool allowFail) {
    auto last = std::filesystem::current_path();
    if (cwd != ".") {
        std::filesystem::current_path(cwd);
    }

    // See also: C:\Users\XXX\AppData\Local\Temp\nrpc-stderr.txt
    const auto err_file = getTempFileName("nrpc-stderr.txt");
    std::array<char, 1024> buffer;
    std::string result = "";
    std::string cmd = command + " 2>" + err_file;
#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "r");
#else
    FILE *pipe = popen(cmd.c_str(), "r");
#endif

    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (std::fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    std::ostringstream resp;
    std::ifstream err_stream(err_file);
    if (err_stream.is_open()) {
        std::string line;
        while (std::getline(err_stream, line)) {
            resp << line << std::endl;
        }
        err_stream.close();
    }
    int status = 0;
#ifdef _WIN32
    status = _pclose(pipe);
#else
    status = pclose(pipe);
#endif
    std::filesystem::current_path(last);

    if (status != 0) {
        if (allowFail) {
            if (result.size()) {
                rightTrim(result);
                std::cerr << result << std::endl;
            }
            auto temp = resp.str();
            rightTrim(temp);
            if (temp.size()) {
                std::cerr << temp << std::endl;
            }
            return "";
        } else {
            std::cerr << "Execution failed: code=" << status << ", cmd='" << command << "'" << std::endl;
            std::cerr << result << std::endl << resp.str() << std::endl;
            throw std::exception("Remote execution error");
        }
    }
    return result;
}

void leftTrim(std::string &s) {
    auto first = 0;
    while (first < s.size() && std::isspace(s[first])) {
        first++;
    }
    s.erase(s.begin(), s.begin() + first);
}

void rightTrim(std::string &s) {
    auto last = s.size();
    while (last > 0 && std::isspace(s[last - 1])) {
        last--;
    }
    s.erase(s.begin() + last, s.end());
}

int findStringIC(const std::string &text, const std::string &needle, int start, bool allowMissing) {
    assert(start <= text.size());
    auto it = std::search(text.begin() + start, text.end(), needle.begin(), needle.end(), [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    if (!allowMissing && it == text.end()) {
        throw std::exception("Substring not found");
    }
    return it != text.end() ? it - text.begin() : -1;
}

std::string toLowerStr(const std::string &str) {
    std::string result;
    std::transform(str.begin(), str.end(), std::back_inserter(result), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string getSubString(const std::string &text, int start, int end, bool trim) {
    assert(end >= start);
    assert(end <= text.size());
    std::string result = text.substr(start, end - start);
    if (trim) {
        leftTrim(result);
        rightTrim(result);
    }
    return result;
}

int parseInt(std::string value) { return std::atoi(value.c_str()); }

std::tuple<std::string, int> getSocketHost(int socket) {
    struct sockaddr_in ss;
    socklen_t len = sizeof(ss);

    if (getpeername(socket, (struct sockaddr *)&ss, &len)) {
        throw std::exception("Peer name issue");
    }

    auto client_ip = std::string(inet_ntoa(ss.sin_addr));
    auto client_port = ntohs(ss.sin_port);
    return std::make_tuple(client_ip, client_port);
}

int64_t getTime() {
    auto now = std::chrono::system_clock::now();
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

int findIndex(std::vector<uint8_t> &data, const char *needle) {
    auto found = std::search(data.begin(), data.end(), (uint8_t *)needle, (uint8_t *)(needle + strlen(needle)));
    if (found == data.end()) {
        return -1;
    }
    return found - data.begin();
}

std::string replaceAll(std::string str, const std::string &from, const std::string &to) {
    size_t idx = 0;
    while ((idx = str.find(from, idx)) != std::string::npos) {
        str.replace(idx, from.length(), to);
        idx += to.length();
    }
    return str;
}

std::vector<std::string> splitString(const std::string &str, std::string delimiter, int maxCount) {
    std::vector<std::string> tokens;
    size_t startIndex = 0;
    size_t endIndex = 0;
    while ((endIndex = str.find(delimiter, startIndex)) != std::string::npos && (maxCount <= 0 || tokens.size() + 1 < maxCount)) {
        tokens.push_back(str.substr(startIndex, endIndex - startIndex));
        startIndex = endIndex + delimiter.length();
    }
    tokens.push_back(str.substr(startIndex));
    return tokens;
}

std::string joinString(std::vector<std::string> parts, std::string delimiter, int start, int last) {
    if (start >= parts.size()) {
        return "";
    }
    std::string result = parts[start];
    for (int j = start + 1; j < (last > 0 ? min(last, parts.size()) : parts.size()); j++) {
        result += " " + parts[j];
    }
    return result;
}

std::map<std::string, std::string> getHeaderMap(std::string headers, std::vector<std::string> names) {
    std::map<std::string, std::string> result;
    for (auto name : names) {
        auto colonName = name + ":";
        if (findStringIC(headers, colonName, 0, true) == -1) {
            result[name] = "";
            continue;
        }
        auto startIndex = findStringIC(headers, colonName, 0, false) + colonName.size();
        auto endIndex = findStringIC(headers, "\n", startIndex, true);
        result[name] = getSubString(headers, startIndex, endIndex != -1 ? endIndex : headers.length(), true);
    }
    return result;
}

std::tuple<std::string, std::string, nlohmann::json> makeRequest(int socket, std::string buffer) {
    auto res = send(socket, buffer.c_str(), buffer.size(), 0);
    assert(res == buffer.size());

    std::vector<uint8_t> readBuffer;
    uint8_t inputBuffer[10000];
    std::string headers = "";
    std::tuple<std::string, std::string> status;
    nlohmann::json payload = nullptr;

    while (true) {
        // Read
        // See also SO_RCVTIMEO
        auto received = recv(socket, (char *)inputBuffer, sizeof(inputBuffer), 0);
        if (received > 0) {
            readBuffer.insert(readBuffer.end(), inputBuffer, inputBuffer + received);
        }
        auto middleIndex = received > 0 ? findIndex(readBuffer, "\r\n\r\n") : -1;

        // Parse headers and payload
        headers = "";
        status = std::make_tuple(std::string(), std::string());
        payload = nullptr;
        if (received > 0 && middleIndex != -1) {
            headers = std::string((char *)readBuffer.data(), middleIndex);
            auto msgLen = parseInt(getHeaderMap(headers, {"Content-Length"})["Content-Length"]);
            assert(msgLen >= 0);
            if (middleIndex + 4 + msgLen <= readBuffer.size()) {
                std::string newLine = "\r\n";
                auto firstLine = std::search(headers.begin(), headers.end(), newLine.begin(), newLine.end()) - headers.begin();
                auto parts = splitString(headers.substr(0, firstLine), " ", 3);
                if (parts.size() != 3) {
                    throw std::runtime_error("Failed to parse http status");
                }
                status = std::make_tuple(parts[1], parts[2]);
                payload = nlohmann::json::parse(std::string((char *)readBuffer.data() + middleIndex + 4, msgLen));
                readBuffer.erase(readBuffer.begin(), readBuffer.begin() + middleIndex + 4 + msgLen);
            }
        }

        if (received < 0) {
            if (WSAGetLastError() == WSA_WAIT_TIMEOUT) {
                // pass
            } else {
                throw std::runtime_error(std::string() + "Server socket closed, code=" + std::to_string(WSAGetLastError()));
            }
        } else if (received == 0) {
            throw std::runtime_error("Server socket closed");
        } else if (payload != nullptr) {
            return std::make_tuple(std::get<0>(status), std::get<1>(status), payload);
        }
    }
}

void setEnvVar(std::string name, std::string value) {
#if defined(__unix__) || defined(__APPLE__)
    if (setenv(name.c_str(), value.c_str(), 1) != 0) {
        std::cerr << "Failed to set environment variable (setenv)" << std::endl;
    }
#elif defined(_WIN32)
    if (_putenv_s(name.c_str(), value.c_str()) != 0) {
        std::cerr << "Failed to set environment variable (_putenv_s)" << std::endl;
    }
#else
    std::cerr << "Environment variable setting not supported on this platform" << std::endl;
#endif
}
} // namespace nativerpc