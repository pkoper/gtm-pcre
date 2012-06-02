#!/bin/sh

# Example installation procedure
# See http://tinco.pair.com/bhaskar/gtm/doc/books/pg/UNIX_manual/ch11s04.html#examples_fo_using_ext_calls for details

cp pcre.m pcreexamples.m ~/.fis-gtm/V5.5-000_x86/r/
cp libpcre.so pcre.xc pcre.env $gtm_dist/plugin/
sed -i "s|gtm_dist|$gtm_dist|" $gtm_dist/plugin/pcre.xc $gtm_dist/plugin/pcre.env
echo ". $gtm_dist/plugin/pcre.env" >> ~/.profile
