#!/bin/bash

array=( "2.21.19303" "2.21.11932")
counter=34
for i in "${array[@]}"
do
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "begin_builder_transaction","params": [],"id": 1}' 'http://127.0.0.1:5901/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "add_operation_to_builder_transaction","params": ['$counter', [ 82,{ "fee": { "amount": 0, "asset_id": "1.3.0" }, "deposit_id": "'$i'", "percent": 1000, "reset": false, "extensions": {} } ]],"id": 1}' 'http://127.0.0.1:5901/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "sign_builder_transaction","params": ['$counter', true],"id": 1}' 'http://127.0.0.1:5901/rpc'
  
  ((counter++))
  sleep 1
done
