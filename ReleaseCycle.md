# Introduction #

This is more for Adrian to remember what to do then it is for others to do releases. :)

# Process #

## Building the release ##

  * Update ChangeLog.Cacheboy
  * Update new version in configure.in
  * Commit the above
  * do a bootstrap/build/test cycle, just to make sure nothing is strangely busted
  * Lay the tag - svn copy https://cacheboy.googlecode.com/svn/branches/CACHEBOY_PRE https://cacheboy.googlecode.com/svn/tags/CACHEBOY_x.y(.z)
  * Check out a fresh copy of the tag somewhere
  * Remove the .svn stuff (I'm sure I can bypass this via relevant SVN magic!) cd 

&lt;directory&gt;

 ; find . -type d -name .svn | xargs -n 1 rm -rf
  * Build the autoconf/automake environment - sh bootstrap.sh
  * Tar it up - cd .. ; tar cf - CACHEBOY\_x.y(.z) | gzip -9 > CACHEBOY\_x.y(.z).tar.gz

## What to do with the release ##

  * Upload it to the code downloads section
  * Email cacheboy-users@ about it!
  * Update news section on website
  * Update blog!
  * Freshmeat!
  * Update the FreeBSD port