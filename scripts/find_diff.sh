find_data_diff() {
    local actual_file="${1:-data/test/test_output_full_mbp.csv}"
    local expected_file="${2:-data/mbp.csv}"
    
    echo "Comparing: $actual_file vs $expected_file"
    echo ""
    
    local line_num=2
    while true; do
        local line1=$(sed -n "${line_num}p" "$actual_file")
        local line2=$(sed -n "${line_num}p" "$expected_file")
        
        if [[ -z "$line1" && -z "$line2" ]]; then
            echo "Files are identical!"
            break
        elif [[ -z "$line1" || -z "$line2" ]]; then
            echo "Files have different lengths"
            echo "Actual file lines: $(wc -l < "$actual_file")"
            echo "Expected file lines: $(wc -l < "$expected_file")"
            break
        fi
        
        # Remove line endings for comparison
        line1=$(echo "$line1" | tr -d '\r\n')
        line2=$(echo "$line2" | tr -d '\r\n')
        
        if [[ "$line1" != "$line2" ]]; then
            echo "=== FIRST DATA DIFFERENCE AT LINE $line_num ==="
            
            # Find which field differs
            IFS=',' read -ra fields1 <<< "$line1"
            IFS=',' read -ra fields2 <<< "$line2"
            
            for ((i=0; i<${#fields1[@]}; i++)); do
                if [[ "${fields1[i]}" != "${fields2[i]}" ]]; then
                    local col_name=$(head -1 "$actual_file" | cut -d',' -f$((i+1)))
                    echo "Column $((i+1)) ($col_name) differs:"
                    echo "  Generated: '${fields1[i]}'"
                    echo "  Expected:  '${fields2[i]}'"
                    echo ""
                    break
                fi
            done
            
            echo "Full lines:"
            echo "Generated: $line1"
            echo "Expected:  $line2"
            break
        fi
        
        ((line_num++))
    done
}
find_data_diff "$@"