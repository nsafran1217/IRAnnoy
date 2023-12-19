
#include <Arduino.h>

#define DISABLE_CODE_FOR_RECEIVER // Disables restarting receiver after each send. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not used.
#include <IRremote.hpp>
#include <WiFi.h>

#include "pins.h" // Define macros for input and output pin etc.
#include "codes.h"

#define NUMBER_OF_REPEATS 3U

const char *ssid = "Sirrius";
const char *password = "1234567890";

WiFiServer server(80);
String header;

int currentMode = 0; // Off, jumpDelay, jumpRandom
String modes[] = {"Off", "Jump Delay", "Jump Random"};

int favoriteNum = 2;
String favoriteToJumpTo = "2";
String secondsDelay = "60";
uint32_t millisDelay = 1000000; // 100 seconds
uint32_t nextMillis = 0;

struct TaskParameters
{
  int currentMode;
  uint32_t millisDelay;
  int favoriteNum;
  // Add other required variables here
};
IRsend irsend;
void sendCommand();
void doTheSendingTask(void *parameter);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  // Just to know which program is running on my Arduino
  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
  Serial.println(F("Send IR signals at pin " STR(IR_SEND_PIN)));

  IrSender.begin(4, ENABLE_LED_FEEDBACK, 2); // Start with IR_SEND_PIN as send pin and enable feedback LED at default feedback LED pin

  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
}

TaskHandle_t sendingTaskHandle = NULL;

void startTask()
{
  // Create an instance of TaskParameters
  TaskParameters *taskParams = new TaskParameters;

  // Set initial values for the variables
  taskParams->currentMode = currentMode;
  taskParams->millisDelay = millisDelay;
  taskParams->favoriteNum = favoriteNum;

  if (sendingTaskHandle != NULL)
  {
    // Delete the existing task
    vTaskDelete(sendingTaskHandle);
    sendingTaskHandle = NULL;
  }

  // Create a new task with the updated parameters
  xTaskCreate(
      doTheSendingTask,  // Function that should be called
      "Send IR code ",   // Name of the task (for debugging)
      10000,             // Stack size (bytes)
      taskParams,        // Parameter to pass
      1,                 // Task priority
      &sendingTaskHandle // Task handle
  );
}

void cleanupTask(TaskParameters *params)
{
  delete params;
}
void KillTask()
{
  currentMode = 0;
  if (sendingTaskHandle != NULL)
  {
    // Delete the existing task
    vTaskDelete(sendingTaskHandle);
    sendingTaskHandle = NULL;
  }
}
String getTextBoxValue(String InputName)
{
  int paramIndex = header.indexOf(InputName + "=");
  if (paramIndex >= 0)
  {
    int endIndex = header.indexOf(" ", paramIndex);
    String numberValue = header.substring(paramIndex + InputName.length() + 1, endIndex);
    return numberValue;
  }
  return "";
}

void setChannel(int channel)
{
  Serial.println(channel);
  int channelTens = channel % 10;
  int channelOnes = channel / 10;
  irsend.sendPronto(DirectTuneBTN, NUMBER_OF_REPEATS);
  delay(400);
  irsend.sendPronto(numberBTNs[channelOnes], NUMBER_OF_REPEATS);
  delay(400);
  irsend.sendPronto(numberBTNs[channelTens], NUMBER_OF_REPEATS);
}

