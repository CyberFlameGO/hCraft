
# MUST BE CHANGED TO SUIT THE COMPUTER THE PROGRAM IS BEING COMPILED ON
soci_include_path = '/usr/local/include/soci'
soci_lib_path     = '/usr/local/lib64'

mysql_include_path = '/usr/local/mysql/include'
mysql_lib_path     = '/usr/local/mysql/lib'

#------------------------------------------------------------------------------

env = Environment(CPPPATH  = ":".join (("#/include", soci_include_path, mysql_include_path)),
									CCFLAGS  = '-g -Wall -O0',
									CXXFLAGS = '-std=c++11 -D_GLIBCXX_USE_NANOSLEEP',
									DEBUG    = True)

env['soci_lib_path'] = soci_lib_path
env['mysql_lib_path'] = mysql_lib_path

SConscript(['src/SConscript'], exports = 'env', variant_dir = 'build')
 
