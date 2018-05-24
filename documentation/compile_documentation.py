import sys
import os


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


def write_to_file(file_name, code):
    with open(file_name, 'w') as f:
        f.write(code)


def write_codeblocks_to_different_files(code_blocks):
    for i, code_block in enumerate(code_blocks):
        write_to_file('codeblock%i.lpg' % i, code_block)

# Main Program
root_dir = '.'
if len(sys.argv) == 2:
    root_dir = sys.argv[1]

if not os.path.exists(root_dir):
    raise Exception('Directory does not exist')

for root, dirs, files in os.walk(root_dir):
    markdown_files = [f for f in files if f.endswith('md')]
    for fi in markdown_files:
        code = process_file(os.path.join(root, fi))
        write_codeblocks_to_different_files(code)
