#!/bin/bash

array=("2.21.122800" "2.21.123139")

counter=27
for i in "${array[@]}"
do
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "begin_builder_transaction","params": [],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "add_operation_to_builder_transaction","params": ['$counter', [ 63,{ "fee": { "amount": 0, "asset_id": "1.3.0" }, "deposit_id": "'$i'", "enabled": false, "extensions": [] } ]],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  curl -XPOST -H "Content-type: application/json" -d '{"jsonrpc": "2.0","method": "sign_builder_transaction","params": ['$counter', true],"id": 1}' 'http://127.0.0.1:5902/rpc'
  
  ((counter++))
  sleep 1
  echo ""
  echo "deposit $i done."
done