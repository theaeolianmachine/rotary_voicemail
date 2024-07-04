#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioPlaySdWav playWav;        // xy=186,137
AudioInputI2S audioInput;      // xy=199,326
AudioOutputI2S audioOutput;    // xy=397,137
AudioAnalyzePeak peakAnalyzer; // xy=402,368
AudioRecordQueue recQueue;     // xy=411,322
AudioConnection patchCord1(playWav, 0, audioOutput, 0);
AudioConnection patchCord2(playWav, 1, audioOutput, 1);
AudioConnection patchCord3(audioInput, 0, recQueue, 0);
AudioConnection patchCord4(audioInput, 0, peakAnalyzer, 0);
AudioControlSGTL5000 sgtl5000_1; // xy=197,223
// GUItool: end automatically generated code

File currentVoicemail;
char recFileName[19];

#define SDCARD_CS_PIN 10

void startRecording()
{
    Serial.println("Recording has started!");
    long randomSuffix = random(10000);
    snprintf(recFileName, 19, "voicemail_%04ld.wav", randomSuffix);
    while (SD.exists(recFileName))
    {
        // Pick a new filename
        randomSuffix = random(10000);
        snprintf(recFileName, 19, "voicemail_%04ld.wav", randomSuffix);
    }

    currentVoicemail = SD.open(recFileName, FILE_WRITE);
    if (currentVoicemail)
    {
        recQueue.begin();
    }
}

void playFile(const char *filename)
{
    playWav.play(filename);

    // A brief delay to read the WAV info
    delay(25);

    while (playWav.isPlaying())
    {
        // Wait for the track to finish playing
    }
}

void setup()
{
    Serial.begin(9600);
    randomSeed(analogRead(2)); // assumes that A2 is not plugged into anything
    AudioMemory(60);
    sgtl5000_1.enable();
    sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
    sgtl5000_1.volume(0.5);

    if (!SD.begin(SDCARD_CS_PIN))
    {
        while (1)
        {
            Serial.println("Unable to access the SD card");
            delay(500);
        }
    }
}

void loop()
{
    // const char *filename = "TEST.WAV";
    // playFile(filename);
    Serial.print("Audio Usage Max: ");
    Serial.println(AudioMemoryUsageMax());
    delay(1000);
}