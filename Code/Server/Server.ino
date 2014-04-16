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
//byte HTML[] PROGMEM = "<!DOCTYPE html> <html> <head> <title>Open Home Automation</title> <script> function GetArduinoInputs() { nocache = \"&nocache=\" + Math.random() * 1000000; var request = new XMLHttpRequest(); request.onreadystatechange = function() { if (this.readyState == 4) { if (this.status == 200) { if (this.responseXML != null) { // extract XML data from XML file (containing light states) var count_var = this.responseXML.getElementsByTagName('count')[0].childNodes[0].nodeValue; var string1 = \"Light \"; var light_string = \"light\"; var xml_string = ""; document.getElementById(\"input_app\").innerHTML = ""; for( var i = 1; i <= count_var; i++ ) { var element = document.createElement(\"p\"); // Assign different attributes to the element. // construct string xml_string = ""; xml_string = xml_string.concat(light_string, i); // set id element.setAttribute(\"id\", light_string.concat(i)); var foo = document.getElementById(\"input_app\"); //Append the element in page (in span). foo.appendChild(element); document.getElementById(light_string.concat(i)).innerHTML = string1.concat(i, \": \", this.responseXML.getElementsByTagName(xml_string)[0].childNodes[0].nodeValue); } } } } } request.open(\"GET\", \"ajax_inputs\" + nocache, true); request.send(null); setTimeout('GetArduinoInputs()', 1000); } function testFunc() { console.log('Testing'); } </script> </head> <body onload=\"GetArduinoInputs()\"> <h1>Lights and Groups</h1> <p><span id=\"input_app\">Updating...</span></p> <button type = \"button\" onclick=\"testFunc()\">Test Button</button> </body> </html>";

File webFile; 

// Web page server
EthernetServer web_server(80);

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
void StrClear(char *str, char length);
char StrContains(char *str, char *sfind);
void XML_response(EthernetClient cl);
/*------------------------------------------------------------------------
SETUP
------------------------------------------------------------------------*/
void setup() {
    
  Serial.println("Server rebooting");
  
   // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    
    Serial.begin(9600);       // for debugging
    
    // initialize SD card
    Serial.println("Initializing SD card...");
    if (!SD.begin(4)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;    // init failed
    }
    Serial.println("SUCCESS - SD card initialized.");
    // check for index.htm file
    if (!SD.exists("index.htm")) {
        Serial.println("ERROR - Can't find index.htm file!");
        return;  // can't find index file
    }
    Serial.println("SUCCESS - Found index.htm file.");
  
  Serial.println("Server starting");

  // start the Ethernet connection:
 Ethernet.begin(mac, web_ip);

  // Start listening on UDP port
  UDP.begin(localPort);

  // start listening for clients
  server.begin();
  
  // Start HTTP server
  web_server.begin(); 

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
  
   EthernetClient web_client = web_server.available();  // try to get client

    if (web_client) {  // got client?
        Serial.println("Get client");
        boolean currentLineIsBlank = true;
        while (web_client.connected()) {
            if (web_client.available()) {   // client data available to read
                char c = web_client.read(); // read 1 byte (character) from client
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    web_client.println("HTTP/1.1 200 OK");
                    // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    // Ajax request - send XML file
                    if (StrContains(HTTP_req, "ajax_inputs")) {
                        // send rest of HTTP header
                        web_client.println("Content-Type: text/xml");
                        web_client.println("Connection: keep-alive");
                        web_client.println();
                        // send XML file containing input states
                        XML_response(web_client);
                    }
                    else {  // web page request
                        // send rest of HTTP header
                        web_client.println("Content-Type: text/html");
                        web_client.println("Connection: keep-alive");
                        web_client.println();
                        // send web page
                        webFile = SD.open("index.htm");        // open web page file
                        if (webFile) {
                            while(webFile.available()) {
                                web_client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }
                    // display received HTTP request on serial port
                    Serial.print(HTTP_req);
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        web_client.stop(); // close the connection
        Serial.println("Conn closed");
    } // end if (client)
  
  
  ////////////////////////
  // Powerline Network
  ////////////////////////
  
  // Broadcast IP address
  UDP.beginPacket( udp_ip, localPort );
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

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}

// send the XML file with switch statuses and analog value
void XML_response(EthernetClient cl)
{
    int analog_val;
    
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    // light count
    cl.print("<count>");
    cl.print(3);
    cl.print("</count>");
    // button 1, pin 7
    cl.print("<light1>");
    if (digitalRead(7)) {
        cl.print("ON");
    }
    else {
        cl.print("OFF");
    }
    cl.print("</light1>");
    // button 2, pin 8
    cl.print("<light2>");
    if (digitalRead(8)) {
        cl.print("ON");
    }
    else {
        cl.print("OFF");
    }
    cl.print("</light2>");
    // button 3, pin 6
    cl.print("<light3>");
    if (digitalRead(6)) {
        cl.print("ON");
    }
    else {
        cl.print("OFF");
    }
    cl.print("</light3>");
    
    // read analog pin A2
    analog_val = analogRead(2);
    cl.print("<analog1>");
    cl.print(analog_val);
    cl.print("</analog1>");
    cl.print("</inputs>");
}

