// Fake user input
String debugInput = "hi";

#include <ArduinoJson.h>

// Include the JSON commands
#include "commands.h"

// Global variable to store the processed output
String commandOutput = "";

// Initialize control system
void setupControl() {
  Serial.println("Control system initialized");
  Serial.print("Debug input: ");
  Serial.println(debugInput);
  
  // Process the debug input through JSON
  commandOutput = lookupInJSON(debugInput);
  Serial.print("Command output: ");
  Serial.println(commandOutput);
}

// Get the processed command output
String getProcessedCommand() {
  return commandOutput;
}

// Process touch coordinates
void processTouch(int x, int y) {
  // For now, just process the debug input again
  commandOutput = lookupInJSON(debugInput);
}

// Look up command in JSON
String lookupInJSON(String input) {
  input.toLowerCase();
  input.trim();
  
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, COMMANDS_JSON);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return "Error: Cannot read commands database";
  }
  
  JsonArray commands = doc.as<JsonArray>();
  
  // First, try exact match
  for (JsonObject cmd : commands) {
    String jsonInput = cmd["input"].as<String>();
    jsonInput.toLowerCase();
    
    if (input == jsonInput) {
      return cmd["output"].as<String>();
    }
  }
  
  // If no exact match found
  return "Command: \"" + input + "\"\nThe command was not found...";
}
