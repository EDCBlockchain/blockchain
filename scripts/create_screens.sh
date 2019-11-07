#!/bin/bash

echo 'Creating screens...'

screens=(
"screen1"
"screen2"
)

for i in "${screens[@]}"
do
 echo "-$i"
 screen -Sdm "$i";
done

echo "done."