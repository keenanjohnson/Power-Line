/*
  Arduino Client Node
 
 Client program to run Arduino + Ethernet shield plugged into PLN device
 via Ethernet cable communicating to a PLN device also connected to the
 router then to the basestation on the other side.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 
 */

#include <SPI.h>
#include <Ethernet.h>

/*------------------------------------------------------------------------
CONSTANTS/TYPES
------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////
// KEEP TWO SKETCHES IN SYNC BECAUSE ARDUINO'S IDE IS DUMB ///////////////
//////////////////////////////////////////////////////////////////////////
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

typedef byte node_cmds; enum {
  NODE_OFF = 0,
  NODE_ON,
  NODE_SAVE_ID,
  // add new cmds here
  
  NODE_CMDS_CNT
};

const char* node_cmds_string[] =
{
  "OFF",
  "ON",
  "SAVE ID"
};
STATIC_ASSERT( sizeof( node_cmds_string ) / sizeof( char* ) == NODE_CMDS_CNT, make_arrays_same_size );

typedef struct __node_packet {
  byte id;
  node_cmds cmd;
  byte data;
} node_packet;
//////////////////////////////////////////////////////////////////////////

/*------------------------------------------------------------------------
VARIABLES
------------------------------------------------------------------------*/
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x04 };

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP UDP;
unsigned int localPort = 8888;      // local port to listen on

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
boolean IP_found = false;

/*------------------------------------------------------------------------
FUNCTION PROTOTYPES
------------------------------------------------------------------------*/
void print_local_ip_address();
void process_cmd_packet( const node_packet &pkt );

/*------------------------------------------------------------------------
SETUP
------------------------------------------------------------------------*/
void setup() {
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // start the Ethernet connection:
  while(Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Wait a bit and try again
    delay(100);
  }
  
  // Start listening on UDP port
  UDP.begin(localPort);

  // print local IP address
  print_local_ip_address();
}

/*------------------------------------------------------------------------
LOOP
------------------------------------------------------------------------*/
void loop() {
    // if there's data available, read a packet
  int packetSize = UDP.parsePacket();

  if(packetSize && !IP_found)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = UDP.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(UDP.remotePort());

    // read the packet into packetBufffer
    UDP.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);
    
    IP_found = true;
    
    client.connect( UDP.remoteIP(), 11700);
    
    while( !client.connected() ) {
      Serial.println("Waiting to connect...");
    }
    
    // bring up connection
    client.println("A");
    
    Serial.write("DATA SENT");
    Serial.println();
  }

  if( !client.connected() ) {
    IP_found = false;
  }

  if( IP_found )
  {
    boolean data_read = false;
    node_packet packet;

    while( !data_read ) {
      if( client.available() >= sizeof( node_packet ) ) {
        Serial.print( "Node Packet detected!" );
        Serial.println();
        data_read = true;
        
        for( int i = 0; i < sizeof( node_packet ); i++ ) {
          ((byte*)&packet)[i] = client.read();
        }
      }

      // TODO MR: may want to flush data from buffer here
    }
    
    if( data_read ) {
      Serial.print("ID: "); Serial.print( packet.id );
      Serial.print(" CMD: "); Serial.print( node_cmds_string[ packet.cmd ] );
      Serial.print(" Data: "); Serial.print( packet.data, HEX );
      Serial.println();

      process_cmd_packet( packet );
    }

  } else {
    Serial.write("IP NOT FOUND");
    Serial.println();
  }
}

/*------------------------------------------------------------------------
FUNCTION DEFINITIONS
------------------------------------------------------------------------*/
void print_local_ip_address()
{
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();
}

void process_cmd_packet( const node_packet &pkt )
{
  switch( pkt.cmd ) {
    case NODE_OFF:
      break;
    case NODE_ON:
      break;
    case NODE_SAVE_ID:
      break;
    default:
      Serial.println("CMD NOT KNOWN");
      break;
  }
}
