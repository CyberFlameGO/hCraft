
# MUST BE CHANGED TO SUIT THE COMPUTER THE PROGRAM IS BEING COMPILED ON
soci_include_path = '/usr/local/include/soci'
soci_lib_path     = '/usr/local/lib64'

mysql_include_path = '/usr/local/mysql/include'
mysql_lib_path     = '/usr/local/mysql/lib'

#------------------------------------------------------------------------------

# Configure command line options:
AddOption( "--release", dest="release", action="store_true" )

# Build compilation strings:
cc_flags = "-Wall "
if GetOption("release"):
	cc_flags += " -O3"
else:
	cc_flags += " -g -O0"

env = Environment(CPPPATH  = ":".join (("#/include", soci_include_path, mysql_include_path)),
									CCFLAGS  = cc_flags,
									CXXFLAGS = '-std=c++11 -D_GLIBCXX_USE_NANOSLEEP',
									DEBUG    = True)

env['soci_lib_path'] = soci_lib_path
env['mysql_lib_path'] = mysql_lib_path

SConscript(['src/SConscript'], exports = 'env', variant_dir = 'build')
 
