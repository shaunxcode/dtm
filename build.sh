clear
rm ./bin/dtm
rm ./bin/dtm-repl
gccp dtm.cpp -o ./bin/dtm -Lvendor/curl/include/curl -lcurl
gccp repl.cpp -o ./bin/dtm-repl -Lvendor/curl/include/curl -lcurl -L/opt/local/lib -lreadline 
