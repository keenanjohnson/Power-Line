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

#define PACKET_WAIT_TIMEOUT 25
#define LOOP_COUNT_RESET    4000

typedef byte node_cmds; enum {
  NODE_OFF = 0,
  NODE_ON,
  NODE_SAVE_ID,
  NODE_KEEP_ALIVE,
  NODE_STATUS,
  // add new cmds here
  
  NODE_CMDS_CNT
};

const char *node_cmds_string[] =
{   
  "OFF\0",
  "ON\0",
  "SAVE ID\0",
  "KEEP ALIVE\0",
  "STATUS\0",
};
STATIC_ASSERT( sizeof( node_cmds_string ) / sizeof( char* ) == NODE_CMDS_CNT, make_arrays_same_size );

typedef struct __node_packet {
  byte id;
  node_cmds cmd;
  byte data;
} node_packet;
//////////////////////////////////////////////////////////////////////////

#define MAX_SERVER_CONNECTIONS MAX_SOCK_NUM

/*------------------------------------------------------------------------
VARIABLES
------------------------------------------------------------------------*/

/*------------------------------------------------------------
WebDuino
------------------------------------------------------------*/
File webFile; 

/* This creates an instance of the webserver.  By specifying a prefix
 * of "", all pages will be at the root of the server. */
#define PREFIX ""
WebServer webserver(PREFIX, 80);

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01 };

IPAddress web_ip(192,168,2,210);

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   50

// buffered HTTP request stored as null terminated string
char HTTP_req[REQ_BUF_SZ] = {0};
// index into HTTP_req buffer
char req_index = 0;

/*------------------------------------------------------------
PLC
------------------------------------------------------------*/
// default to 11700
EthernetServer server(11700);

// whether or not the client was connected previously
boolean alreadyConnected = false;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP UDP;
byte IP_addr[4];
unsigned int localPort = 8888;      // local port to listen on
IPAddress udp_ip(255, 255, 255, 255);

EthernetClient old_client[MAX_SERVER_CONNECTIONS];

/*------------------------------------------------------------
Node processing
------------------------------------------------------------*/
boolean node_status[MAX_SERVER_CONNECTIONS];
uint16_t loop_count[MAX_SERVER_CONNECTIONS];
int connected_node_cnt;

/*------------------------------------------------------------------------
FUNCTION PROTOTYPES
------------------------------------------------------------------------*/
void print_local_ip_address();
void process_cmd_packet( const node_packet &pkt );
void save_local_ip_address();
int send_cmd_to_client( const int &id, const node_cmds &cmd, const byte &data, EthernetClient* client );
void web_response(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
void update_status(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);

/*------------------------------------------------------------------------
SETUP
------------------------------------------------------------------------*/
void setup() {
  Serial.begin(9600);

  Serial.println("Server Starting");
  
  // disable Ethernet chip
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
 
  // initialize SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
      Serial.println("ERROR - SD card initialization failed!");
      return;    // init failed
  }

  // DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // initialize the ethernet device not using DHCP:
    Ethernet.begin(mac, web_ip);
  }

  // Start listening on UDP port
  UDP.begin(localPort);

  // start listening for clients
  server.begin();
  
  /* setup our default command that will be run when the user accesses
   * the root page on the server */
  webserver.setDefaultCommand(&web_response);
  
  webserver.addCommand("Status", &update_status);
  
  /* start the webserver */
  webserver.begin();

  // print and save local IP address
  print_local_ip_address();
  save_local_ip_address();
  
  // node processing
  memset( &loop_count, 0x00, sizeof( loop_count ) );
  connected_node_cnt = 0;
  
  Serial.println("Setup finished");
}

