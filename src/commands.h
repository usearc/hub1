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
  {"input": "weather", "output": "I can provide weather updates. Currently showing clear skies."},
  {"input": "what's the weather", "output": "Weather: Sunny, 72°F, perfect day!"},
  {"input": "weather forecast", "output": "Forecast: Clear skies all day. Light breeze from the west."},
  {"input": "temperature", "output": "Current temperature is 72 degrees Fahrenheit."},
  {"input": "lights", "output": "I can control your smart lighting system."},
  {"input": "turn on lights", "output": "Turning on the living room lights..."},
  {"input": "turn off lights", "output": "Turning off all lights..."},
  {"input": "dim lights", "output": "Dimming lights to 50%..."},
  {"input": "bright lights", "output": "Setting lights to maximum brightness..."},
  {"input": "music", "output": "Let's play some music! What's your mood?"},
  {"input": "play music", "output": "Starting your favorite playlist..."},
  {"input": "next song", "output": "Skipping to next track..."},
  {"input": "pause music", "output": "Music paused."},
  {"input": "volume up", "output": "Increasing volume..."},
  {"input": "volume down", "output": "Decreasing volume..."},
  {"input": "help", "output": "I can help with: time, weather, lights control, music playback, and general questions."},
  {"input": "what can you do", "output": "I'm ARC Assistant! I can show time, check weather, control lights, play music, and more."},
  {"input": "status", "output": "System status: All systems operational. WiFi connected. Version 0.4.3"},
  {"input": "system info", "output": "ARC Hub v0.4.3 | WiFi: Connected | Memory: 65% free"},
  {"input": "goodbye", "output": "Goodbye! Have a wonderful day!"},
  {"input": "bye", "output": "Bye! See you soon!"},
  {"input": "exit", "output": "Closing assistant... Feel free to tap again when you need help."},
  {"input": "thank you", "output": "You're welcome! Happy to help."},
  {"input": "thanks", "output": "My pleasure! Let me know if you need anything else."},
  {"input": "test", "output": "System test successful! All functions working."},
  {"input": "debug", "output": "Debug mode active. Touch the screen to test."}
])=====";
