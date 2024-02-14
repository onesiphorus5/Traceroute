This is a "trace route" program. To run the program:
1. Compile the program by running "make mytraceroute"  
2. Run the program by running "sudo ./mytraceroute \<host IP\> or \<host name\>  
You should then see the list of gateways packets would traverse to reach the target host from your local computer.  
E.g.: sudo ./mytraceroute www.google.com