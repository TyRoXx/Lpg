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
    for i, code_block in enumerate(code):
        print_block = '// %s : %i\n%s\n' % (file_name, i, code_block)
        destination.write(print_block)


# Main Program
root_dir = '.'
destination = open('documentation.lpg', 'w')
if len(sys.argv) == 2:
    root_dir = sys.argv[1]

for root, dirs, files in os.walk(root_dir):
    for fi in files:
        if fi.endswith('md'):
            code = process_file(os.path.join(root, fi))
            write_to_file(fi, code)
destination.close()
