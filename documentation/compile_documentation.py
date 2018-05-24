import sys
import os
from subprocess import call


def find_code(text):
    START_TAG = '```lpg'
    END_TAG = '```'
    first_index = text.find(START_TAG)
    if first_index == -1:
        return None, None
    last_index = text.find(END_TAG, first_index + 1)
    return first_index + len(START_TAG), last_index


def process_file(path):
    content = open(path, 'r').read()
    left, right = find_code(content)
    code_pieces = []
    while left is not None:
        code_pieces.append(content[left:right].strip())
        content = content[right:]
        left, right = find_code(content)
    return code_pieces


def compile_code(codeblock):
    # Write to file to have a target to build
    with open('documentation.lpg', 'w') as f:
        f.write(codeblock)

    exit_code = call(['build/cli/lpg', 'documentation.lpg', '--compile-only'])
    if exit_code != 0:
        raise Exception('Could not compile code')


# Main Program
root_dir = '.'
if len(sys.argv) == 2:
    root_dir = sys.argv[1]

if not os.path.exists(root_dir):
    raise Exception('Directory does not exist')

for root, dirs, files in os.walk(root_dir):
    markdown_files = [f for f in files if f.endswith('md')]
    for fi in markdown_files:
        codeblocks = process_file(os.path.join(root, fi))
        for code in codeblocks:
            compile_code(code)
sys.exit(0)
