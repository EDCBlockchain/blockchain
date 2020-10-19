#!/bin/bash

# accounts.txt:
# 	"1.2.153923",
# 	"1.2.153958",
#   ...

readarray -t array < accounts.txt
counter=791
for i in "${array[@]}"
do
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "begin_builder_transaction","params": [],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "add_operation_to_builder_transaction","params": ['$counter', [ 52,{ "fee": { "amount": 0, "asset_id": "1.3.0" }, "target": "'$i'", "verification_is_required": true, "extensions": [] } ]],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "sign_builder_transaction","params": ['$counter', true],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  ((counter++))
  sleep 1
  echo ""
  echo "user $i done."
done