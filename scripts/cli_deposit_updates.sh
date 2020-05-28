#!/bin/bash

array=("2.21.116" "2.21.304")
counter=791
for i in "${array[@]}"
do
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "begin_builder_transaction","params": [],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "add_operation_to_builder_transaction","params": ['$counter', [ 82,{ "fee": { "amount": 0, "asset_id": "1.3.0" }, "deposit_id": "'$i'", "percent": 0, "reset": false, "extensions": {} } ]],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "sign_builder_transaction","params": ['$counter', true],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  ((counter++))
  sleep 1
  echo ""
  echo "deposit $i done."
done