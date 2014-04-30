#Senior_Design#

###Current design###
4 "devices", communication is from top to bottom and vice versa:

* Arduino + Ethernet Shield -> (Ethernet cable) -> Router
* Router -> (Ethernet cable) -> PLN 1
* PLN 1 -> power line -> PLN 2
* PLN 2 -> (Ethernet cable) -> Arduino + Ethernet Sheild

####Information####
Ensure that the node devices all have different MAC addresses to get different IP addresses from the host router. The router should be set up to deliver a certain IP address to the MAC address of the base station. In our project's current state, it is meant to be delivered 192.168.2.50 but will setup its own IP address if that one is unobtainable. The nodes will just listen for the IP address so this is mainly important for access the website, which you will require you to know the IP address.

###Powerline###
![Powerline](https://raw.github.com/keenanjohnson/Senior_Design/master/Sweet_Pics/powerline__a_goofy_movie_10_by_xxsteefylovexx-d4lc7ph.png)
