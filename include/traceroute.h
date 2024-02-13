#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

class ICMP_packet {
private:
   const char* _buffer;
   size_t      _buffer_size;

   const struct iphdr*    _iphdr;
   const struct icmphdr*  _icmphdr;
   const struct udphdr*   _udphdr;

public:
   ICMP_packet( const char* buffer, size_t size ) {
      size_t headers_size = 0;
      headers_size += sizeof( struct iphdr );
      headers_size += sizeof( struct icmphdr );
      // The original package headers (IP and UDP) are included 
      headers_size += sizeof( struct iphdr );
      headers_size += sizeof( struct udphdr );

      if ( size < headers_size ) {
         puts( "ICMP_packet(): buffer size too small" );
         exit( EXIT_FAILURE );
      }

      _buffer      = buffer;
      _buffer_size = size;

      _iphdr   = (struct iphdr*)_buffer;
      _icmphdr = (struct icmphdr*) ( (char*)_iphdr + sizeof( struct iphdr ) );
      _udphdr  = (struct udphdr*) ( (char*)_icmphdr + sizeof( struct icmphdr) + 
                                    sizeof( struct iphdr ) );
   }

   size_t buffer_size() const { return _buffer_size; }

   const struct iphdr* iphdr() const { return _iphdr; }
   const struct icmphdr* icmphdr() const { return _icmphdr; }
   const struct udphdr*  udphdr() const { return _udphdr; }
};