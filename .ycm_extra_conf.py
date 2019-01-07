import os

dir_path = os.path.dirname(os.path.realpath(__file__))

def Settings( **kwargs ):
  return {
    'flags': [ '-x', 'c++', '-Wall', '-Wextra', '-Werror',
        '-I/Users/hong/usr/boost-1.68.0/include', 
        '-I/Users/hong/usr/googletest/googletest/include', 
        '-I'+ dir_path + '/src',
        '-std=c++11' ],
  }


