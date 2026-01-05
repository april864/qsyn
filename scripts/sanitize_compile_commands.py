#!/usr/bin/env python3
"""
Sanitize compile_commands.json by removing compiler-specific flags
that clang-tidy doesn't recognize (e.g., -Wno-restrict from GCC).
This script is used by scripts/LINT to sanitize the compile_commands.json
file for clang-tidy.
"""

import json
import sys
import argparse


def sanitize_compile_commands(input_file: str, output_file: str) -> None:
    """Remove GCC-specific flags from compile commands."""
    try:
        with open(input_file, 'r') as f:
            data = json.load(f)
        
        for entry in data:
            if 'command' in entry:
                # Remove -Wno-restrict flag (GCC-specific, not supported by clang)
                entry['command'] = (
                    entry['command']
                    .replace(' -Wno-restrict ', ' ')
                    .replace(' -Wno-restrict', '')
                    .replace('-Wno-restrict ', '')
                )
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        print(f'Error processing compile_commands.json: {e}', file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Sanitize compile_commands.json for clang-tidy'
    )
    parser.add_argument(
        'input',
        help='Input compile_commands.json file path'
    )
    parser.add_argument(
        'output',
        help='Output compile_commands.json file path'
    )
    
    args = parser.parse_args()
    sanitize_compile_commands(args.input, args.output)


if __name__ == '__main__':
    main()

