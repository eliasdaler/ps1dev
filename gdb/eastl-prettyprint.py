import re

class EASTLVectorPrinter:
    "Print an eastl vector"

    def __init__(self, val):
        self.val = val
        self.length = self.val['mpEnd'] - self.val['mpBegin']
        self.mem = self.val['mpBegin']

    def next_element(self):
        for i in range(self.length):
            yield str(i), (self.mem + i).dereference()

    def display_hint(self):
        return 'array'

    def children (self):
        return self.next_element()

    def to_string (self):
        return '%s of length %d' % (self.val.type, self.length)

def eastl_vector_lookup_function(val):
    lookup_tag = val.type.tag
    if lookup_tag is None:
        return None
    regex = re.compile("^eastl::vector<.*>$")
    if regex.match(lookup_tag):
        return EASTLVectorPrinter(val)
    return None

gdb.pretty_printers.append(eastl_vector_lookup_function)
