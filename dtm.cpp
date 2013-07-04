#//define DEBUG
#include "vendor/edn-cpp/edn.hpp"
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <stdio.h>
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
  if (verbose) std::cout << "TRANSACT: " << transactString << std::endl;
  char *txdata = curl_easy_escape(curl, transactString.c_str(), 0);
  std::string data = "tx-data=" + std::string(txdata);
  curl_free(txdata);
  return request(POST, "data/" + alias + "/" + db + "/", data); 
}

edn::EdnNode query(std::string queryString) {
  if (verbose) std::cout << "QUERY: " << queryString << std::endl;
  char *query = curl_easy_escape(curl, queryString.c_str(), 0);
  char *args = curl_easy_escape(curl, std::string("[{:db/alias \"" + alias + "/" + db + "\"}]").c_str(), 0);
  std::string url = "api/query?q=" + std::string(query) + "&args=" + std::string(args);
  curl_free(query);
  curl_free(args);
  return request(GET, url);
}

std::string getJustNamespace(std::string ns) { 
  if (ns[0] == ':') ns = ns.substr(1); 
  return ns;
}
 
edn::EdnNode getAttributes(std::string ns) {
  return query("[:find ?ident ?valueType ?cardinality "
               " :where [?e :db/ident ?ident] "
                      " [(namespace ?ident) ?ns] "
                      " [(= ?ns \"" + getJustNamespace(ns) + "\")] "
                      " [?e :db/valueType ?v] " 
                      " [?v :db/ident ?valueType] " 
                      " [?e :db/cardinality ?c] "
                      " [?c :db/ident ?cardinality]]");  
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
  return quit("commands: [query querystring]\n"
              "            expects querystring to be well formed edn e.g. [:find ?n :where [_ :db/ident ?n]]\n"
              "          [entity id]\n"
              "            fetch all attributes stored against an entity\n"
              "          [entities namespace]\n"
              "            fetch all entities for a given namespace\n"
              "          [idents namespace]\n"
              "            fetch idents in enum for namespace\n"
              "          [createIdent ident]\n"
              "            adds ident e.g. :my.ns/my-ident\n"
              "          [transact data]\n"
              "            expects data to be well formed edn e.g. [{:db/id #db/id [:db.part/user -1] :some/attr :some-val}]\n"
              "          [aliases]\n"
              "            list all available aliases on REST service\n"
              "          [databases]\n"
              "            list all available databases for active alias\n"
              "          [createDatabase name]\n"
              "            create a new database\n"
              "          [namespaces]\n"
              "            see all namespaces in active db\n"
              "          [namespace namespaceName]\n"
              "            see all attributes for a namespace\n"
              "          [createAttribute namespace]\n"
              "            create a new attribute for the given namespace\n"
              "          [createEntity namespaceName]\n"
              "            prompt for creating an entity for attributes in a namespace\n"
              "          [fns]\n"
              "            list all functions\n"
              "          [fnsInNamespace namespace]\n"
              "            list all functions for namespace\n" 
              "          [createFn]\n"  
              "            prompt for creating a new fn\n"
              "          [help]\n"
              "            this information\n"
              "    args: [--alias | -a]\n"
              "            the name of the storage/alias as defined when starting REST service\n"
              "          [--db | -d]\n"
              "            the name of the database\n"
              "          [--host | -h]\n"
              "            the host of the datomic REST service\n"
              "          [--format | -f]\n"
              "            can be EDN JSON CSV or TSV\n"
              "          [--path]\n"
              "            expects well formed edn vector of integers for walking into a result from any command returning vector e.g. [0 0]"); 
}

bool validEdn(std::string val, std::string ednType, edn::EdnNode &node) {
  if (ednType == ":db.type/string") {
    node.value = val; 
    node.type = edn::EdnString;
  }
 
  if (ednType == ":db.type/boolean" && edn::validBool(val)) {
    node.value = val; 
    node.type = edn::EdnBool;
  } 

  return true;
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
    } else if (arg == "aliases" || arg == "databases" || arg == "namespaces" || arg == "fns" || arg == "createFn") {
      command = arg;
      continue;
    } else if (arg == "query" || arg == "entity" || arg == "transact" || arg == "createDatabase" || arg == "namespace" || arg == "createEntity" || arg == "fnsInNamespace" || arg == "entities" || arg == "idents" || arg == "createIdent") {
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

  edn::EdnNode result;

  if (command == "aliases")
    result = getStorages();

  if (command == "namespace") {
    result = getAttributes(args.at("namespace")); 
    
  }

  if (command == "namespaces") { 
    result = query("[:find ?ns (count-distinct ?ns)"
                   " :where [_ :db/ident ?name] "
                          " [(namespace ?name) ?n] "
                          " [(keyword ?n) ?ns]]");
    std::list<edn::EdnNode>::iterator it;
    result.values.pop_front();
    for (it = result.values.begin(); it != result.values.end(); ++it) {
      it->values.pop_back();
    }
  }

  if (command == "fns") {
    result = query("[:find ?fn :where [?f :db/fn _] [?f :db/ident ?fn]]");
  }

  if (command == "idents") {
    result = query("[:find ?ident "
                   " :where [_ :db/ident ?ident] "
                          " [(namespace ?ident) ?ns] "
                          " [(= ?ns \"" + getJustNamespace(args.at("idents")) + "\")]]");
  } 

  if (command == "createIdent") { 
    std::string ident = args.at("createIdent");
    if (ident[0] != ':') ident = ":" + ident; 
    result = transact("[{:db/id #db/id [:db.part/db] :db/ident " + ident + "}]"); 
  }

  if (command == "entities") { 
    edn::EdnNode attrs = getAttributes(args.at("entities"));
    std::string findClause = "[:find ?e ";
    std::string whereClause = ":where ";
    std::list<edn::EdnNode>::iterator it;
    std::string attrName;
    int symCount = 0;
    char findSym[10]; 
    for (it = attrs.values.begin(); it != attrs.values.end(); ++it) {
      attrName = it->values.front().value;
      sprintf(findSym, "?%d", symCount); 
      findClause += std::string(findSym) + " "; 
      whereClause += " [?e " + attrName + " " + findSym + "]";
      symCount++;
    }
    result = query(findClause + whereClause + "]"); 
  }

  if (command == "createEntity") {
    result = getAttributes(args.at("createEntity")); 
    std::list<edn::EdnNode>::iterator it;
    
    edn::EdnNode val;
    std::string tx = "[{:db/id #db/id [:db.part/user -1]"; 
    std::string input;
    std::string attrName;
    std::string attrType;
    std::string attrCard;
     
    for (it = result.values.begin(); it != result.values.end(); ++it) { 
      attrName = it->values.front().value; 
      it->values.pop_front(); 
      attrType = it->values.front().value;
      it->values.pop_front();
      attrCard = it->values.front().value; 

      while (true) {  
        std::cout << attrName << " (" + attrType + "): ";
        std::getline (std::cin, input); 
        if (validEdn(input, attrType, val)) { 
          tx += " " + attrName + " " + edn::pprint(val);
          break;
        } else { 
          std::cout << "Error validating " << attrType << std::endl; 
        } 
      }
    }
    result = transact(tx);
  }

  if (command == "query")
    result = query(args.at("query"));

  if (command == "databases")  
    result = getDatabases(alias);

  if (command == "transact") 
    result = transact(args.at("transact"));

  if (command == "entity") 
    result = getEntity(args.at("entity")); 

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
  return quit();
}
