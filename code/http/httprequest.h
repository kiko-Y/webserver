#ifndef _HTTPREQUEST_H_
#define _HTTPREQUEST_H_

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"
#include "httpheader.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE = 0,
        HEADERS,
        BODY,
    };

    enum LINE_STATE {
        LINE_OK = 0,
        LINE_ERROR,
        LINE_OPEN,
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() { init(); };

    ~HttpRequest() = default;

    void init();

    // 解析读入数据
    // 解析成功返回 GET_REQUEST
    // 解析错误返回 BAD_REQUEST
    // 解析数据未读入完全返回 NO_REQUEST
    HTTP_CODE parse(Buffer& buff);

    std::string path() const { return path_; };
    std::string method() const { return method_; };
    std::string version() const { return version_; };
    std::string body() const { return body_; }
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;
    PARSE_STATE getMainState() { return parseState_; }
    LINE_STATE getSubState() { return lineState_; }

    bool isKeepAlive() const;

    LINE_STATE getLine(Buffer& buff, std::string& line);

private:
    HTTP_CODE parseRequestLine_(const std::string& line);
    HTTP_CODE parseHeader_(const std::string& line);
    HTTP_CODE parseBody_(const std::string& line);


    void parsePath_();
    HTTP_CODE parsePost_();
    void parseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string pwd, bool isLogin);

    PARSE_STATE parseState_;

    LINE_STATE lineState_;

    std::string method_;
    std::string path_;
    std::string version_;
    std::string body_;

    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};

#endif