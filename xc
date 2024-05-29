#!/bin/bash
# Xcompiler for C
# bash patch to accept xxc as a shebang
if [ "$#" -le 1 ]; then
    /usr/local/bin/xcc/xcc
    exit
fi
args=()
split_arguments() {
    local arg="$1"
    local in_quote=false
    local part=""
    if [[ ! -z $2 ]]; then
        args+=("$arg")
    else
        for ((i = 0; i < ${#arg}; i++)); do
            char="${arg:$i:1}"
            if [[ $char == '"' ]]; then
                in_quote=!$in_quote 
            elif [[ $char == ' ' && $in_quote == false ]]; then
                args+=("$part")
                part=""
            else
                part+="$char"
            fi
        done
        args+=("$part")
    fi
}
arguments=()
while [ ! -f "${!#}" ]; do
    arg="${!#}"
    split_arguments "$arg" run
    arguments+=("\"${args[@]}\"")
    unset args
    set -- "${@:1:$(($#-1))}"
done
num_args=$#
filename="${!num_args}"
for arg in "${@:1:$((num_args - 1))}"; do
    split_arguments "$arg"
done
revrargs=()
for ((i=${#arguments[@]}-1; i>=0; i--)); do
    revrargs+="${arguments[i]} "
done
if [ ${#revrargs[@]} -gt 0 ]; then
    /usr/local/bin/xcc/xcc "${args[@]}" -rarg "$(echo ${revrargs[@]})" "${filename}"
else
    /usr/local/bin/xcc/xcc "${args[@]}" "${filename}"
fi