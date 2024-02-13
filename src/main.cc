#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <iostream>

#include "traceroute.h"

const uint16_t HOST_PORT = 33445;      // Within 33434-33534
const uint8_t  MAX_HOP_COUNT = 64;

void resolve_host( const char*, uint32_t* );
std::string get_hostname( uint32_t addr );

bool timed_out = false;
void sigalrm_handler( int );

int main( int argc, const char* argv[] ) {
   if ( argc < 2 ) {
      std::string_view exit_msg = " is expected to run with at least one argument";
      std::cout << argv[0] << exit_msg << std::endl;
      exit( EXIT_FAILURE );
   }
   
   uint32_t resolved_ip = 0;
   resolve_host( argv[1], &resolved_ip );
   if ( resolved_ip == 0 ) {
      std::cout << "host " << argv[1] << " can't be resolved" << std::endl;
      exit( EXIT_FAILURE );
   }

   // Install the signal handler
   if ( signal( SIGALRM, sigalrm_handler ) == SIG_ERR ) {
      perror( "signal()" );
      exit( EXIT_FAILURE );
   }

   // Print the first line
   struct in_addr _in_addr = {.s_addr = resolved_ip };
   std::string _in_addr_str = std::string( inet_ntoa( _in_addr ) );

   std::string udp_message = std::string( "trace route for " ) + _in_addr_str;

   std::string first_line = "traceroute to " + std::string( argv[1] );
   first_line += " (" + _in_addr_str + "), ";
   first_line += std::to_string( MAX_HOP_COUNT) + " hops max, ";
   first_line += std::to_string( udp_message.size() ) + " byte packets";
   std::cout << first_line << std::endl;

   // Create a UDP socket
   int udp_socket = socket( AF_INET, SOCK_DGRAM, 0 );
   if ( udp_socket == -1 ) {
      perror( "socket()" );
      exit( EXIT_FAILURE );
   }
   // Bind the UDP socket to a port
   // We will use this port to know the 
   uint16_t local_port = ( getpid() & 0xFFFF ) | 0x8000;
   struct sockaddr_in local_addr;
   local_addr.sin_family = AF_INET;
   local_addr.sin_addr   = { .s_addr = INADDR_ANY };
   local_addr.sin_port   = htons( local_port );
   if ( bind( udp_socket, (const struct sockaddr*)&local_addr, 
              sizeof( struct sockaddr_in ) ) == -1 ) {
      perror( "bind()" );
      exit( EXIT_FAILURE );
   }

   struct sockaddr_in host_addr;
   socklen_t addrlen;
   host_addr.sin_family = AF_INET; 
   host_addr.sin_addr = { .s_addr =  resolved_ip };
   host_addr.sin_port = htons( HOST_PORT );
   addrlen = sizeof( struct sockaddr_in );

   uint32_t reply_addr; // Replying gateway/host IP address
   struct timeval t1, t2;
   double timeInterval;
   for ( int ttl=1; ttl <= MAX_HOP_COUNT; ++ttl ) {
      if ( reply_addr == resolved_ip ) {
         break;
      }

      // reset the timeout flag
      timed_out = false;

      // Set TTL on socket
      if ( setsockopt( udp_socket, IPPROTO_IP, IP_TTL, 
                       &ttl, sizeof(ttl) ) == -1 ) {
         perror( "setsockopt()" );
         exit( EXIT_FAILURE );
      }
   
      // Send UDP packet to host
      if ( gettimeofday( &t1, NULL ) == -1 ) {
         perror( "gettimeofday()" );
         exit( EXIT_FAILURE );
      }
      if ( sendto( udp_socket, udp_message.c_str(), udp_message.size(), 0,  
                   (const struct sockaddr*) &host_addr, addrlen ) == -1  ) {
         perror( "sendto()" );
         exit( EXIT_FAILURE );
      }

      // Receive ICMP packets
      int icmp_socket = socket( AF_INET, SOCK_RAW, IPPROTO_ICMP );
      if ( icmp_socket == -1 ) {
         perror( "socket()" );
         exit( EXIT_FAILURE );
      }

      size_t buffer_size = 1024;
      char buffer[ buffer_size ];
      struct sockaddr_in sender_addr;
      ssize_t recvd_bytes;
      while ( true ) {
         // SIGALRM signal will be delivered to this process after 1 second
         alarm( 1 );

         memset( buffer, 0, buffer_size );
         recvd_bytes = recvfrom( icmp_socket, buffer, buffer_size, 0, 
                                 (struct sockaddr*)&sender_addr, &addrlen );
         if ( recvd_bytes == -1 ) {
            if ( errno == EINTR && timed_out ) {
               if ( gettimeofday( &t2, NULL ) == -1 ) {
                  perror( "gettimeofday()" );
                  exit( EXIT_FAILURE );
               }

               timeInterval = ( t2.tv_sec - t1.tv_sec ) * 1000.0;
               timeInterval += ( t2.tv_usec - t1.tv_usec ) / 1000.0;

               std::cout << "  " << ttl << "   " << "* * * " 
                         << timeInterval << " ms" << std::endl;
               break;
            } else {
               perror( "recvfrom()" );
               exit( EXIT_FAILURE );
            }
         }
  
         ICMP_packet icmp_packet( buffer, recvd_bytes );
         // Update replying gateway/host IP address
         reply_addr = icmp_packet.iphdr()->saddr;
   
         // ICMP reply will have a UDP header in the reply. This UDP header is 
         // the original UDP header to which ICMP is replying.
         // To check if this ICMP packet belongs to this process we should 
         // check the source port, instead of the destination port.
         uint16_t src_port  = icmp_packet.udphdr()->source;
         std::string reply_hostname;
         if ( ntohs( src_port ) == local_port ) {
            if ( gettimeofday( &t2, NULL ) == -1 ) {
               perror( "gettimeofday()" );
               exit( EXIT_FAILURE );
            }
            timeInterval = ( t2.tv_sec - t1.tv_sec ) * 1000.0;
            timeInterval += ( t2.tv_usec - t1.tv_usec ) / 1000.0;

            struct in_addr addr = {.s_addr = reply_addr };
            std::string addr_str = inet_ntoa( addr );
            reply_hostname = get_hostname( reply_addr );
            if ( reply_hostname.empty() ) {
               reply_hostname = addr_str;
            }
            std::cout << "  " << ttl << "   " << addr_str 
                      << " (" << reply_hostname << ") " 
                      << timeInterval << " ms" << std::endl;
            break;
         }
      }

      // Clear any pending alarm
      alarm( 0 );
   }

   return 0;
}

void resolve_host( const char* host, uint32_t* resolved_ip ) {
   struct hostent* _hostent = gethostbyname( host );
   if ( _hostent != nullptr ) {
      char** h_addr_list = _hostent->h_addr_list;
      while ( *h_addr_list != nullptr ) {
         *resolved_ip = *(uint32_t*)*h_addr_list;
         // ++h_addr_list

         // We are interested in only 1 address
         break;
      }
   }
}

std::string get_hostname( uint32_t addr ) {
   std::string hostname;

   struct in_addr _addr = {.s_addr = addr };
   socklen_t addrlen = sizeof( struct in_addr );
   struct hostent* _hostent = gethostbyaddr( &_addr, addrlen, AF_INET );
   if ( _hostent != nullptr ) {
      hostname = _hostent->h_name;
   }

   return hostname;
}


void sigalrm_handler( int sig_num ) {
   timed_out = true;
}