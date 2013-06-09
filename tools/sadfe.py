#!/usr/bin/env python2.7 -B

import cmd
import socket
import sys
import select
import threading
import tempfile
import subprocess
import struct


def make_srec(addr, code):
  header = 'S00600004844521B'
  terminate = 'S9030000FC'
  addr = '%.8x' % addr
  code = code.encode('hex').upper()
  length = '%.2x' % (len(code.decode('hex')) + 5)
  srec = length + addr + code

  bytesum = sum(ord(i) for i in srec.decode('hex'))
  cksum = "%.2X" % (~bytesum & 0xFF)

  return '\n'.join([header, 'S3' + srec + cksum, terminate])


class SimpleAmigaDebuggerConnection(object):
  def __init__(self, server):
    self.server = server

  def recv(self, size=None):
    recv = lambda: self.server.recv(8192)

    if not size:
      return recv()

    buf = ""

    while len(buf) < size:
      data = recv()
      if not len(data):
        break
      buf += data

    if len(buf) != size:
      raise ValueError(buf)

    return buf

  def send(self, data):
    self.server.send(data)

  def send_cmd(self, cmd, payload=None):
    self.send(struct.pack('>BB', 0xAF, cmd))

    if payload is not None:
      return self.send(payload)

  def close(self):
    self.server.close()

  def expect(self, data):
    recv_data = self.recv(len(data))
    assert recv_data == data

  def expect_ack(self, cmd):
    self.expect(struct.pack('>BB', 0x00, cmd))

  def expect_done(self, cmd, datalen=0):
    self.expect(struct.pack('>BB', 0x1F, cmd))

    if datalen:
      data = self.recv(datalen)
    else:
      data = None

    self.expect("SAD?")

    return data

  def nop(self):
    self.send_cmd(0)

  def write_byte(self, ptr, val):
    self.send_cmd(1, struct.pack('>IB', ptr, val))
    self.expect_ack(1)
    self.expect_done(1)

  def write_word(self, ptr, val):
    self.send_cmd(2, struct.pack('>IH', ptr, val))
    self.expect_ack(2)
    self.expect_done(2)

  def write_long(self, ptr, val):
    self.send_cmd(3, struct.pack('>II', ptr, val))
    self.expect_ack(3)
    self.expect_done(3)

  def read_byte(self, ptr):
    self.send_cmd(4, struct.pack('>I', ptr))
    self.expect_ack(4)
    data = self.expect_done(4, 1)
    return struct.unpack('>B', data)[0]

  def read_word(self, ptr):
    self.send_cmd(5, struct.pack('>I', ptr))
    self.expect_ack(5)
    data = self.expect_done(5, 2)
    return struct.unpack('>H', data)[0]

  def read_long(self, ptr):
    self.send_cmd(6, struct.pack('>I', ptr))
    self.expect_ack(6)
    data = self.expect_done(6, 4)
    return struct.unpack('>I', data)[0]

  def call_address(self, ptr):
    self.send_cmd(7, struct.pack('>I', ptr))
    self.expect_ack(7)
    self.expect_done(7)

  def return_to_system(self):
    self.send_cmd(8, struct.pack('>I', 0))
    self.expect_ack(8)

  def get_context_frame(self):
    self.send_cmd(9)
    self.expect_ack(9)
    data = self.expect_done(9, 4)
    return struct.unpack('>I', data)[0]

  def allocate_memory(self, size, memtype):
    self.send_cmd(10, struct.pack('>II', size, memtype))
    self.expect_ack(10)
    data = self.expect_done(10, 4)
    return struct.unpack('>I', data)[0]

  def free_memory(self, ptr):
    self.send_cmd(11, struct.pack('>I', ptr))
    self.expect_ack(11)
    self.expect_done(11)

  def turn_on_single(self):
    self.send_cmd(12)
    self.expect_ack(12)
    data = self.expect_done(12, 4)
    return struct.unpack('>I', data)[0]

  def turn_off_single(self, ptr):
    self.send_cmd(13, struct.pack('>I', ptr))
    self.expect_ack(13)
    self.expect_done(13)

  def write_array(self, ptr, data):
    self.send_cmd(14, struct.pack('>IIs', ptr, len(data), data))
    self.expect_ack(14)
    self.expect_done(14)

  def read_array(self, ptr, size):
    self.send_cmd(15, struct.pack('>II', ptr, size))
    self.expect_ack(15)
    return self.expect_done(15, size)

  def reset(self):
    self.send_cmd(16)
    self.expect_ack(16)


