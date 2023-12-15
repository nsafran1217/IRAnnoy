/*
 * SendProntoDemo.cpp
 *
 *  Example for sending pronto codes with the IRremote library.
 *  The code used here, sends NEC protocol data.
 *
 *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
 *
 * Copyright (c) 2020-2022 Armin Joachimsmeyer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************************
 */
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

int channelNum = 2;
String channelToJumpTo = "2"; 
String secondsDelay = "60";
uint32_t millisDelay = 1000000; // 100 seconds
uint32_t nextMillis = 0;

struct TaskParameters
{
  int currentMode;
  uint32_t nextMillis = 0;
  uint32_t millisDelay;
  int channelNum;
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

// Function to create or update the sending task
void createOrUpdateSendingTask(TaskParameters *params)
{
  // Check if a task is already running
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
      params,            // Parameter to pass
      1,                 // Task priority
      &sendingTaskHandle // Task handle
  );
}
void startTask()
{
  // Create an instance of TaskParameters
  TaskParameters taskParams;

  // Set initial values for the variables
  taskParams.currentMode = currentMode;
  taskParams.nextMillis = 0;
  taskParams.millisDelay = millisDelay;
  taskParams.channelNum = channelNum;

  // Call the function to create or update the sending task
  createOrUpdateSendingTask(&taskParams);
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
            if (header.indexOf("GET /mode/off") >= 0)
            {
              Serial.println("set to off");
              currentMode = 0;
              if (sendingTaskHandle != NULL)
              {
                // Delete the existing task
                vTaskDelete(sendingTaskHandle);
                sendingTaskHandle = NULL;
              }
            }
            else if (header.indexOf("GET /mode/JumpDelay") >= 0)
            {
              Serial.println("set toJumpDelay");
              currentMode = 1;
              startTask();
            }
            else if (header.indexOf("GET /mode/JumpRandom") >= 0)
            {
              Serial.println("set to JumpRandom");
              currentMode = 2;
              startTask();
            }
            // Handle form submission
            else if (header.indexOf("GET /channelNumber") >= 0)
            {
              int paramIndex = header.indexOf("channelNumberInput=");
              if (paramIndex >= 0)
              {
                int endIndex = header.indexOf(" ", paramIndex);
                String numberValue = header.substring(paramIndex + 19, endIndex);
                channelToJumpTo = numberValue;
                Serial.println("Set channelToJumpTo to: " + channelToJumpTo);
                channelNum = channelToJumpTo.toInt();
              }
            }
            // Handle form submission for delayNumber
            else if (header.indexOf("GET /delayNumber") >= 0)
            {
              int paramIndex = header.indexOf("delayNumberInput=");
              if (paramIndex >= 0)
              {
                int endIndex = header.indexOf(" ", paramIndex);
                String delayValue = header.substring(paramIndex + 17, endIndex);
                secondsDelay = delayValue;
                Serial.println("Set secondsDelay to: " + secondsDelay);
                millisDelay = secondsDelay.toInt() * 1000;
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

            // Display current channelToJumpTo value
            client.println("<p>Current Favorite: " + channelToJumpTo + "</p>");

            client.println("<p>Current delay: " + secondsDelay + "</p>");

            // Add a form with a text input for the user to enter a number
            client.println("<form action=\"/channelNumber\" method=\"get\">");
            client.println("<label for=\"channelNumberInput\">Enter a Favorite:</label>");
            client.println("<input type=\"text\" id=\"channelNumberInput\" name=\"channelNumberInput\" required>");
            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");

            // Add a form with a text input for the user to enter a number
            client.println("<form action=\"/delayNumber\" method=\"get\">");
            client.println("<label for=\"delayNumberInput\">Enter a delay(sec):</label>");
            client.println("<input type=\"text\" id=\"delayNumberInput\" name=\"delayNumberInput\" required>");
            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");

            client.println("<p><a href=\"/mode/JumpDelay\"><button class=\"button\">jump delay to favorite " + channelToJumpTo + "</button></a></p>");
            client.println("<p><a href=\"/mode/JumpRandom\"><button class=\"button\">jump random to favorite " + channelToJumpTo + "</button></a></p>");

            client.println("<p><a href=\"/mode/off\"><button class=\"button\">OFF</button></a></p>");

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

  while (1)
  {
    delay(1000);
    Serial.println("waiting");

    // Access variables using the structure
    switch (params->currentMode)
    {
    case 0:
      Serial.println("doing nothing");
      break;
    case 1:
      if (millis() > params->nextMillis)
      {
        params->nextMillis = millis() + params->millisDelay;
        Serial.println("Sending delay");
        irsend.sendPronto(numberBTNs[params->channelNum], NUMBER_OF_REPEATS);
      }
      break;
    case 2:
      if (millis() > params->nextMillis)
      {
        params->nextMillis = millis() + random(300000, 360000);
        Serial.println("Sending random");
        irsend.sendPronto(numberBTNs[params->channelNum], NUMBER_OF_REPEATS);
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