#pragma once

#ifndef DOM_REGPATH
	#define DOM_REGPATH "/var/local/dynamic-object-models/"
#endif // !DOM_REGPATH

#if !defined(NDEBUG) && defined(DOM_TRACE)
	#include <regex>
	const static std::regex __dom_re_fn(R"((.*?)(?:<.*?>)?(::~?\w+)\(.*)");
	const static std::regex __dom_re_return(R"((?:[\w\s]+\s+)?(.*))");
	#define DOM_ERR(format,...) fprintf(stderr,"[ ERROR::%s ] " format "\n", std::regex_replace(std::regex_replace(__PRETTY_FUNCTION__, __dom_re_fn, "$1$2"), __dom_re_return, "$1").c_str(), ##__VA_ARGS__)
	#define DOM_CALL_TRACE(format,...) fprintf(stdout,"[ TRACE::%s ] " format "\n", std::regex_replace(std::regex_replace(__PRETTY_FUNCTION__, __dom_re_fn, "$1$2"), __dom_re_return, "$1").c_str(), ##__VA_ARGS__)
#else
#define DOM_ERR(format,...)
#define DOM_CALL_TRACE(format,...)
#endif

#include "./core/interface.h"
#include "./core/server.h"
#include "./core/client.h"