class SimpleAmigaDebuggerFrontEnd(cmd.Cmd):
  def __init__(self, sad):
    cmd.Cmd.__init__(self)
    self.prompt = '>>> '
    self.sad = sad

  def parse_args(self, args, *types):
    args = args.split(' ')

    if len(args) != len(types):
      print 'Wrong number of arguments!'
      return None

    values = []

    for v, t in zip(args, types):
      if t == 'ptr':
        v = int(v, 16)
        assert (v >= 0 or v < 2 ** 32)
      elif t == 'byte':
        v = int(v, 10)
        assert (v >= 0 or v < 2 ** 8)
      elif t == 'word':
        v = int(v, 10)
        assert (v >= 0 or v < 2 ** 16)
      elif t == 'long':
        v = int(v, 10)
        assert (v >= 0 or v < 2 ** 32)
      elif t == 'data':
        v = v.decode('hex')
      else:
        raise ValueError

      values.append(v)

    return values

  def do_quit(self, args):
    return True

  def do_wb(self, args):
    """ write byte """
    ptr, byte = self.parse_args(args, 'ptr', 'byte')
    self.sad.write_byte(ptr, byte)

  def do_ww(self, args):
    """ write word """
    ptr, word = self.parse_args(args, 'ptr', 'word')
    self.sad.write_word(ptr, word)

  def do_wl(self, args):
    """ write long """
    ptr, long_ = self.parse_args(args, 'ptr', 'long')
    self.sad.write_long(ptr, long_)

  def do_rb(self, args):
    """ read byte """
    ptr = self.parse_args(args, 'ptr')[0]
    byte = self.sad.read_byte(ptr)
    print "%.2x" % byte

  def do_rw(self, args):
    """ read word """
    ptr = self.parse_args(args, 'ptr')[0]
    word = self.sad.read_word(ptr)
    print "%.4x" % word

  def do_rl(self, args):
    """ read long """
    ptr = self.parse_args(args, 'ptr')[0]
    long_ = self.sad.read_long(ptr)
    print "%.8x" % long_

  def do_call(self, args):
    """ call address """
    ptr = self.parse_args(args, 'ptr')[0]
    self.sad.call_address(ptr)

  def do_return(self, _):
    """ return to system """
    self.sad.return_to_system()
    return True

  def do_frame(self, _):
    """ get context frame """
    ptr = self.sad.get_context_frame()
    data = self.sad.read_array(ptr, 21 * 4 + 2)
    data = struct.unpack('>IIIIIIIIIIIIIIIIIIIIHI', data)
    desc = ['VBR', 'AttnFlags', 'ExecBase', 'USP', 'D0', 'D1', 'D2', 'D3',
            'D4', 'D5', 'D6', 'D7', 'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6',
            'Prompt', 'SR', 'PC']

    for k, v in zip(desc, data):
      print k.ljust(max(len(d) for d in desc)), "%.8x" % v

  def do_alloc(self, args):
    """ allocate memory """
    size = self.parse_args(args, 'long')[0]
    ptr = self.sad.allocate_memory(size, 0)
    print "%.8x" % ptr

  def do_free(self, args):
    """ free memory """
    ptr = self.parse_args(args, 'ptr')[0]
    self.sad.free_memory(ptr)

  def do_trace(self, _):
    """ turn on single """
    ptr = self.sad.turn_on_single()
    print "%.8x" % ptr

  def do_notrace(self, args):
    """ turn off single """
    ptr = self.parse_args(args, 'ptr')[0]
    self.sad.turn_off_single(ptr)

  def do_wr(self, args):
    """ write array """
    ptr, data = self.parse_args(args, 'ptr', 'data')
    self.sad.write_array(ptr, data)

  def do_rr(self, args):
    """ read array """
    ptr, size = self.parse_args(args, 'ptr', 'long')
    data = self.sad.read_array(ptr, size)
    print data.encode('hex')

  def do_dis(self, args):
    """ disassemble """
    ptr, size = self.parse_args(args, 'ptr', 'long')
    data = self.sad.read_array(ptr, size)

    with tempfile.NamedTemporaryFile() as srec:
      srec.write(make_srec(ptr, data))
      srec.flush()
      output = subprocess.check_output(
        ['m68k-amigaos-objdump', '-b', 'srec', '-m', '68020', '-D', srec.name])
      print '\n'.join(output.split('\n')[7:-1])

  def do_reset(self, _):
    """ reset """
    self.sad.reset()


class SimpleAmigaDebuggerThread(threading.Thread):
  def __init__(self, server):
    threading.Thread.__init__(self)
    self.sad = SimpleAmigaDebuggerConnection(server)

  def run(self):
    sadfe = SimpleAmigaDebuggerFrontEnd(self.sad)
    sadfe.cmdloop()
    self.sad.close()


class SimpleAmigaDebuggerClient(object):
  def __init__(self):
    self.socket = None
    self.sad_mode = False

  def sad_keep_alive(self):
    ready = select.select([self.socket, self.client], [], [], 0.5)[0]

    if not ready:
      self.socket.send(struct.pack('>BB', 0xAF, 0))

    if self.socket in ready:
      data = self.socket.recv(8192)
      # print "=> client", data.encode('hex')
      self.client.send(data)

    if self.client in ready:
      data = self.client.recv(8192)

      if not len(data):
        self.client.close()
        self.sad_mode = False
        return

      # print "=> server", data.encode('hex')
      self.socket.send(data)

  def run(self):
    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.socket.connect(("localhost", 1234))

    try:
      recvbuf = ""

      while True:
        if self.sad_mode:
          self.sad_keep_alive()
          continue

        data = self.socket.recv(8192)
        recvbuf += data

        if recvbuf.endswith("SAD?"):
          print ""

          client, server = socket.socketpair()

          recvbuf = ""
          self.sad_mode = True
          self.client = client

          SimpleAmigaDebuggerThread(server).start()

        sys.stdout.write(data)
        sys.stdout.flush()

        recvbuf = recvbuf[-4:]
    except KeyboardInterrupt:
      pass

    self.socket.close()


if __name__ == '__main__':
  client = SimpleAmigaDebuggerClient()
  client.run()
