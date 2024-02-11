#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

const uint16_t TRACEROUTE_PORT = 33445; // Within 33434-33534

void resolve_host( const char*, uint32_t* );

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

   struct in_addr _in_addr = {.s_addr = resolved_ip };
   std::string _in_addr_str = std::string( inet_ntoa( _in_addr ) );

   std::string first_line = "traceroute to ";
   first_line += std::string( argv[1] );
   first_line += " (" + _in_addr_str + "), 64 hops max";
   std::cout << first_line << std::endl;

   // Create DATAGRAM socket
   int udp_socket = socket( AF_INET, SOCK_DGRAM, 0 );
   if ( udp_socket == -1 ) {
      perror( "socket()" );
      exit( EXIT_FAILURE );
   }
   // Set TTL on socket
   int ttl = 1;
   if ( setsockopt( udp_socket, IPPROTO_IP, IP_TTL, 
                    &ttl, sizeof(ttl) ) == -1 ) {
      perror( "setsockopt()" );
      exit( EXIT_FAILURE );
   }

   // Send UDP packet to host
   struct sockaddr_in host_addr;
   socklen_t addrlen;

   host_addr.sin_family = AF_INET; 
   host_addr.sin_addr = { .s_addr =  resolved_ip };
   host_addr.sin_port = htons( TRACEROUTE_PORT );
   addrlen = sizeof( struct sockaddr_in );

   std::string udp_message = std::string( "trace route for " ) + _in_addr_str;
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
   while ( true ) {
      if ( recvfrom( icmp_socket, buffer, buffer_size, 0, 
                     (struct sockaddr*)&sender_addr, &addrlen ) == -1 ) {
         perror( "recvfrom()" );
         exit( EXIT_FAILURE );
      }

      std::cout << "Received a reply" << std::endl;
      sleep( 1 );
   }

   return 0;
}

void resolve_host( const char* host, uint32_t* resolved_ip ) {
   struct hostent* _hostent = gethostbyname( host );
   if ( _hostent != nullptr ) {
      char** h_addr_list = _hostent->h_addr_list;
      while ( *h_addr_list != nullptr ) {
         *resolved_ip = *(uint32_t*)*h_addr_list;
         // struct in_addr host_addr;
         // host_addr.s_addr = *(uint32_t*)*h_addr_list;

         // resolved_ip = inet_ntoa( host_addr );
         // ++h_addr_list

         // We are interested in only 1 address
         break;
      }
   }
}