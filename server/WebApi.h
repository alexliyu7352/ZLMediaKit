﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBAPI_H
#define ZLMEDIAKIT_WEBAPI_H

#include <string>
#include <functional>
#include "jsoncpp/json.h"
#include "Common/Parser.h"
#include "Network/Socket.h"
#include "Http/HttpSession.h"

using namespace std;
using namespace Json;
using namespace toolkit;
using namespace mediakit;

//配置文件路径
extern string g_ini_file;

namespace mediakit {
////////////RTSP服务器配置///////////
namespace Rtsp {
extern const string kPort;
} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
extern const string kPort;
} //namespace RTMP
}  // namespace mediakit

namespace API {
typedef enum {
    NotFound = -500,//未找到
    Exception = -400,//代码抛异常
    InvalidArgs = -300,//参数不合法
    SqlFailed = -200,//sql执行失败
    AuthFailed = -100,//鉴权失败
    OtherFailed = -1,//业务代码执行失败，
    Success = 0//执行成功
} ApiErr;
}//namespace API

class ApiRetException: public std::runtime_error {
public:
    ApiRetException(const char *str = "success" ,int code = API::Success):runtime_error(str){
        _code = code;
    }
    ~ApiRetException() = default;
    int code(){ return _code; }
private:
    int _code;
};

class AuthException : public ApiRetException {
public:
    AuthException(const char *str):ApiRetException(str,API::AuthFailed){}
    ~AuthException() = default;
};

class InvalidArgsException: public ApiRetException {
public:
    InvalidArgsException(const char *str):ApiRetException(str,API::InvalidArgs){}
    ~InvalidArgsException() = default;
};

class SuccessException: public ApiRetException {
public:
    SuccessException():ApiRetException("success",API::Success){}
    ~SuccessException() = default;
};

using ApiArgsType = map<string, string, StrCaseCompare>;

template<typename Args, typename First>
string getValue(Args &args, const First &first) {
    return args[first];
}

template<typename First>
string getValue(Json::Value &args, const First &first) {
    return args[first].asString();
}

template<typename First>
string getValue(string &args, const First &first) {
    return "";
}

template<typename First>
string getValue(const Parser &parser, const First &first) {
    auto ret = parser.getUrlArgs()[first];
    if (!ret.empty()) {
        return ret;
    }
    return parser.getHeader()[first];
}

template<typename First>
string getValue(Parser &parser, const First &first) {
    return getValue((const Parser &) parser, first);
}

template<typename Args, typename First>
string getValue(const Parser &parser, Args &args, const First &first) {
    auto ret = getValue(args, first);
    if (!ret.empty()) {
        return ret;
    }
    return getValue(parser, first);
}

template<typename Args>
class HttpAllArgs {
public:
    HttpAllArgs(const Parser &parser, Args &args) {
        _get_args = [&args]() {
            return (void *) &args;
        };
        _get_parser = [&parser]() -> const Parser & {
            return parser;
        };
        _get_value = [](HttpAllArgs &that, const string &key) {
            return getValue(that.getParser(), that.getArgs(), key);
        };
        _clone = [&](HttpAllArgs &that) {
            that._get_args = [args]() {
                return (void *) &args;
            };
            that._get_parser = [parser]() -> const Parser & {
                return parser;
            };
            that._get_value = [](HttpAllArgs &that, const string &key) {
                return getValue(that.getParser(), that.getArgs(), key);
            };
            that._cache_able = true;
        };
    }

    HttpAllArgs(const HttpAllArgs &that) {
        if (that._cache_able) {
            _get_args = that._get_args;
            _get_parser = that._get_parser;
            _get_value = that._get_value;
            _cache_able = true;
        } else {
            that._clone(*this);
        }
    }

    ~HttpAllArgs() = default;

    template<typename Key>
    variant operator[](const Key &key) const {
        return (variant)_get_value(*(HttpAllArgs*)this, key);
    }

    const Parser &getParser() const {
        return _get_parser();
    }

    Args &getArgs() {
        return *((Args *) _get_args());
    }

    const Args &getArgs() const {
        return *((Args *) _get_args());
    }

private:
    bool _cache_able = false;
    function<void *() > _get_args;
    function<const Parser &() > _get_parser;
    function<string(HttpAllArgs &that, const string &key)> _get_value;
    function<void(HttpAllArgs &that) > _clone;
};

#define API_ARGS_MAP SockInfo &sender, HttpSession::KeyValue &headerOut, const HttpAllArgs<ApiArgsType> &allArgs, Json::Value &val
#define API_ARGS_MAP_ASYNC API_ARGS_MAP, const HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_JSON SockInfo &sender, HttpSession::KeyValue &headerOut, const HttpAllArgs<Json::Value> &allArgs, Json::Value &val
#define API_ARGS_JSON_ASYNC API_ARGS_JSON, const HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_STRING SockInfo &sender, HttpSession::KeyValue &headerOut, const HttpAllArgs<string> &allArgs, Json::Value &val
#define API_ARGS_STRING_ASYNC API_ARGS_STRING, const HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_VALUE sender, headerOut, allArgs, val

//注册http请求参数是map<string, variant, StrCaseCompare>类型的http api
void api_regist(const string &api_path, const function<void(API_ARGS_MAP)> &func);
//注册http请求参数是map<string, variant, StrCaseCompare>类型,但是可以异步回复的的http api
void api_regist(const string &api_path, const function<void(API_ARGS_MAP_ASYNC)> &func);

//注册http请求参数是Json::Value类型的http api(可以支持多级嵌套的json参数对象)
void api_regist(const string &api_path, const function<void(API_ARGS_JSON)> &func);
//注册http请求参数是Json::Value类型，但是可以异步回复的的http api
void api_regist(const string &api_path, const function<void(API_ARGS_JSON_ASYNC)> &func);

//注册http请求参数是http原始请求信息的http api
void api_regist(const string &api_path, const function<void(API_ARGS_STRING)> &func);
//注册http请求参数是http原始请求信息的异步回复的http api
void api_regist(const string &api_path, const function<void(API_ARGS_STRING_ASYNC)> &func);

template<typename Args, typename First>
bool checkArgs(Args &args, const First &first) {
    return !args[first].empty();
}

template<typename Args, typename First, typename ...KeyTypes>
bool checkArgs(Args &args, const First &first, const KeyTypes &...keys) {
    return checkArgs(args, first) && checkArgs(args, keys...);
}

//检查http url中或body中或http header参数是否为空的宏
#define CHECK_ARGS(...)  \
    if(!checkArgs(allArgs,##__VA_ARGS__)){ \
        throw InvalidArgsException("缺少必要参数:" #__VA_ARGS__); \
    }

//检查http参数中是否附带secret密钥的宏，127.0.0.1的ip不检查密钥
#define CHECK_SECRET() \
    if(sender.get_peer_ip() != "127.0.0.1"){ \
        CHECK_ARGS("secret"); \
        if(api_secret != allArgs["secret"]){ \
            throw AuthException("secret错误"); \
        } \
    }

void installWebApi();
void unInstallWebApi();
Value makeMediaSourceJson(MediaSource &media);
Value makeSimpleMediaSourceJson(MediaSource &media);
void getStatisticJson(const function<void(Value &val)> &cb);
void addStreamProxy(const string &vhost, const string &app, const string &stream, const string &url, int retry_count,
                    bool enable_hls, bool enable_mp4, int rtp_type, float timeout_sec,
                    const function<void(const SockException &ex, const string &key)> &cb);
#endif //ZLMEDIAKIT_WEBAPI_H
