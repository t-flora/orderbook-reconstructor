compare_csv_fields() {
    local line1=$(sed -n '2p' data/test/test_debug_all_events.csv)
    local line2=$(sed -n '2p' data/mbp.csv)
    
    IFS=',' read -ra fields1 <<< "$line1"
    IFS=',' read -ra fields2 <<< "$line2"
    
    for ((i=0; i<${#fields1[@]}; i++)); do
        if [[ "${fields1[i]}" != "${fields2[i]}" ]]; then
            echo "Field $((i+1)) differs:"
            echo "  Generated: '${fields1[i]}'"
            echo "  Expected:  '${fields2[i]}'"
            break
        fi
    done
}
compare_csv_fields