void powerBtn()
{
  irsend.sendPronto(PowerBTN, NUMBER_OF_REPEATS);
}
void loop()
{

  WiFiClient client = server.available(); // Listen for incoming clients

  if (client)
  {                                // If a new client connects,
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected())
    { // loop while the client's connected
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /mode/Modeoff") >= 0)
            {
              KillTask();
              Serial.println("set to off");
              currentMode = 0;
            }
            else if (header.indexOf("GET /mode/Pwr") >= 0)
            {
              KillTask();
              Serial.println("PWR");
              currentMode = 0;
              powerBtn();
            }

            else if (header.indexOf("GET /mode/JumpDelay") >= 0)
            {
              KillTask();
              Serial.println("set toJumpDelay");
              currentMode = 1;
              startTask();
            }
            else if (header.indexOf("GET /mode/JumpRandom") >= 0)
            {
              KillTask();
              Serial.println("set to JumpRandom");
              currentMode = 2;
              startTask();
            }
            // Handle form submission
            else if (header.indexOf("GET favoriteNumber") >= 0)
            {
              KillTask();
              if (favoriteToJumpTo = getTextBoxValue("favoriteNumberInput"))
              {
                Serial.println("Set favoriteToJumpTo to: " + favoriteToJumpTo);
                favoriteNum = favoriteToJumpTo.toInt();
              }
            }
            // Handle form submission for delayNumber
            else if (header.indexOf("GET /delayNumber") >= 0)
            {
              KillTask();
              if (secondsDelay = getTextBoxValue("delayNumberInput"))
              {
                Serial.println("Set secondsDelay to: " + secondsDelay);
                millisDelay = secondsDelay.toInt() * 1000;
              }
            }
            else if (header.indexOf("GET /setChannel") >= 0)
            {
              KillTask();
              if (String channel = getTextBoxValue("setChannelInput"))
              {
                Serial.println("Set channel to: " + channel);
                setChannel(channel.toInt());
              }
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>Sirrius</h1>");

            // Display current state
            client.println("<p>Current Mode: " + modes[currentMode] + "</p>");

            // Display current values
            client.println("<p>Current Favorite: " + favoriteToJumpTo + "</p>");
            client.println("<p>Current delay: " + secondsDelay + "</p>");

            // form to get favorite number
            client.println("<form action=\"favoriteNumber\" method=\"get\">");
            client.println("<label for=\"favoriteNumberInput\">Enter a Favorite:</label>");
            client.println("<input type=\"text\" id=\"favoriteNumberInput\" name=\"favoriteNumberInput\" required>");
            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");

            // for to get delay time
            client.println("<form action=\"/delayNumber\" method=\"get\">");
            client.println("<label for=\"delayNumberInput\">Enter a delay(sec):</label>");
            client.println("<input type=\"text\" id=\"delayNumberInput\" name=\"delayNumberInput\" required>");
            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");

            // buttons
            client.println("<p><a href=\"/mode/JumpDelay\"><button class=\"button\">jump delay to favorite " + favoriteToJumpTo + "</button></a></p>");
            client.println("<p><a href=\"/mode/JumpRandom\"><button class=\"button\">jump random to favorite " + favoriteToJumpTo + "</button></a></p>");
            client.println("<p><a href=\"/mode/Modeoff\"><button class=\"button\">MODE OFF</button></a></p>");


            // form to go to channel
            client.println("<form action=\"/setChannel\" method=\"get\">");
            client.println("<label for=\"setChannelInput\">GoTo Channel:</label>");
            client.println("<input type=\"text\" id=\"setChannelInput\" name=\"setChannelInput\" required>");
            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");

            client.println("<p><a href=\"/mode/Pwr\"><button class=\"button\">POWER</button></a></p>");
            //client.println("<p><a href=\"/mode/PwrOff\"><button class=\"button\">OFF</button></a></p>");

            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }

    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void doTheSendingTask(void *parameter)
{
  // Cast the parameter back to the structure type
  TaskParameters *params = (TaskParameters *)parameter;

  uint32_t nextMillisTask = 0;
  int mode = params->currentMode;
  uint32_t millisDelayTask = params->millisDelay;
  int channelTask = params->favoriteNum;
  cleanupTask(params);
  while (1)
  {

    delay(1000);
    Serial.print("waiting:");
    Serial.println(mode);

    // Access variables using the structure
    switch (mode)
    {
    case 0:
      Serial.println("doing nothing");
      break;
    case 1:
      if (millis() > nextMillisTask)
      {
        nextMillisTask = millis() + millisDelayTask;
        Serial.println("Sending delay");
        irsend.sendPronto(numberBTNs[channelTask], NUMBER_OF_REPEATS);
      }
      break;
    case 2:
      if (millis() > nextMillisTask)
      {
        nextMillisTask = millis() + random(300000, 360000);
        Serial.println("Sending random");
        irsend.sendPronto(numberBTNs[channelTask], NUMBER_OF_REPEATS);
      }
      break;
    default:
      break;
    }
  }
}
void sendCommand()
{
  Serial.println("Sending from normal memory");
  irsend.sendPronto(ChannelDownBTN, NUMBER_OF_REPEATS);
}