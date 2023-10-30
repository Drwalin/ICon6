def Settings( **kwargs ):
  return {
    'flags': ['-x', 'c++', '-Wall', '-pedantic', '-Isrc', '-Ibitscpp/include',
    '-Iinclude', '-IGameNetworkingSockets/include', '-std=c++17', '-I/usr/include'],
  }