/*------------------------------------------------------------------------
LOOP
------------------------------------------------------------------------*/
void loop() {
  
  ////////////////////////
  // Web Server
  /////////////////////////
  
  char buff[64];
  int len = 64;

  /* process incoming connections one at a time forever */
  webserver.processConnection(buff, &len);
  
  ////////////////////////
  // Powerline Network
  ////////////////////////
  
  // broadcast IP address if room for more nodes
  if( connected_node_cnt < MAX_SERVER_CONNECTIONS ) {
    // Broadcast IP address
    UDP.beginPacket( udp_ip, localPort );
    UDP.write( IP_addr, 4 );
    UDP.endPacket();
  }

  // wait for a new client:
  EthernetClient client = server.available();
  
  if( client ) {
    old_client[0x01] = client;
    
    if (!alreadyConnected) {
      // clead out the input buffer:
      client.flush();
      Serial.println("We have a new client");
      alreadyConnected = true;

      send_cmd_to_client( 0x01, NODE_SAVE_ID, 0x01, &client );
    }

    boolean data_read = false;
    node_packet packet;

    if( client.available() >= sizeof( node_packet ) ) {
      data_read = true;
      
      client.read( (byte*)&packet, sizeof( node_packet ) );
    }
    
    if( data_read ) {
      if( packet.cmd != NODE_KEEP_ALIVE || 1) {
        Serial.print("RECEIVED --- ");
        Serial.print("ID: "); Serial.print( packet.id );
        Serial.print(" CMD: "); Serial.print( node_cmds_string[ packet.cmd ] );
        Serial.print(" Data: "); Serial.print( packet.data, HEX );
        Serial.println();
      }

      process_cmd_packet( packet );
    }
  }

  if( old_client[0x01] ) {
    loop_count[0x01]++;
    
    // haven't seen a response from client in timeout
    if( loop_count[0x01] > (2 * LOOP_COUNT_RESET ) ) {
      // disconnect node
      Serial.println("Disconnecting node...");
      old_client[0x01].flush();
      old_client[0x01].stop();
      alreadyConnected = false;
      loop_count[0x01] = 0;
    }
  } else {
    if( old_client[0x01] ) {
      old_client[0x01].flush();
      old_client[0x01].stop();
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

void process_cmd_packet( const node_packet &pkt )
{
  loop_count[0x01] = 0;

  switch( pkt.cmd ) {
    case NODE_OFF:
      break;
    case NODE_ON:
      break;
    case NODE_SAVE_ID:
      break;
    case NODE_KEEP_ALIVE:
      send_cmd_to_client( pkt.id, NODE_KEEP_ALIVE, 0x00, &(old_client[0x01]) );
      Serial.print( "NODE_STATUS: " );
      Serial.println( pkt.data );
      if( pkt.data == HIGH ) {
        node_status[pkt.id] = true;
      } else {
        node_status[pkt.id] = false;
      }
      break;
    case NODE_STATUS:
      Serial.print( "NODE_STATUS: " );
      Serial.println( pkt.data );
      if( pkt.data == HIGH ) {
        node_status[pkt.id] = true;
      } else {
        node_status[pkt.id] = false;
      }
      break;
    default:
      Serial.println("CMD NOT KNOWN");
      break;
  }
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

  if( cmd != NODE_KEEP_ALIVE || 1 ) {
    Serial.print("SENDING --- ");
    Serial.print("ID: "); Serial.print( packet.id );
    Serial.print(" CMD: "); Serial.print( node_cmds_string[ packet.cmd ] );
    Serial.print(" Data: "); Serial.print( packet.data, HEX );
    Serial.println();
  }

  return (*client).write( (byte*)&packet, sizeof( node_packet ) );
}

/* commands are functions that get called by the webserver framework
 * they can read any posted data from client, and they output to the
 * server to send data back to the web browser. */
void web_response(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  
  // ////////////////////////////////////
  // Handling POST requests from website
  ///////////////////////////////////////
  if ( type == WebServer::POST)
  {
    Serial.println("Post received");
    bool repeat;
    char name[16], value[16];
    
    /* readPOSTparam returns false when there are no more parameters
     * to read from the input.  We pass in buffers for it to store
     * the name and value strings along with the length of those
     * buffers. */
    repeat = server.readPOSTparam(name, 16, value, 16);
    
    // Light ON
    if ( strcmp(name, "ButtonOn") == 0)
    {
        // Turn light on
        Serial.println("Light on Command");
        send_cmd_to_client( 0x01, NODE_ON, 0x00, &(old_client[0x01]) );
        node_status[0x01] = true;
    }
    //Light off
    if ( strcmp(name, "ButtonOff") == 0)
    {
        // Turn light off
        Serial.println("Light off Command");
        send_cmd_to_client( 0x01, NODE_OFF, 0x00, &(old_client[0x01]) );
        node_status[0x01] = false;
    }
    
    return;
  }
  
  
  // ////////////////////////////////////
  // Handling GET for website page sending
  ///////////////////////////////////////
   /* for a GET or HEAD, send the standard "it's all OK headers" */
  server.httpSuccess();
  
  if (type == WebServer::GET)
  {
    Serial.println(url_tail);
    URLPARAM_RESULT rc;
    char name[32];
    char value[32];
    
    // Write HTML file out to client
    webFile = SD.open("index.htm");        // open web page file
    if (webFile) {
        while(webFile.available()) {
            server.write(webFile.read()); // send web page to client
        }
        webFile.close();
    }
  }
}

// Handles sending out LED status updates
void update_status(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  // Grabbing status from node's keep_alive msg to avoid traffic
  //send_cmd_to_client( 0x01, NODE_STATUS, 0x00, &(old_client[0x01]) );
  
  server.print("<?xml version = \"1.0\" ?>");
  server.print("<inputs>");
  // light count
  server.print("<count>");
  server.print(1);
  server.print("</count>");
  // button 1, pin 7
  server.print("<light1>");
  if( node_status[0x01] ) {
      server.print("ON");
  }
  else {
      server.print("OFF");
  }
  server.print("</light1>");
  server.print("</inputs>");
}
