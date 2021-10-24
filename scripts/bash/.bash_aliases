NS3_ROOT_DIR="" # where ns3 directory is located
NS3_WORK_DIR=$NS3_ROOT_DIR"/output" # where output files (pcap, traceroutes, etc.) are stored

alias ns3="clear && cd "$NS3_ROOT_DIR

wafr()  { # run a project with waf in /scratch without args
    dir=$NS3_WORK_DIR/$1
    if [ -d "$dir" ]; then
        echo -e "\033[38;5;3m${dir} already exist, clearing directory...\033[00m"
        rm -r $dir
    fi
    mkdir $dir
    cd $NS3_ROOT_DIR && ./waf --run "$@" --cwd=$dir

    echo -e "\033[38;5;3many output file has been saved in ${dir}\033[00m"
    unset dir
} 
wafrv()  { wafr "$@" --vis ; } # with PyViz visualizer
wafrd()  { wafr $1 --command-template="gdb --args %s $2" ; } # debugger with 1 arg

# Bash completion for ./waf --run aliases in scratch dir
_local_scratch_ns3_waf_complete() {
    local cur prev opts
    COMPREPLY=()
    cur=${COMP_WORDS[COMP_CWORD]}
    prev=${COMP_WORDS[COMP_CWORD-1]}
    opts=$(ls $NS3_ROOT_DIR/scratch)
    COMPREPLY=( $(compgen -W "$opts" -- $cur) )
}
complete -o nospace -F _local_scratch_ns3_waf_complete wafr
complete -o nospace -F _local_scratch_ns3_waf_complete wafrv
complete -o nospace -F _local_scratch_ns3_waf_complete wafrd

# Read trace file ".pcap" with tcpdump
alias pcapread="tcpdump -nn -tt -r"