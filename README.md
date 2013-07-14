dtm
===

command line datomic query/transact bin that plays nice with unix.

##overview
command line tool for reflecting upon, querying and populating datomic. 

##environment vars
You can set these to avoid constantly passing arguments indicating your host etc.

	DTM_HOST=http://your-datomic-rest-server/
	DTM_ALIAS=name-of-alias
	DTM_DB=name-of-db
	DTM_FORMAT=EDN
	
##arguments
	--host
	--alias
	--db
	--format [EDN JSON CSV TSV]
	--path
	--verbose
		will turn on extra logging to show queries and all curl data 
		
##commands

    aliases
    
    databases
    
    create-database [db-name]
    
    entity [entity-id]
    
    entities [namespace]

	excise [entity]
	
    fns
    
    fns-in [namespace]
    
    create-fn

	namespaces
	
	namespace [namespace]

	create-entity [namespace]
    
    idents [namespace]
    
	create-ident [ident]

	create-attribute  

	retract [entity-id]
	
	transact [tx-edn]
	
	query [query-edn]
		--rules
		--args
		
		
##requirements
curl.h
