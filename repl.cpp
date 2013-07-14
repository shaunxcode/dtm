#include "vendor/edn-cpp/edn.hpp"
#include "lib/datomicRest.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <readline/readline.h>
#include <readline/history.h>

namespace DR = datomicRest;

void printResult(edn::EdnNode &result) {
  if (DR::format == DR::TBL)
    if (result.type == edn::EdnMap) {
      edn::EdnNode kvpairs;
      kvpairs.type = edn::EdnVector;
      std::list<edn::EdnNode>::iterator it;
      for (it = result.values.begin(); it != result.values.end(); ++it) {
        edn::EdnNode pair;
        pair.type = edn::EdnVector;
        pair.values.push_back(*it);
        it++;
        pair.values.push_back(*it);
        kvpairs.values.push_back(pair);
      }
      DR::queryHeader = edn::read("[key value]");
      result = kvpairs;
    }
    if (result.type == edn::EdnVector) {
      if (result.values.front().type != edn::EdnVector) {
        edn::EdnNode kvpairs;
        kvpairs.type = edn::EdnVector;
        std::list<edn::EdnNode>::iterator it;
        for (it = result.values.begin(); it != result.values.end(); ++it) {
          edn::EdnNode pair;
          pair.type = edn::EdnVector;
          pair.values.push_back(*it);
          kvpairs.values.push_back(pair);
        }
        DR::queryHeader = edn::read("[value]");
        result = kvpairs;
      }
      if (DR::queryHeader.values.size())
        DR::printTable(result, DR::queryHeader);
      else
        DR::printTable(result);
    }
  else 
    std::cout << edn::pprint(result) << std::endl;
}

int main() {
  using std::string;
  
  DR::init();
  
  rl_bind_key('\t', rl_abort); //disable auto-complete
  
  char *buf;
  
  DR::verbose = false;
  DR::validate = false;
  
  string last = "";
  string ednString;
  string command;
  edn::EdnNode result;
  
  while ((buf = readline("\ndtm> ")) != NULL) {
    if (buf[0] == 0) continue;
    DR::queryHeader.values.clear();
    
    try {
      if (string(buf) != last) add_history(buf);
      
      edn::EdnNode node = edn::read("(" + string(buf) + ")");
      
      if (node.values.front().type == edn::EdnKeyword || node.values.front().type == edn::EdnInt) {
        result = DR::getEntity(node.values.front().value);
      } else if (node.values.front().type == edn::EdnSymbol) {
        command = node.values.front().value; 
        
        if (command == "test") { 
          DR::format = DR::TBL;
          DR::queryHeader = edn::read("[?fruit ?vegetable ?animal]");
          result = edn::read("[[apple carrot turtle][banana appletoneske cherrybomb]]");
        }
        
        if (command == "clear") {
          //need to make this work for windows differently
          std::cout << "\x1b[2J\x1b[1;1H" << std::flush;
          continue;
        }
        
        if (command == "verbose") {
          if (node.values.size() > 1)
            DR::verbose = (node.values.back().value == "on");
            
          result = edn::read(
            "{:verbose " + string(DR::verbose ? "on" : "off") + "}");
        }
        
        if (command == "validate") {
          if (node.values.size() > 1)
            DR::validate = (node.values.back().value == "on");
            
          result = edn::read(
            "{:validate " + string(DR::validate ? "on" : "off") + "}");
        }
        
        if (command == "format") {
          if (node.values.size() > 1)
            DR::format = DR::getFormatType(node.values.back().value);
            
          result = edn::read(
            "{:format " + DR::getFormatName(DR::format) + "}");
        }
        
        if (command == "attributes")
          if (node.values.size() == 2)
            result = DR::getAttributes(node.values.back().value);
          else
            result = edn::read("{:error \"attributes expects one argument - the namespace\"}");
        
        if (command == "storages") 
          result = DR::getStorages();
          
        if (command == "databases") 
          result = DR::getDatabases(DR::alias);
          
        if (command == "namespaces")
          result = DR::getNamespaces();
      
        if (command == "entity")
          result = DR::getEntity(edn::pprint(node.values.back()));
          
        if (command == "query")
          result = DR::query(edn::pprint(node.values.back()));
        
        if (command == "idents")
          if (node.values.size() == 2)
            result = DR::getIdents(node.values.back().value);
          else
            result = edn::read("{:error \"idents expects one argument - the namespace\"}");
                    
        if (command == "entities")
          if (node.values.size() == 2)
            result = DR::getEntities(node.values.back().value);
          else
            result = edn::read("{:error \"entities expects one argument\"}");
      }
      else {
        result = edn::read("{:does-not-understand " + edn::pprint(node) + "}");
        std::cout << "GOT BAD RESULT" << std::endl;
      }

      printResult(result); 
    } catch (const char* e) { 
      std::cout << "Error: " << e << std::endl;
    }
    
    last = string(buf); 
  }
  
  free(buf);
  return 0;
}