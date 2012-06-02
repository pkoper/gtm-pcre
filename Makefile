# successfully compiled with gcc version 4.6.3 on Debian 6 in GT.M user environment
#
all:
	gcc -Wall -Werror -pedantic -fPIC -shared -o libpcre.so pcre.c $(shell pcre-config --libs) -I$(gtm_dist)
