import sys

from tree_sitter import Point

p = Point(300, 0)
# access via subscript, not .row: avoids triggering Bug 3
by_index = sys.getrefcount(p[0])
print(f"sys.getrefcount(p[0]) = {by_index}  (bug: 3, fix: 2)")
