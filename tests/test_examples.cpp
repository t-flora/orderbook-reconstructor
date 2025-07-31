#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

// Helper function to check if a file exists
bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

// Helper function to create directory if it doesn't exist
bool create_directory(const std::string& path) {
    struct stat st = {0};
    if (stat(path.c_str(), &st) == -1) {
        #ifdef _WIN32
            return mkdir(path.c_str()) == 0;
        #else
            return mkdir(path.c_str(), 0755) == 0;
        #endif
    }
    return true; // Directory already exists
}

// Helper function to create directory path recursively
void ensure_directory_exists(const std::string& filepath) {
    size_t pos = filepath.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir_path = filepath.substr(0, pos);
        create_directory(dir_path);
    }
}

// Helper function to normalize line endings (remove \r)
std::string normalize_line_endings(const std::string& line) {
    std::string normalized = line;
    if (!normalized.empty() && normalized.back() == '\r') {
        normalized.pop_back();
    }
    return normalized;
}

// Helper function to read file content into vector of lines
std::vector<std::string> read_file_lines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << filename << std::endl;
        return lines;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(normalize_line_endings(line));
    }
    
    return lines;
}

// Helper function to show character differences
void show_character_diff(std::string_view line1, std::string_view line2, size_t line_num) {
    std::cerr << "Files differ at line " << line_num << ":" << std::endl;
    std::cerr << "File1 (" << line1.size() << " chars): " << line1 << std::endl;
    std::cerr << "File2 (" << line2.size() << " chars): " << line2 << std::endl;
    
    size_t min_len = std::min(line1.size(), line2.size());
    for (size_t j = 0; j < min_len; ++j) {
        if (line1[j] != line2[j]) {
            std::cerr << "First difference at position " << j << ": "
                      << "'" << line1[j] << "' (ASCII " << (int)line1[j] << ") vs "
                      << "'" << line2[j] << "' (ASCII " << (int)line2[j] << ")" << std::endl;
            break;
        }
    }
    if (line1.size() != line2.size()) {
        std::cerr << "Lines have different lengths: " << line1.size() << " vs " << line2.size() << std::endl;
    }
}

// Helper function to compare two files line by line using string views
bool compare_files(const std::string& file1, const std::string& file2) {
    auto lines1 = read_file_lines(file1);
    auto lines2 = read_file_lines(file2);
    
    if (lines1.size() != lines2.size()) {
        std::cerr << "Files have different number of lines: " 
                  << lines1.size() << " vs " << lines2.size() << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < lines1.size(); ++i) {
        std::string_view line1_view = lines1[i];
        std::string_view line2_view = lines2[i];
        
        if (line1_view != line2_view) {
            show_character_diff(line1_view, line2_view, i + 1);
            return false;
        }
    }
    
    return true;
}

void test_sample_mbo_to_mbp_conversion() {
    std::cout << "Running test: sample MBO to MBP conversion..." << std::endl;
    
    // Define file paths
    const std::string input_file = "data/sample_mbo.csv";
    const std::string expected_output = "data/sample_mbp.csv";
    const std::string test_output = "data/test/test_output_mbp.csv";
    const std::string executable = "build/reconstructor_TiagoFlora";
    
    // Check if required files exist
    if (!file_exists(input_file)) {
        std::cerr << "Error: Input file not found: " << input_file << std::endl;
        assert(false);
    }
    
    if (!file_exists(expected_output)) {
        std::cerr << "Error: Expected output file not found: " << expected_output << std::endl;
        assert(false);
    }
    
    if (!file_exists(executable)) {
        std::cerr << "Error: Executable not found: " << executable << std::endl;
        std::cerr << "Please run 'make' to build the executable first." << std::endl;
        assert(false);
    }
    
    // Remove test output file if it exists
    if (file_exists(test_output)) {
        std::remove(test_output.c_str());
    }
    
    // Ensure output directory exists
    ensure_directory_exists(test_output);
    
    // Build and run the command
    std::string command = executable + " " + input_file + " " + test_output;
    std::cout << "Executing: " << command << std::endl;
    
    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Error: Executable failed with return code: " << result << std::endl;
        assert(false);
    }
    
    // Check if output file was created
    if (!file_exists(test_output)) {
        std::cerr << "Error: Output file was not created: " << test_output << std::endl;
        assert(false);
    }
    
    // Compare output with expected result
    bool files_match = compare_files(test_output, expected_output);
    
    if (files_match) {
        std::cout << "P Test passed: Generated MBP output matches expected result" << std::endl;
        // Clean up test output file
        std::remove(test_output.c_str());
    } else {
        std::cerr << "F Test failed: Generated output does not match expected result." << std::endl;
        std::cerr << "Generated file: " << test_output << std::endl;
        std::cerr << "Expected file: " << expected_output << std::endl;
        assert(false);
    }
}

void test_full_mbo_to_mbp_conversion() {
    std::cout << "Running test: full MBO to MBP conversion..." << std::endl;
    
    // Define file paths
    const std::string input_file = "data/mbo.csv";
    const std::string expected_output = "data/mbp.csv";
    const std::string test_output = "data/test/test_output_full_mbp.csv";
    const std::string executable = "build/reconstructor_TiagoFlora";
    
    // Check if required files exist
    if (!file_exists(input_file)) {
        std::cerr << "Error: Input file not found: " << input_file << std::endl;
        assert(false);
    }
    
    if (!file_exists(expected_output)) {
        std::cerr << "Error: Expected output file not found: " << expected_output << std::endl;
        assert(false);
    }
    
    if (!file_exists(executable)) {
        std::cerr << "Error: Executable not found: " << executable << std::endl;
        std::cerr << "Please run 'make' to build the executable first." << std::endl;
        assert(false);
    }
    
    // Remove test output file if it exists
    if (file_exists(test_output)) {
        std::remove(test_output.c_str());
    }
    
    // Ensure output directory exists
    ensure_directory_exists(test_output);
    
    // Build and run the command
    std::string command = executable + " " + input_file + " " + test_output;
    std::cout << "Executing: " << command << std::endl;
    
    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Error: Executable failed with return code: " << result << std::endl;
        assert(false);
    }
    
    // Check if output file was created
    if (!file_exists(test_output)) {
        std::cerr << "Error: Output file was not created: " << test_output << std::endl;
        assert(false);
    }
    
    // Compare output with expected result
    bool files_match = compare_files(test_output, expected_output);
    
    if (files_match) {
        std::cout << "P Test passed: Generated full MBP output matches expected result" << std::endl;
        // Clean up test output file
        std::remove(test_output.c_str());
    } else {
        std::cerr << "F Test failed: Generated output does not match expected result." << std::endl;
        std::cerr << "Generated file: " << test_output << std::endl;
        std::cerr << "Expected file: " << expected_output << std::endl;
        assert(false);
    }
}

int main() {
    std::cout << "=== Running example tests ===" << std::endl;
    
    try {
        test_sample_mbo_to_mbp_conversion();
        test_full_mbo_to_mbp_conversion();
        std::cout << "All tests passed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
