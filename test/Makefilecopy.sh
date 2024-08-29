#!/bin/bash

# Specify the directory where you want to start the search
start_directory="."

# Find all directories containing Makefile
directories=$(find $start_directory -type f -name "Makefile" -exec dirname {} \;)

# Specify the custom clean target with a message
clean_target='clean:\n\t@echo "Cleaning up $(TEST)"\n\t@find . -type f ! -name '\''*.c'\'' ! -name '\''*.h'\'' ! -name '\''Makefile'\'' -exec rm -f {} +\n\t@find . -type d ! -name '\''.'\'' ! -name '\''.*'\'' -exec rm -rf {} +\n\t@echo "Cleaning up $(TEST)... OK"'

# Iterate through each directory and append the custom clean target
for dir in $directories; do
    # Check if the clean target already exists in the Makefile
    if ! grep -q 'clean:' "$dir/Makefile"; then
        echo -e "\nAppending clean target to $dir/Makefile"
        echo -e "\n$clean_target" >> "$dir/Makefile"
    else
        echo "Clean target already exists in $dir/Makefile"
    fi
done
