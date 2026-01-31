// commands.h
// This file contains the JSON commands for your smart assistant
// Format: [{"input": "command", "output": "response"}, ...]

const char* COMMANDS_JSON = R"=====([
  {"input": "hello", "output": "Hello! How can I assist you today?"},
  {"input": "hi", "output": "Hi there! What can I do for you?"},
  {"input": "hey", "output": "Hey! Ready to assist you."},
  {"input": "time", "output": "The current time is shown on the display above."},
  {"input": "what time is it", "output": "Check the digital clock on the screen for the exact time."},
  {"input": "current time", "output": "You can see the time at the top of the display."},
  {"input": "clock", "output": "The clock is synchronized with internet time."},
  {"input": "what can you do", "output": "I'm ARC Assistant! I can show time, check weather, control lights, play music, and more."},
  {"input": "status", "output": "System status: All systems operational. WiFi connected. Version 0.4.3"},
  {"input": "system info", "output": "ARC Hub v0.4.3 | WiFi: Connected},
  {"input": "goodbye", "output": "Goodbye! Have a wonderful day!"},
  {"input": "bye", "output": "Bye! See you soon!"},
  {"input": "exit", "output": "Sorry, right now I can't turn off the hardware."},
  {"input": "thank you", "output": "You're welcome! Happy to help."},
  {"input": "thanks", "output": "My pleasure! Let me know if you need anything else."},
  {"input": "test", "output": "Testing! This is fake output text"}
])=====";
