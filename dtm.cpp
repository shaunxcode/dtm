#//define DEBUG
#include "vendor/edn-cpp/edn.hpp"
#include "lib/datomicRest.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>

using std::string;
using std::cout;
using std::endl;

namespace DR = datomicRest;

std::map<string, string> args;

int quit(string msg = "") {
  DR::cleanup();
  if(msg.length()) {
    cout << msg << endl;
    return 1;
  }
  return 0;
}

int help() {
  return quit("commands:\n"
    "  [query querystring]\n"
    "    expects querystring to be well formed edn\n"
    "    e.g. [:find ?n :where [_ :db/ident ?n]]\n"
    "  [entity id]\n"
    "    fetch all attributes stored against an entity\n"
    "  [entities namespace]\n"
    "    fetch all entities for a given namespace\n"
    "  [events]\n"
    "    listens to and displays events for db.\n"
    "  [with-event handler]\n"
    "    listens for event and then executes handler\n"
    "  [datoms args]\n"
    "    direct access to the datoms. args is well formed edn map.\n"
    "    {:start :end :offset :limit :as-of :since :history}\n"
    "  [idents namespace]\n"
    "    fetch idents in enum for namespace\n"
    "  [create-ident ident]\n"
    "    adds ident e.g. :my.ns/my-ident\n"
    "  [transact data]\n"
    "    expects data to be well formed edn\n"
    "    e.g. [{:db/id #db/id [:db.part/user -1] :some/attr :some-val}]\n"
    "  [aliases]\n"
    "    list all available aliases on REST service\n"
    "  [databases]\n"
    "    list all available databases for active alias\n"
    "  [create-database name]\n"
    "    create a new database\n"
    "  [namespaces]\n"
    "    see all namespaces in active db\n"
    "  [attributes namespace]\n"
    "    see all attributes for a namespace\n"
    "  [create-ttribute namespace]\n"
    "    create a new attribute for the given namespace\n"
    "  [create-entity namespace ]\n"
    "    prompt for creating an entity for attributes in a namespace\n"
    "  [fns]\n"
    "    list all functions\n"
    "  [fns-in namespace]\n"
    "    list all functions for namespace\n" 
    "  [create-fn]\n"  
    "    prompt for creating a new fn\n"
    "  [help]\n"
    "    this information\n"
    "args: \n"
    "  [--alias | -a]\n"
    "    the name of the storage/alias as defined when starting REST service\n"
    "  [--db | -d]\n"
    "    the name of the database\n"
    "  [--host | -h]\n"
    "    the host of the datomic REST service\n"
    "  [--format | -f]\n"
    "    can be EDN JSON CSV or TSV\n"
    "  [--path]\n"
    "    expects well formed edn vector of integers for walking into a result from any command returning vector e.g. [0 0]\n"
    "  [--offset]\n"
    "    integer offset for dealing with large query results\n"
    "    (.e.g which page of results where page is based on limit)\n"
    "  [--limit]\n"
    "    integer limit for dealing with large query results (number of records to see at a time)"); 
}

void eventHandler(bool success, edn::EdnNode eventResult) {
  cout << "Got result " << edn::pprint(eventResult) << endl;  
}

void printResult(edn::EdnNode result) {
  cout << edn::pprint(result) << endl;
}

