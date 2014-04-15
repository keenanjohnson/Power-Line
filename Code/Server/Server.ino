/*
  Arduino Server Basestation
 
 Server program to run Arduino + Ethernet shield plugged into router via
 Ethernet cable communicating to a PLN device also connected to the router
 via an Ethernet cable.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 
 */

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include "WebServer.h"

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
  NODE_KEEP_ALIVE,
  // add new cmds here
  
  NODE_CMDS_CNT
};

const char* node_cmds_string[] =
{
  "OFF",
  "ON",
  "SAVE ID",
  "KEEP ALIVE",
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
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01 };

// default to 11700
EthernetServer server(11700);

/* This creates an instance of the webserver.  By specifying a prefix
 * of "", all pages will be at the root of the server. */
#define PREFIX ""
WebServer webserver(PREFIX, 80);

// whether or not the client was connected previously
boolean alreadyConnected = false;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP UDP;
byte IP_addr[4];
unsigned int localPort = 8888;      // local port to listen on
IPAddress ip(255, 255, 255, 255);

EthernetClient old_client;

File webFile; 

/*------------------------------------------------------------------------
FUNCTION PROTOTYPES
------------------------------------------------------------------------*/
void print_local_ip_address();
void save_local_ip_address();
int send_cmd_to_client( const int &id, const node_cmds &cmd, const byte &data, EthernetClient* client );
void Cmd_Center(WebServer &Web_server, WebServer::ConnectionType type, char *, bool);

/*------------------------------------------------------------------------
SETUP
------------------------------------------------------------------------*/
void setup() {
  
  // disable Ethernet chip
  // presumably to prevent SPI bus
  // conflicts
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
  
 // Open serial communications and wait for port to open:
  Serial.begin(9600);

  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  pinMode(10, OUTPUT);

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");


  // start the Ethernet connection:
  while(Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Wait a bit and try again
    delay(100);
  }
  
    /* setup our default command that will be run when the user accesses
   * the root page on the server */
  webserver.setDefaultCommand(&Cmd_Center);

  /* start the webserver */
  webserver.begin();

  // Start listening on UDP port
  UDP.begin(localPort);

  // start listening for clients
  server.begin();

  // print and save local IP address
  print_local_ip_address();
  save_local_ip_address();

}

/*------------------------------------------------------------------------
LOOP
------------------------------------------------------------------------*/
void loop() {
  
  #define web_buf_size 64
  
  char web_buff[web_buf_size];
  int web_buf_len = web_buf_size;
  
  /* process incoming connections one at a time forever */
  webserver.processConnection(web_buff, &web_buf_len);
  
  // Broadcast IP address
  UDP.beginPacket( ip, localPort );
  UDP.write( IP_addr, 4 );
  UDP.endPacket();
  Serial.println("Sending IP address");

  // wait for a new client:
  EthernetClient client = server.available();
  
  if( client ) {
    if (!alreadyConnected) {
      // clead out the input buffer:
      client.flush();
      Serial.println("We have a new client");
      alreadyConnected = true;

      send_cmd_to_client( 0x01, NODE_SAVE_ID, 0x01, &client );
    } 

    while(client.available() > 0) {
      // read the bytes incoming from the client:
      char thisChar = client.read();
      // echo the bytes back to the client:
      // server.write(thisChar);
      // echo the bytes to the server as well:
      Serial.write(thisChar);
    }

    old_client = client;
  }

  if( old_client && old_client.connected() && send_cmd_to_client( 0x01, NODE_KEEP_ALIVE, 0x00, &old_client ) ) {
    static int count = 0;

    if( count == 0 ) {
      send_cmd_to_client( 0x01, NODE_ON, 0x02, &old_client );
      count++;
    } else if( count == 1 ) {
      send_cmd_to_client( 0x01, NODE_SAVE_ID, 0x02, &old_client );
      count++;
    } else {
      send_cmd_to_client( 0x01, NODE_OFF, 0x02, &old_client );
      delay(100);
      count = 0;
    }
  } else {
    if( old_client ) {
      old_client.stop();
    }
    alreadyConnected = false;
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

void save_local_ip_address()
{
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    IP_addr[thisByte] = Ethernet.localIP()[thisByte];
  }
}

int send_cmd_to_client( const int &id, const node_cmds &cmd, const byte &data, EthernetClient* client )
{
  node_packet packet;
  packet.id = id;
  packet.cmd = cmd;
  packet.data = data;

  Serial.print("ID: "); Serial.print( packet.id );
  Serial.print(" CMD: "); Serial.print( node_cmds_string[ packet.cmd ] );
  Serial.print(" Data: "); Serial.print( packet.data, HEX );
  Serial.println();

  return (*client).write( (byte*)&packet, sizeof( packet ) );
}

// This is the function that process all incoming web connections
// for now. It posts the http page below
void Cmd_Center(WebServer &Web_server, WebServer::ConnectionType type, char *, bool)
{
  Serial.println("Connection received bixxxxxxx");
  
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  Web_server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type != WebServer::HEAD)
  {
    /* this defines some HTML text in read-only memory aka PROGMEM.
     * This is needed to avoid having the string copied to our limited
     * amount of RAM. */
   // P(helloMsg) = "<h1>Hello, World Bop!</h1>";
    
    // send web page
    webFile = SD.open("index.htm");        // open web page file
    if (webFile) {
        while(webFile.available()) {
            Web_server.write(webFile.read()); // send web page to client
        }
        webFile.close();
    }
  }
}
