import sys, os


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
        code_pieces.append(content[left:right])
        content = content[right:]
        left, right = find_code(content)
    return code_pieces

root_dir = '.'
if len(sys.argv) == 2:
    root_dir = sys.argv[1]

code = []
for root, dirs, files in os.walk(root_dir):
    for fi in files:
        if fi.endswith('md'):
            code += process_file(os.path.join(root, fi))

code_file = open('documentation.lpg', 'w')
for code_block in code:
    code_file.write(code_block + '\n// Next block\n')

