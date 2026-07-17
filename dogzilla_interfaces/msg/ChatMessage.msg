# One line of the human<->robot speech conversation, for the digital twin's
# chat log. A single type is used on two topics: the robot publishes HEARD /
# SPOKEN / SYSTEM lines on /dogzilla/speech/log, and the twin (or the external
# LLM bridge) publishes text for the robot to speak (source SPOKEN) on
# /dogzilla/speech/say.
std_msgs/Header header   # stamp auto-filled by the publisher from the node clock

# Who produced this line.
uint8 HEARD  = 0         # transcribed from the human via the robot's mic (STT)
uint8 SPOKEN = 1         # uttered by the robot via TTS
uint8 SYSTEM = 2         # status / errors
uint8 source

string text              # the utterance

# STT confidence in [0, 1] (mean whisper token probability). 0 when not
# applicable (SPOKEN / SYSTEM lines).
float32 confidence
