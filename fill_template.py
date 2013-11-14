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
  for line in f:
    for tok in re.split(r'(\s+)', line):
      if re.match('^\d+$', tok):
        output.append(constants.popleft())
      elif re.match('^\(\d+$', tok):
        output.append('(' + constants.popleft())
      elif re.match('^\d+\)$', tok):
        output.append(constants.popleft() + ')')
      else:
        output.append(tok)
print ''.join(output)

# make sure we have used all constants
assert not constants
