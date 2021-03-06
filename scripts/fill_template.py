#!/usr/bin/env python
import sys, re
from collections import deque

constants = deque()

with open(sys.argv[2]) as f:
  for line in f:
    for number in line.strip().split():
      constants.append(number)

output = []
with open(sys.argv[1]) as f:
  found_query = False
  for line in f:
    if '(query' in line:
      found_query = True
    for tok in re.split(r'(\s+)', line):
      if found_query:
        if re.match('^\d+$', tok):
          output.append(constants.popleft())
        elif re.match('^\(+\d+$', tok):
          count = tok.count('(')
          output.append('(' * count + constants.popleft())
        elif re.match('^\d+\)+$', tok):
          count = tok.count(')')
          output.append(constants.popleft() + ')' * count)
        else:
          output.append(tok)
      else:
        output.append(tok)

print ''.join(output)

# make sure we have used all constants
assert not constants
