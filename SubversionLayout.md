# Introduction #

The one thing to remember is that development doesn't happen off trunk.

# Layout #

  * Squid-2.HEAD is tracked in /svn/trunk
  * Cacheboy development is in /svn/branches/CACHEBOY\_HEAD
  * Project playpens are in /svn/playpen/ ; generally branched off CACHEBOY\_PRE (previous development branch) or CACHEBOY\_HEAD (current development head)

# Merging #

  * Squid-2.HEAD changes are imported, changeset at a time, into /svn/trunk (so it should mostly resemble the CVS repository)
  * Changes are then merged into /svn/branches/CACHEBOY\_PRE, so it constantly tracks Squid-2.HEAD (this may change if Squid-2.HEAD becomes unnecessarily unstable for some reason..)
  * Playpen work/changes are folded into /svn/branches/CACHEBOY\_PRE when ready

# Releases #

  * Releases are made off /svn/branches/CACHEBOY\_HEAD (previously CACHEBOY\_PRE) by laying down tags in /svn/tags
  * Major releases don't cause a branch yet - the codebase is still undergoing widescale changes and trying to deal with multiple concurrent branches would be a nightmare. This will change when the codebase settles down.