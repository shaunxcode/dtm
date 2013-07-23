#include "trim.hpp"
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>


namespace datomicRest {
  using std::vector; 
  using std::string;
  using std::cout;
  using std::endl;
  using std::setw;
  using std::fill;
  using std::left; 
  
  char* envFormat = getenv("DTM_FORMAT"); 
  char* envHost = getenv("DTM_HOST");
  char* envAlias = getenv("DTM_ALIAS");
  char* envDb = getenv("DTM_DB");
  
  enum ReqTypes { GET, PUT, POST, DELETE };
  enum FormatTypes { EDN, TBL, JSON, CSV, TSV };
  FormatTypes format = TBL;
  
  string data;
  string host;
  string alias;
  string db;
    
  int queryLimit;
  int queryOffset;
  edn::EdnNode queryHeader;
  
  bool validate = false;
  bool verbose = false;
  bool watchingEvents = false;
  void (*watchingEventsHandler)(bool, edn::EdnNode);
  
  CURL *curl;

  FormatTypes getFormatType(string str) {
    if (str == "EDN" || str == "edn")
      return EDN;
    if (str == "JSON" || str == "json")
      return JSON;
    if (str == "CSV" || str == "csv")
      return CSV;
    if (str == "TSV" || str == "tsv")
      return TSV;
    if (str == "TBL" || str == "tbl")
      return TBL;
    return EDN;
  }

  string getFormatName(FormatTypes type) { 
    string format;
    switch (type) {
      case EDN: format = "EDN"; break;
      case JSON: format = "JSON"; break;
      case CSV: format = "JSON"; break;
      case TSV: format = "TSV"; break;
      case TBL: format = "TBL"; break;
    }
    return format;
  }

