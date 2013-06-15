def hexdump(data):
  hexch = ['%.2x' % ord(c) for c in data]

  ascii = []
  for c in data:
    if ord(c) >= 32 and ord(c) < 127:
      ascii.append(c)
    else:
      ascii.append('.')

  for i in range(0, len(hexch), 16):
    hexstr = ' '.join(hexch[i:i + 16])
    asciistr = ''.join(ascii[i:i + 16])
    print '  {2:04} | {0} |{1}|'.format(hexstr.ljust(47, ' '), asciistr, i)