int main(int argc, char *argv[]) {
  DR::init();  
  edn::EdnNode result;
  string arg;
  string command;
  
  for (int i = 1; i < argc; ++i) {
    arg = string(argv[i]);
    if (arg == "help" || arg == "--help" || arg == "-help") {
      return help();
    } else if (arg == "--verbose") { 
      DR::verbose = true;
      continue;
    } else if (arg == "aliases"    || arg == "databases" || 
               arg == "namespaces" || arg == "fns"       ||
               arg == "create-fn"  || arg == "events") {
      command = arg;
      continue;
    } else if (arg == "query"      || arg == "entity"          || 
               arg == "transact"   || arg == "create-database" || 
               arg == "attributes" || arg == "create-entity  " || 
               arg == "fns-in"     || arg == "entities"        || 
               arg == "idents"     || arg == "create-ident"    || 
               arg == "offset"     || arg == "limit") {
      command = arg;
    }

    if (i < argc - 1)
      args[arg] = string(argv[++i]);
    else
      return quit("missing argument for " + arg);
  }

  if (!command.length()) return help();

  if (args.count("--offset"))
    if(edn::validInt(args.at("--offset"), false))
      DR::queryOffset = atoi(args.at("--offset").c_str());
    else 
      return quit("Invalid offset provided. unsigned int expected e.g. 5");  
  else 
    DR::queryOffset = 0;

  if (args.count("--limit"))
    if (edn::validInt(args.at("--limit"), false))
      DR::queryLimit = atoi(args.at("--limit").c_str());
    else
      return quit("Invalid offset provides. unsigned int expected e.g. 5"); 
  else 
    DR::queryLimit = -1;

  if (args.count("--alias")) 
    DR::alias = args.at("--alias");
  else if (args.count("-a")) 
    DR::alias = args.at("-a");
  else if (command != "aliases" && DR::alias.empty())
    return quit("Error: no alias provided via -a --alias or set in env as DTM_ALIAS");

  if (args.count("--db"))
    DR::db = args.at("--db");
  else if (args.count("-d"))
    DR::db = args.at("-d");
  else if (command != "aliases" && command != "databases" && DR::db.empty())
    return quit("Error: no db provided via -d --db or set in env as DTM_DB");

  if (args.count("--format"))
    DR::format = DR::getFormatType(args.at("--format"));
  else if (args.count("-f")) 
    DR::format = DR::getFormatType(args.at("-f"));

  if (args.count("--host"))
    DR::host = args.at("--host");
  else if (args.count("-h"))
    DR::host = args.at("-h");
  else if (DR::host.empty())
    return quit("Error: no host provided via -h --host or set in env as DTM_HOST");

  if (command == "aliases")
    result = DR::getStorages();

  if (command == "attributes")
    result = DR::getAttributes(args.at("attributes"));  

  if (command == "events") {
    DR::watchingEvents = true;
    DR::watchingEventsHandler = &eventHandler;
    
    string url = "events/" + DR::alias + "/" + DR::db;
    DR::request(DR::GET, url, "", "Accept: text/event-stream");
    return quit("done");
  }

  if (command == "namespaces")
    result = DR::getNamespaces();

  if (command == "fns")
    result = DR::getFns();

  if (command == "idents")
    result = DR::getIdents(args.at("idents")); 

  if (command == "create-ident") { 
    string ident = args.at("create-ident");
    if (ident[0] != ':') ident = ":" + ident; 
    result = DR::transact(
      "[{:db/id #db/id [:db.part/db] :db/ident " + ident + "}]"); 
  }

  if (command == "entities") { 
    result = DR::getEntities(args.at("entities"));
  }

  if (command == "create-entity") {
    result = DR::getAttributes(args.at("create-entity")); 
    std::list<edn::EdnNode>::iterator it;
    
    edn::EdnNode val;
    string tx = "[{:db/id #db/id [:db.part/user -1]"; 
    string input;
    string attrName;
    string attrType;
    string attrCard;
     
    for (it = result.values.begin(); it != result.values.end(); ++it) { 
      attrName = it->values.front().value; 
      it->values.pop_front(); 
      attrType = it->values.front().value;
      it->values.pop_front();
      attrCard = it->values.front().value; 

      while (true) {  
        cout << attrName << " (" + attrType + "): ";
        std::getline (std::cin, input); 
        if (DR::validEdn(input, attrType, val)) { 
          tx += " " + attrName + " " + edn::pprint(val);
          break;
        } else { 
          cout << "Error validating " << attrType << endl; 
        } 
      }
    }
    result = DR::transact(tx + "}]");
  }

  if (command == "query")
    result = DR::query(args.at("query"));

  if (command == "databases")  
    result = DR::getDatabases(DR::alias);

  if (command == "transact") 
    result = DR::transact(args.at("transact"));

  if (command == "entity") 
    result = DR::getEntity(args.at("entity")); 

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
      return quit("Error parsing path vector: " + string(e));
    }
  } 

  printResult(result); 
  return quit();
}