  //write to some sort of log file?
  std::string exec(char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
      if(fgets(buffer, 128, pipe) != NULL)
        result += buffer;
    }
    pclose(pipe);
    return result;
  }
  
  void init() {
    if (envHost != NULL) host = envHost;
    if (envAlias != NULL) alias = envAlias;
    if (envDb != NULL) db = envDb;
    if (envFormat != NULL) format = getFormatType(envFormat);
    if (*host.rbegin() != '/') host += '/';
      
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
  }
  
  void cleanup(string msg = "") {
    curl_easy_cleanup(curl); 
    curl_global_cleanup(); 
  }
  
  size_t writeCallback(char* buf, size_t size, size_t nmemb, void* up) {
    for(size_t c = 0; c < size*nmemb; c++) {
      data.push_back(buf[c]);
    }

    if (watchingEvents && data.length() > 0 && 
        !(data.length() == 1 && data[0] == ':')) {
      try {
        watchingEventsHandler(true, edn::read(data)); 
      } catch (const char* e) {
        watchingEventsHandler(
          false, 
          edn::read("\"Invalid edn [" + string(e) + "] [" + data + "]\"")); 
      }
    }

    return size*nmemb;
  }

  edn::EdnNode request(ReqTypes reqType, 
                       string url, 
                       string postData = "", 
                       string acceptHeader = "Accept: application/edn") {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, acceptHeader.c_str());

    data = "";
    
    if (reqType == POST) {
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      if(postData.length()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
      }
    }
    else {
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    }

    string fullHost = host + url;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, fullHost.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
    if (verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
    curl_easy_perform(curl); 

    long responseCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    if (verbose) { 
      cout << "URL: " << fullHost << endl;
      cout << "RESPONSE CODE: " << responseCode << endl;
      cout << "DATA: " << data << endl;
    }

    if(responseCode == 500) { 
      //parse out <title>{string we care about}</title>
      size_t start = data.find("<title>") + 7; 
      size_t stop = data.find("</title>") - 8; 
      size_t len = stop - start; 
      return edn::read("\"Problem: " + data.substr(start, len) + "\""); 
    } else { 
      return edn::read(data);
    }
  }

  edn::EdnNode getStorages() {
    return request(GET, "data/");
  }

  
  edn::EdnNode createDatabase(string name) {
    char *dbname = curl_easy_escape(curl, name.c_str(), 0);
    string data = "db-name=" + string(dbname); 
    curl_free(dbname);
    return request(POST, "data/" + alias + "/", data);
  }

  edn::EdnNode getDatabases(string alias) {
    return request(GET, "data/" + alias + "/");
  }

  edn::EdnNode getEntity(string entity) {
    char *entityId = curl_easy_escape(curl, entity.c_str(), 0);
    string url = "data/" + alias + "/" + db + "/-/entity?e=" + string(entityId);
    curl_free(entityId);
    return request(GET, url);
  }
  
  edn::EdnNode transact(string transactString) {
    if (verbose) cout << "TRANSACT: " << transactString << endl;
    char *txdata = curl_easy_escape(curl, transactString.c_str(), 0);
    string data = "tx-data=" + string(txdata);
    if (verbose) cout << "DATA: " << data << endl;
    curl_free(txdata);
    return request(POST, "data/" + alias + "/" + db + "/", data); 
  }

  edn::EdnNode retractEntity(string entity) {
    return transact(string("[[:db.fn/retractEntity ") + entity + string("]]"));
  }
  
  edn::EdnNode retractEntities(edn::EdnNode entitiesList) { 
    if (entitiesList.type != edn::EdnVector) throw "must be a vector of ints";
    
    string retractions;
    std::list<edn::EdnNode>::iterator it;
    for (it = entitiesList.values.begin(); it != entitiesList.values.end(); ++it) {
      if (it->type != edn::EdnInt) throw "Must be a vector of ints";
      retractions += "[:db.fn/retractEntity " + it->value + "]";
    } 
    return transact("[" + retractions + "]");
  }
  
  edn::EdnNode query(string queryString) {
    if (verbose) cout << "QUERY: " << queryString << endl;
    if (verbose) cout << "CONN:  " << host << " | " << alias << " | " << db << endl;
    try {
      string qheader = "[";
      edn::EdnNode qedn = edn::read(queryString);
      std::list<edn::EdnNode>::iterator qit;
      //iterate over list expecting first node to be :find until next :keyword
      for (qit = qedn.values.begin(); qit != qedn.values.end(); ++qit) {
        if (qit->type == edn::EdnKeyword && qit->value == ":find") continue;
        if (qit->type == edn::EdnKeyword) break;
        qheader += " \"" + edn::pprint(*qit) + "\"";
      }
      qheader.erase(
        std::remove(qheader.begin(), qheader.end(), '\n'), 
        qheader.end());
      
      queryHeader = edn::read(qheader + "]");

    } catch (const char* e) {
      throw "Could not parse query: " + string(e);
    }
    
    char *query = curl_easy_escape(curl, queryString.c_str(), 0);
    char *args = curl_easy_escape(curl, string("[{:db/alias \"" + alias + "/" + db + "\"}]").c_str(), 0);
    string url = "api/query?q=" + string(query) + "&args=" + string(args);
    curl_free(query);
    curl_free(args);
    return request(GET, url);
  }

  edn::EdnNode getEntitiesWith(edn::EdnNode attrs) {
    string findClause = "[:find ?db-id ";
    string whereClause = ":where ";
    std::list<edn::EdnNode>::iterator it;
    string attrName;
    string findSym;
    for (it = attrs.values.begin(); it != attrs.values.end(); ++it) {
      attrName = it->value;
      findSym = attrName;
      findSym[0] = '?';
      std::replace(findSym.begin(), findSym.end(), '/', '-');
      findClause += findSym + " ";
      whereClause += " [?db-id " + attrName + " " + findSym + "]";
    }
    return query(findClause + whereClause + "]");
  }
  
  edn::EdnNode getNamespaces() {
    edn::EdnNode result = query(
      "[:find ?ns "
      " :where [_ :db/ident ?name] "
             " [(namespace ?name) ?n] "
             " [(keyword ?n) ?ns]]");
    
    string vals; 
    std::list<edn::EdnNode>::iterator it;
    for (it = result.values.begin(); it != result.values.end(); ++it) {
      if (it->values.front().value.substr(0, 4) == ":db." ||
          it->values.front().value == ":fressian" ||
          it->values.front().value == ":db") {
        continue;
      }
      
      if (it->values.front().type == edn::EdnNil) {
        vals += "[top-level]";
      } else {
        vals += "[" + it->values.front().value + "]";
      }
    }
    
    return edn::read("[" + vals + "]");
  }

  string getJustNamespace(string ns) { 
    if (ns[0] == ':') ns = ns.substr(1); 
    return ns;
  }
    
  edn::EdnNode getIdents(string ns) { 
    return query(
      "[:find ?ident "
      " :where [_ :db/ident ?ident] "
             " [(namespace ?ident) ?ns] "
             " [(= ?ns \"" + getJustNamespace(ns) + "\")]]");
  }
  
  edn::EdnNode getFns() {
    return query("[:find ?fn :where [?f :db/fn _] [?f :db/ident ?fn]]");
  }
  
  edn::EdnNode getAttributes(string ns) {
    return query("[:find ?ident ?valueType ?cardinality "
                 " :where [?e :db/ident ?ident] "
                        " [(namespace ?ident) ?ns] "
                        " [(= ?ns \"" + getJustNamespace(ns) + "\")] "
                        " [?e :db/valueType ?v] " 
                        " [?v :db/ident ?valueType] " 
                        " [?e :db/cardinality ?c] "
                        " [?c :db/ident ?cardinality]]");  
  }

  edn::EdnNode getEntities(string ns) {
    edn::EdnNode attrs = getAttributes(ns);
    string findClause = "[:find ?db-id ";
    string whereClause = ":where ";
    std::list<edn::EdnNode>::iterator it;
    string attrName;
    string findSym; 
    for (it = attrs.values.begin(); it != attrs.values.end(); ++it) {
      attrName = it->values.front().value;
      findSym = attrName;
      findSym[0] = '?';
      std::replace(findSym.begin(), findSym.end(), '/', '-');
      findClause += findSym + " "; 
      whereClause += " [?db-id " + attrName + " " + findSym + "]";
    }
    return query(findClause + whereClause + "]");
  }
  
  bool validEdn(string val, string ednType, edn::EdnNode &node) {
    if (ednType == ":db.type/string") {
      node.value = val; 
      node.type = edn::EdnString;
      return true;
    }
 
    if (ednType == ":db.type/boolean" && edn::validBool(val)) {
      node.value = val; 
      node.type = edn::EdnBool;
      return true;
    } 

    return false;
  }
  
  edn::EdnNode atPath(string pathStr, edn::EdnNode result) { 
    edn::EdnNode path = edn::read(pathStr);
    std::list<edn::EdnNode>::iterator it;
    for (it = path.values.begin(); it != path.values.end(); ++it) {
      bool found = false; 
      if (result.type == edn::EdnMap) {
        std::list<edn::EdnNode>::iterator kit;
        for (kit = result.values.begin(); kit != result.values.end(); ++kit) {
          if (kit->value == it->value) {
            kit++;
            found = true;
            break;
          }
          ++kit;
        }
        if (found) {
          edn::EdnNode newResult = *kit;
          result = newResult;
        }
      } else if (it->type == edn::EdnInt) {
        std::list<edn::EdnNode>::iterator lit;
        int index = 0;
        int matchIndex = atoi(it->value.c_str());
        for (lit = result.values.begin(); lit != result.values.end(); ++lit) {
          if (index == matchIndex) {
            found = true;
            break;
          }
          index++;
        }
        if (found) { 
          edn::EdnNode newResult = *lit;
          result = newResult;
        }
     }
      
     if (!found) 
      throw "Could not find item " + it->value + " in " + result.value;
    }
   
    return result;
  }
  
  bool atPathExists(string pathStr, edn::EdnNode result) {
    try {
      atPath(pathStr, result);
      return true;
    } catch(...) {
      return false;
    }
  }
  
  void printTable(edn::EdnNode node, bool withHeader = false) {
    std::list<edn::EdnNode>::iterator rit;
    std::list<edn::EdnNode>::iterator cit;
    vector<int> widths;
    int index;
    for (rit = node.values.begin(); rit != node.values.end(); ++rit) {
      index = 0;
      for(cit = rit->values.begin(); cit != rit->values.end(); ++cit) {
        if (cit->value.length() == 0)
          cit->value = edn::pprint(*cit); 
          cit->value.erase(
            std::remove(cit->value.begin(), cit->value.end(), '\n'),
            cit->value.end());
            
        if (index > int(widths.size() - 1))
          widths.push_back(cit->value.length());
        else if(int(cit->value.length()) > widths[index])
          widths[index] = cit->value.length();
        index++;
      }
    }
    
    int row = 0;
    for (rit = node.values.begin(); rit != node.values.end(); ++rit) {
      index = 0;
      if (!row && withHeader) {
        for (unsigned i = 0; i < widths.size(); ++i) {
          if (i == 0) cout << " ┌";
          else cout << "┬";
          for(int j = 0; j < widths[i] + 2; ++j) cout << "─";
        } 
        cout << "┐" << endl;
      }
        
      for(cit = rit->values.begin(); cit != rit->values.end(); ++cit) {
        cout << " │ " << setw(widths[index]) << left << cit->value;
        index++; 
      }
      
      cout << " │ " << endl;
      if (!row && withHeader) {
        for (unsigned i = 0; i < widths.size(); ++i) {
          if (i == 0) cout << " ├";
          else cout << "┼";
          for(int j = 0; j < widths[i] + 2; ++j) cout << "─";
        } 
        cout << "┤" << endl;
      }
      row++;
    }
    for (unsigned i = 0; i < widths.size(); ++i) {
      if (i == 0) cout << " └";
      else cout << "┴";
      for(int j = 0; j < widths[i] + 2; ++j) cout << "─";
    } 
    cout << "┘" << endl;
  }
    
  void printTable(edn::EdnNode node, edn::EdnNode header) {
    //get widths including headers potentially
    node.values.push_front(header);
    printTable(node, true);
  }
}