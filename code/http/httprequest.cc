#include "httprequest.h"

using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::init() {
    parseState_ = PARSE_STATE::REQUEST_LINE;
    lineState_ = LINE_STATE::LINE_OK;
    method_ = "";
    path_ = "";
    version_ = "";
    body_ = "";
    header_.clear();
    post_.clear();
}

HttpRequest::HTTP_CODE HttpRequest::parse(Buffer& buff) {
    string line;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    while (getLine(buff, line) == LINE_OK) {
        switch(parseState_) {
        case REQUEST_LINE:
            ret = parseRequestLine_(line);
            buff.retrive(line.length() + 2);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;
        case HEADERS:
            ret = parseHeader_(line);
            buff.retrive(line.length() + 2);
            if (ret != NO_REQUEST) {
                // GET_REQUEST or BAD_REQUEST
                return ret;
            }
            break;
        case BODY:
            ret = parseBody_(line);
            buff.retrive(line.length());
            return ret;
            break;
        default:
            break;
        }
    }
    // 表示需要继续读入
    return NO_REQUEST;
}

HttpRequest::LINE_STATE HttpRequest::getLine(Buffer& buff, string& line) {
    static const char CRLF[] = "\r\n";
    if (parseState_ != BODY) {
        const char* lineEnd = search(buff.peek(), static_cast<const char*>(buff.beginWrite()), CRLF, CRLF + 2);
        if (lineEnd == buff.beginWrite()) {
            // 没有找到\r\n
            line = "";
            lineState_ = LINE_STATE::LINE_OPEN;
        } else {
            line = string(buff.peek(), lineEnd);
            lineState_ = LINE_STATE::LINE_OK;
        }
    } else {
        if (header_.count(HttpHeader::content_length) == 0) {
            lineState_ = LINE_STATE::LINE_ERROR;
        } else {
            size_t contentLength = stoi(header_[HttpHeader::content_length]);
            if (buff.readableBytes() < contentLength) {
                line = "";
                lineState_ = LINE_STATE::LINE_OPEN;
            } else {
                line = string(buff.peek(), buff.peek() + contentLength);
                lineState_ = LINE_STATE::LINE_OK;
            }
        }
    }
    return lineState_;
}


string HttpRequest::getPost(const string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

string HttpRequest::getPost(const char* key) const {
    assert(key != nullptr);
    return getPost(string(key));
}

bool HttpRequest::isKeepAlive() const {
    if(header_.count(HttpHeader::connection) == 1) {
        return header_.find(HttpHeader::connection)->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

HttpRequest::HTTP_CODE HttpRequest::parseRequestLine_(const string& line) {
    regex pattern("^([a-zA-Z]+) ([^ ]+) HTTP/([^ ]+)$");
    smatch subMatch;
    if(regex_match(line, subMatch, pattern)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        parsePath_();
        parseState_ = PARSE_STATE::HEADERS;
    } else {
        Log_Error("parseRequestLine Error, line: %s", line.c_str());
        return BAD_REQUEST;
    }
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::parseHeader_(const string& line) {
    if (line.empty()) {
        if (header_.count(HttpHeader::content_length) == 0 || stoi(header_.at(HttpHeader::content_length)) == 0) {
            return GET_REQUEST;
        } else {
            parseState_ = PARSE_STATE::BODY;
            return NO_REQUEST;
        }
    }
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    } else {
        Log_Error("parseHeader Error, line: %s", line.c_str());
        return BAD_REQUEST;
    }
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::parseBody_(const string& line) {
    body_ = line;
    return parsePost_();
}

void HttpRequest::parsePath_() {
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}


// TODO: read code and modify return value check
HttpRequest::HTTP_CODE HttpRequest::parsePost_() {
    if(method_ == "POST" && header_[HttpHeader::content_type] == "application/x-www-form-urlencoded") {
        parseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            Log_Debug("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
    return GET_REQUEST;
}

// TODO: read
void HttpRequest::parseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    int n = body_.size();
    string processed_body;
    for (int i = 0; i < n; i++) {
        if (body_[i] == '+') processed_body += ' ';
        else if (body_[i] == '%') {
            int num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            char ch = static_cast<char>(num);
            i += 2;
            processed_body += ch;
        } else {
            processed_body += body_[i];
        }
    }
    body_ = std::move(processed_body);
    string key, value;
    n = body_.size();
    int num = 0;
    int i = 0, j = 0;
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            Log_Debug("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}



// TODO: read code
bool HttpRequest::UserVerify(const std::string& name, const std::string pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    Log_Info("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    SqlConnRAII conn(SqlConnPool::Instance());
    assert(conn.get() != nullptr);
    bool flag = false;
    char order[256] = { 0 };
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    Log_Debug("%s", order);

    if(mysql_query(conn.get(), order)) { 
        // mysql_free_result(res);
        return false; 
    }
    MYSQL_RES *res = mysql_store_result(conn.get());
    unsigned int j = mysql_num_fields(res);
    MYSQL_FIELD *fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        Log_Debug("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                Log_Debug("pwd error!");
            }
        } 
        else { 
            flag = false; 
            Log_Debug("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        Log_Debug("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        Log_Debug("%s", order);
        if(mysql_query(conn.get(), order)) { 
            Log_Debug( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    Log_Debug( "UserVerify success!!");
    return flag;
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch - '0';
}

