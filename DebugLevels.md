#Debug level reorganization proposal

# Debug Level Reorganization #


The debug levels as defined in squid.conf with

> debug\_options section,level

need better classification. Actually the log is spammed with unecessary messages under level 1 which hide important errors and warnings. This makes it hard for any user even an expert finding what matters.


# Debug Levels #

My suggestion is

  * 0
> > essential startup messages
> > helper initializatin and termination
> > critical errors

  * 1
> > Errors and Warnings which could be corrected with config options
> > Warnings which could help answer problems, as for example fqdncacheParse: No PTR record for ...

  * 2
> > less important warnings but eventually interesting for the advanced user/adm

  * 3
> > debug messages for the curious as http parsings etc

  * 4
> > debug messages which show programm flow, most intersting for developers

  * 5
> > low level debugs which are only interesting for developer work or detailed problem search


# Debug Sections #

The Debug sections eventually need something similare, especially documentation and transparency. On the other side, eventually unecessary since the above level suggestion would make it easy, the section selection could be always ALL, could even vanish and the new config option could be


> debug\_options level [1-5]


The code could be smaller, actually as
> debug(section, level (msg)

could be changed to
> debug(level) (msg)