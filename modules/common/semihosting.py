import sys, uos, uio

def init():
  global __fh__
  global __fmode__
  global __lastfh__
  __fh__ = {
  0:sys.stdin,
  1:sys.stdout,
  2:sys.stderr
  }
  __fmode__ = {
  0:'r',
  1:'rb',
  2:'r+',
  3:'r+b',
  4:'w',
  5:'wb',
  6:'w+',
  7:'w+b',
  8:'a',
  9:'ab',
  10:'a+',
  11:'a+b'
  }
  __lastfh__=2
  return

def open_fh(fh):
  global __fh__
  if not fh in __fh__:
    raise ValueError('file '+str(fh)+' not open')
  return __fh__[fh]

def close(fh):
  #print('close')
  f=open_fh(fh)
  f.close()
  if fh > 2:
    del __fh__[fh]
  return 0

def elapsed():
  #print('elapsed')
  return -1

def errno():
  #print('errno')
  return 0

def flen(fh):
  #print('flen '+str(fh))
  len=-1
  if fh < 3:
    return len
  try:
    f=open_fh(fh)
    pos = f.tell()
    f.seek(0, 2)
    len = f.tell()
    f.seek(pos, 0)
  except OSError:
    len = -1
  return len

def iserror(err):
  #print('iserror')
  return 0

def istty(fh):
  #print('istty')
  if fh < 3:
    return 1
  else:
    return 0

def open(fnam, flags):
  #print('open '+fnam+" "+__fmode__[flags])
  global __fh__
  global __fmode__
  global __lastfh__
  if (flags < 0 or flags > 11):
    raise ValueError('wrong open mode '+str(flags))
  if (fnam == ':tt'):
    fh = flags // 4
    return fh
  if (fnam == ':semihosting-features'):
    fnam = '/flash/shfb.txt'
    try:
      f=uio.open(fnam,'r')
      f.close()
    except OSError:
      f=uio.open(fnam,'w')
      f.write('SHFB\x03')
      f.close()
  __lastfh__=__lastfh__+1
  newf=uio.open(fnam, __fmode__[flags])
  __fh__[__lastfh__]=newf
  #print("handle "+str(__lastfh__))
  return __lastfh__

def read(fh, len):
  #print('read ' + str(fh) + " len: "+str(len))
  try:
    if fh == 0:
      sys.stdout.write('? ')
      buf=''
      for i in range(1,len):
        ch=sys.stdin.read(1)
        sys.stdout.write(ch)
        if ch=='\r':
          ch='\n'
        buf=buf+ch
        if (ch == '\n'):
          break
      return buf
    else:
      f=open_fh(fh)
      buf=f.read(len)
      return buf
  except OSError:
    return b''

def readc():
  #print('readc')
  ch = sys.stdin.read(1)
  sys.stdout.write(ch)
  return ord(ch)

def remove(fnam):
  #print('remove')
  try:
    uos.remove(fnam)
  except OSError:
    return -1
  return 0

def rename(fnam1, fnam2):
  #print('rename')
  try:
    uos.rename(fnam1, fnam2)
  except OSError:
    return -1
  return 0

def seek(fh, offset):
  #print('seek')
  try:
    f=open_fh(fh)
    f.seek(offset, 0)
  except OSError:
    return -1
  return 0

def system(cmd):
  #print("system ", cmd)
  # uncomment the following line at your own risk.
  #eval(cmd)
  return 0

def tickfreq():
  #print("tickfreq")
  return 100

def write(fh, buf):
  #print('write ' + str(fh))
  if (fh == 1) or (fh == 2):
    sys.stdout.write(buf)
  else:
    f=open_fh(fh)
    f.write(buf)
  return len(buf)

# not truncated
