
env = Environment(CPPPATH  = '#/include:#/include/commands',
									CCFLAGS  = '-g -O0',
									CXXFLAGS = '-std=c++11 -D_GLIBCXX_USE_NANOSLEEP',
									DEBUG    = True)

SConscript(['src/SConscript'], exports = 'env', variant_dir = 'build')

