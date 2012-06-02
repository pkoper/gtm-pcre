# successfully compiled with gcc version 4.6.3 on Debian 6 in GT.M user
# environment for PCRE 8.30-5 (libpcre3-dev package must be installed)
#
all:
	gcc -Wall -Werror -pedantic -fPIC -shared -o libpcre.so pcre.c $(shell pcre-config --libs) -I$(gtm_dist)

# Example installation procedure
#
# See http://tinco.pair.com/bhaskar/gtm/doc/books/pg/UNIX_manual/ch11s04.html#examples_fo_using_ext_calls
# for details
#
# NOTES:
#	The $(gtm_dist)/plugin directory and the plugins functionality is
#	available since GT.M V5.5-000, for older GT.M versions place pcre.m
#	and pcreexamples.m in your routines directory, and pcre.env and
#	libpcre.so in a directory of your choice, and update pcre.xc and
#	.profile accordingly to point those files.
#
#	The $(gtm_dist)/plugin/o directory should be writable the GT.M user.
#
#	The script will succeed if your version of GT.M is V5.5-000 or above,
#	and the GT.M user has an access to recursively modify the default
#	permissions for $(gtm_dist)/plugin, you can simply run
#		chown -R gtm $(gtm_dist)/plugin
#	as root to achieve that (change the "gtm" to your GT.M system user).
#
install:
	chmod u+w -R $(gtm_dist)/plugin
	cp pcre.m pcreexamples.m $(gtm_dist)/plugin/r/
	cp libpcre.so pcre.xc pcre.env $(gtm_dist)/plugin/
	sed -i "s|gtm_dist|$(gtm_dist)|" $(gtm_dist)/plugin/pcre.xc $(gtm_dist)/plugin/pcre.env
	echo ". $(gtm_dist)/plugin/pcre.env" >> ~/.profile
