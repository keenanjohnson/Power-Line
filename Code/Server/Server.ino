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
  NODE_STATUS,
  // add new cmds here
  
  NODE_CMDS_CNT
};

prog_char string_0[] PROGMEM = "OFF";
prog_char string_1[] PROGMEM = "ON";
prog_char string_2[] PROGMEM = "SAVE ID";
prog_char string_3[] PROGMEM = "KEEP ALIVE";
prog_char string_4[] PROGMEM = "STATUS";

PROGMEM const char *node_cmds_string[] =
{   
  string_0,
  string_1,
  string_2,
  string_3,
  string_4,
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

// default to 11700
EthernetServer server(11700);

// whether or not the client was connected previously
boolean alreadyConnected = false;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP UDP;
byte IP_addr[4];
unsigned int localPort = 8888;      // local port to listen on
IPAddress udp_ip(255, 255, 255, 255);

EthernetClient old_client;

/*------------------------------------------------------------------------
FUNCTION PROTOTYPES
------------------------------------------------------------------------*/
void print_local_ip_address();
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
  
  // Broadcast IP address
  UDP.beginPacket( udp_ip, localPort );
  UDP.write( IP_addr, 4 );
  UDP.endPacket();

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
      }
      //Light off
      if ( strcmp(name, "ButtonOff") == 0)
      {
          // Turn light off
          Serial.println("Light off Command");
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
  server.print("<?xml version = \"1.0\" ?>");
    server.print("<inputs>");
    // light count
    server.print("<count>");
    server.print(1);
    server.print("</count>");
    // button 1, pin 7
    server.print("<light1>");
    if (digitalRead(7)) {
        server.print("ON");
    }
    else {
        server.print("OFF");
    }
    server.print("</light1>");
    server.print("</inputs>");
}
