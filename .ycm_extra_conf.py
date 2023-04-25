def Settings( **kwargs ):
  return {
    'flags': ['-x', 'c++', '-Wall', '-pedantic', '-Isrc',
    '-Iinclude', '-Ienet/include', '-std=c++17', '-I/usr/include'],
  }
