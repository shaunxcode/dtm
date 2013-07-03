#//define DEBUG
#include "vendor/edn-cpp/edn.hpp"
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <stdlib.h>
#include <vector>

enum FormatTypes { EDN, JSON, CSV, TSV };

FormatTypes getFormatType(std::string str) {
  if (str == "EDN" || str == "edn")
    return EDN;
  if (str == "JSON" || str == "json")
    return JSON;
  if (str == "CSV" || str == "csv")
    return CSV;
  if (str == "TSV" || str == "tsv")
    return TSV;
  return EDN;
}

enum ReqTypes { GET, PUT, POST, DELETE };

std::string data;
FormatTypes format;
std::string host;
std::string alias;
std::string db;
bool verbose;
CURL *curl;

size_t writeCallback(char* buf, size_t size, size_t nmemb, void* up) {
  data = "";
  for(size_t c = 0; c < size*nmemb; c++) {
    data.push_back(buf[c]);
  }

  return size*nmemb;
}

edn::EdnNode request(ReqTypes reqType, std::string url, std::string postData = "") {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/edn");
  
  if (reqType == POST) {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    if(postData.length()) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    }
  }
  else {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
  }

  std::string fullHost = host + url;
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_URL, fullHost.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
  if (verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
  curl_easy_perform(curl); 

  long responseCode;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
  if (verbose) { 
    std::cout << "URL: " << fullHost << std::endl;
    std::cout << "RESPONSE CODE: " << responseCode << std::endl;
    std::cout << "DATA: " << data << std::endl;
  }

  return edn::read(data);
}

void printResult(edn::EdnNode result) {
  std::cout << edn::pprint(result) << std::endl;
}

edn::EdnNode getStorages() {
  return request(GET, "data/");
}

edn::EdnNode createDatabase(std::string name) {
  char *dbname = curl_easy_escape(curl, name.c_str(), 0);
  std::string data = "db-name=" + std::string(dbname); 
  curl_free(dbname);
  return request(POST, "data/" + alias + "/", data);
}

edn::EdnNode getDatabases(std::string alias) {
  return request(GET, "data/" + alias + "/");
}

edn::EdnNode getEntity(std::string entity) {
  char *entityId = curl_easy_escape(curl, entity.c_str(), 0);
  std::string url = "data/" + alias + "/" + db + "/-/entity?e=" + std::string(entityId);
  curl_free(entityId);
  return request(GET, url);
}

edn::EdnNode transact(std::string transactString) {
  char *txdata = curl_easy_escape(curl, transactString.c_str(), 0);
  std::string data = "tx-data=" + std::string(txdata);
  curl_free(txdata);
  return request(POST, "data/" + alias + "/" + db + "/", data); 
}

edn::EdnNode query(std::string queryString) {
  char *query = curl_easy_escape(curl, queryString.c_str(), 0);
  char *args = curl_easy_escape(curl, std::string("[{:db/alias \"" + alias + "/" + db + "\"}]").c_str(), 0);
  std::string url = "api/query?q=" + std::string(query) + "&args=" + std::string(args);
  curl_free(query);
  curl_free(args);
  return request(GET, url);
}

std::map<std::string, std::string> args;

int quit(std::string msg = "") {
  curl_easy_cleanup(curl); 
  curl_global_cleanup(); 
  if(msg.length()) {
    std::cout << msg << std::endl;
    return 1;
  }
  return 0;
}

int help() {
  return quit("commands: [query querystring] [entity id] [transact data] [aliases] [databases] [createDatabase name] [help]\n    args: [--alias | -a] [--db | -d] [--host | -h]  [--format | -f]"); 
}

int main(int argc, char *argv[]) {
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  verbose = false;
  char* envFormat = getenv("DTM_FORMAT"); 
  char* envHost = getenv("DTM_HOST");
  char* envAlias = getenv("DTM_ALIAS");
  char* envDb = getenv("DTM_DB");

    std::string arg;
  std::string command;
  for (int i = 1; i < argc; ++i) {
    arg = std::string(argv[i]);
    if (arg == "help" || arg == "--help" || arg == "-help") {
      return help();
    } else if (arg == "--verbose") { 
      verbose = true;
      continue;
    } else if (arg == "aliases" || arg == "databases") {
      command = arg;
      continue;
    } else if (arg == "query" || arg == "entity" || arg == "transact" || arg == "createDatabase") {
      command = arg;
    }
    if (i < argc - 1) { 
      args[arg] = std::string(argv[++i]);
    } else {
      command = "fail";
      std::cout << "missing argument for " << arg << std::endl;
    }
  }

  if (!command.length()) return help();

  if (args.count("--alias")) 
    alias = args.at("--alias");
  else if (args.count("-a")) 
    alias = args.at("-a");
  else if (envAlias != NULL) 
    alias = envAlias;
  else
    return quit("Error: no alias provided via -a --alias or set in env as DTM_ALIAS");

  if (args.count("--db"))
    db = args.at("--db");
  else if (args.count("-d"))
    db = args.at("-d");
  else if (envDb != NULL)
    db = envDb;
  else
    return quit("Error: no db provided via -d --db or set in env as DTM_DB");

  if (args.count("--format"))
    format = getFormatType(args.at("--format"));
  else if (args.count("-f")) 
    format = getFormatType(args.at("-f"));
  else if (envFormat != NULL)
    format = getFormatType(envFormat);
  else
    format = EDN;

  if (args.count("--host"))
    host = args.at("--host");
  else if (args.count("-h"))
    host = args.at("-h");
  else if (envHost != NULL) 
    host = envHost;
  else
    return quit("Error: no host provided via -h --host or set in env as DTM_HOST");

  if (command == "aliases") {
    printResult(getStorages());
  }
  if (command == "query") {
    edn::EdnNode result = query(args.at("query"));
    if (args.count("--path")) {
      try { 
        edn::EdnNode path = edn::read(args.at("--path"));
        std::list<edn::EdnNode>::iterator it;
        for (it = path.values.begin(); it != path.values.end(); ++it) {
          if (it->type == edn::EdnInt) {
            if (result.type == edn::EdnVector) { 
              std::vector<edn::EdnNode> values(result.values.begin(), result.values.end()); 
              result = values.at(atoi(it->value.c_str()));
            } else {
              return quit("attempted to access index " + it->value + " in a non-vector: " + result.value);
            }
          } else {
            return quit("--path should only be vector of ints.");
          }
        }
      } catch (const char* e) {
        return quit("Error parsing path vector: " + std::string(e));
      }
    } 
    printResult(result); 
  } 
  if (command == "databases") { 
    printResult(getDatabases(alias));
  }
  if (command == "transact") {
    printResult(transact(args.at("transact")));
  }
  if (command == "entity") {
    printResult(getEntity(args.at("entity"))); 
  }

  return quit();
}
