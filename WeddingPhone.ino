#include <elapsedMillis.h>
#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioPlaySdWav playWav;           // xy=170,137
AudioSynthWaveform voicemailBeep; // xy=189,73
AudioInputI2S audioInput;         // xy=199,326
AudioRecordQueue recQueue;        // xy=410,324
AudioMixer4 mixerLeft;            // xy=510,89
AudioMixer4 mixerRight;           // xy=512,186
AudioOutputI2S audioOutput;       // xy=697,136
AudioConnection patchCord1(playWav, 0, mixerLeft, 0);
AudioConnection patchCord2(playWav, 1, mixerRight, 0);
AudioConnection patchCord3(voicemailBeep, 0, mixerLeft, 1);
AudioConnection patchCord4(voicemailBeep, 0, mixerRight, 1);
AudioConnection patchCord5(audioInput, 0, recQueue, 0);
AudioConnection patchCord6(mixerLeft, 0, audioOutput, 0);
AudioConnection patchCord7(mixerRight, 0, audioOutput, 1);
AudioControlSGTL5000 sgtl5000_1; // xy=196,268
// GUItool: end automatically generated code

#define HOOK_PIN 0
#define SDCARD_CS_PIN 10

enum Mode
{
    Ready,
    Prompting,
    Recording,
};

Bounce phoneHook = Bounce(HOOK_PIN, 40);
elapsedMillis waitTime = 0;
File currentVoicemail;
Mode mode = Mode::Ready;
bool isFinished = false;
char recFileName[19];

void writeWAVHeader(File &file, uint32_t dataSize)
{
    file.seek(0);
    file.write("RIFF", 4);                    // ChunkID
    writeLittleEndian32(file, 36 + dataSize); // ChunkSize
    file.write("WAVE", 4);                    // Format
    file.write("fmt ", 4);                    // Subchunk1ID
    writeLittleEndian32(file, 16);            // Subchunk1Size (16 for PCM)
    writeLittleEndian16(file, 1);             // AudioFormat (1 for PCM)
    writeLittleEndian16(file, 1);             // NumChannels (1 for mono)
    writeLittleEndian32(file, 44100);         // SampleRate
    writeLittleEndian32(file, 44100 * 2);     // ByteRate (SampleRate * NumChannels * BitsPerSample/8)
    writeLittleEndian16(file, 2);             // BlockAlign (NumChannels * BitsPerSample/8)
    writeLittleEndian16(file, 16);            // BitsPerSample
    file.write("data", 4);                    // Subchunk2ID
    writeLittleEndian32(file, dataSize);      // Subchunk2Size
}

void updateWAVHeader(File &file)
{
    uint32_t fileSize = file.size();
    uint32_t dataSize = fileSize - 44;

    file.seek(4);
    writeLittleEndian32(file, fileSize - 8); // ChunkSize
    file.seek(40);
    writeLittleEndian32(file, dataSize); // Subchunk2Size
}

void writeLittleEndian32(File &file, uint32_t value)
{
    file.write(value & 0xFF);
    file.write((value >> 8) & 0xFF);
    file.write((value >> 16) & 0xFF);
    file.write((value >> 24) & 0xFF);
}

void writeLittleEndian16(File &file, uint16_t value)
{
    file.write(value & 0xFF);
    file.write((value >> 8) & 0xFF);
}

void startRecording()
{
    long randomSuffix = random(10000);
    snprintf(recFileName, 19, "voicemail_%04ld.wav", randomSuffix);
    while (SD.exists(recFileName))
    {
        // Pick a new filename
        randomSuffix = random(10000);
        snprintf(recFileName, 19, "voicemail_%04ld.wav", randomSuffix);
    }

    Serial.print("Recording new voicemail to ");
    Serial.println(recFileName);

    currentVoicemail = SD.open(recFileName, FILE_WRITE);
    writeWAVHeader(currentVoicemail, 0);

    if (currentVoicemail)
    {
        recQueue.begin();
    }
}

void continueRecording()
{
    if (recQueue.available() >= 2)
    {
        // Grab two 256 Byte Audio Packets and copy them to a buffer
        byte buffer[512];
        for (uint8_t i = 0; i < 2; i++)
        {
            memcpy(buffer + (i * 256), recQueue.readBuffer(), 256);
            recQueue.freeBuffer();
        }

        // Write out to the current voicemail file
        currentVoicemail.write(buffer, 512);
    }
}

void stopRecording()
{
    Serial.print("Stopping recording of ");
    Serial.println(recFileName);
    recQueue.end();
    if (mode == Mode::Recording)
    {
        while (recQueue.available() > 0)
        {
            currentVoicemail.write((byte *)recQueue.readBuffer(), 256);
            recQueue.freeBuffer();
        }

        updateWAVHeader(currentVoicemail);
        currentVoicemail.close();
    }
    mode = Mode::Ready;
}

void playGreeting()
{
    playWav.play("TEST.WAV");

    // A brief delay to read the WAV info
    delay(25);

    while (playWav.isPlaying())
    {
        phoneHook.update();

        // Return early if they put the phone down
        if (phoneHook.risingEdge())
        {
            playWav.stop();
            mode = Mode::Ready;
            return;
        }
    }
}

void playBeep()
{
    voicemailBeep.frequency(1400);
    voicemailBeep.amplitude(0.2);
    delay(500);
    voicemailBeep.amplitude(0.0);
}

void setup()
{
    pinMode(HOOK_PIN, INPUT_PULLUP);
    AudioMemory(60);

    sgtl5000_1.enable();
    sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
    sgtl5000_1.micGain(10);
    sgtl5000_1.volume(0.5);

    while (!SD.begin(SDCARD_CS_PIN))
    {
        Serial.println("Unable to access the SD card");
        delay(500);
    }
    Serial.println("Voicemail is online.");
}

void loop()
{
    phoneHook.update();
    if (mode == Mode::Ready)
    {
        if (phoneHook.fallingEdge())
        {
            Serial.println("Handset up, starting prompt...");
            mode = Mode::Prompting;
        }
    }
    else if (mode == Mode::Prompting)
    {
        delay(1000); // Wait for them to pick up the phone
        playGreeting();
        playBeep();

        Serial.println("Starting recording");
        mode = Mode::Recording;
        startRecording();
    }
    else if (mode == Mode::Recording)
    {
        if (phoneHook.risingEdge())
        {
            Serial.println("Handset down, ending recording...");
            stopRecording();
            mode = Mode::Ready;
        }
        else
        {
            continueRecording();
        }
    }
}