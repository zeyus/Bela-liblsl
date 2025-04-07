#include <Bela.h>
#include <lsl_cpp.h>
#include <vector>
#include <string>
#include <atomic>
#include <cstring>
#include <cmath>

// Configuration
const std::string AUDIO_STREAM_NAME = "audio";
const int AUDIO_BUFFER_FRAMES = 8192;  // Fixed buffer size in frames
const int MAX_CHANNELS = 8;            // Maximum supported channels

// Global state flags
std::atomic<bool> shouldResolveStreams{true};
std::atomic<bool> audioStreamActive{false};

// LSL resolver
lsl::continuous_resolver* resolver = nullptr;

// Audio stream handling
lsl::stream_inlet* audioInlet = nullptr;
float audioSampleRate = 0.0f;
int audioChannels = 0;
double belaSampleRate = 0.0f;

// Fixed-size buffers (no dynamic allocation during runtime)
float audioBuffer[AUDIO_BUFFER_FRAMES * MAX_CHANNELS] = {0};
float pullBuffer[1024 * MAX_CHANNELS] = {0};      // Temp buffer for pulling samples
double timestampBuffer[1024] = {0};              // Temp buffer for timestamps

// Buffer management
int readPos = 0;
int writePos = 0;
int bufferMask = AUDIO_BUFFER_FRAMES - 1;  // For fast modulo with power-of-2 sizes

// Auxiliary tasks
AuxiliaryTask gResolveStreamsTask;
AuxiliaryTask gFillAudioBufferTask;

// Function prototypes
void resolveStreams(void*);
void fillAudioBuffer(void*);

// Return available frames in ring buffer
int samplesAvailable() {
    if (!audioStreamActive) return 0;
    int available = writePos - readPos;
    if (available < 0) available += AUDIO_BUFFER_FRAMES;
    return available;
}

// Fill the audio buffer with samples from LSL
void fillAudioBuffer(void*) {
    if (!audioStreamActive || !audioInlet || audioChannels <= 0 || audioChannels > MAX_CHANNELS)
        return;
    
    try {
        // Calculate available space
        int readPosSnapshot = readPos; // Take a snapshot to avoid race conditions
        int available = readPosSnapshot - writePos - 1;
        if (available <= 0) available += AUDIO_BUFFER_FRAMES;
        
        // Limit pull size to our temp buffer and available space
        int maxFramesToPull = std::min(512, available);
        if (maxFramesToPull <= 0) return;
        
        // Pull samples into our temp buffer
        std::size_t samples_read = audioInlet->pull_chunk_multiplexed(
            pullBuffer, 
            timestampBuffer, 
            maxFramesToPull * audioChannels, 
            maxFramesToPull, 
            0.0);
        
        // Calculate frames pulled and copy to ring buffer
        int framesPulled = samples_read / audioChannels;
        if (framesPulled > 0) {
            // Copy frames to the ring buffer
            for (int f = 0; f < framesPulled; f++) {
                int bufferIndex = (writePos & bufferMask) * audioChannels;
                int pullIndex = f * audioChannels;
                
                // Copy one frame of audio (all channels)
                for (int ch = 0; ch < audioChannels; ch++) {
                    audioBuffer[bufferIndex + ch] = pullBuffer[pullIndex + ch];
                }
                
                writePos = (writePos + 1) & bufferMask;
            }
            
            // Occasionally report status
            static int reportCounter = 0;
            if (++reportCounter % 1000 == 0) {
                rt_printf("Audio buffer: %d/%d frames\n", samplesAvailable(), AUDIO_BUFFER_FRAMES);
            }
        }
    } catch (std::exception &e) {
        rt_printf("Error in fillAudioBuffer: %s\n", e.what());
        audioStreamActive = false;
    }
}

// Find and connect to LSL streams
void resolveStreams(void*) {
    if (!resolver) return;
    
    // Get available streams
    std::vector<lsl::stream_info> streams = resolver->results();
    if (streams.empty()) {
        rt_printf("No LSL streams found\n");
        return;
    }
    
    // Check if we need to create a new audio stream inlet
    if (!audioStreamActive) {
        // Look for an audio stream
        for (const auto& info : streams) {
            if (info.name() == AUDIO_STREAM_NAME) {
                // Check sample rate compatibility
                if (std::abs(info.nominal_srate() - belaSampleRate) < belaSampleRate * 0.001) {
                    try {
                        // Create the inlet
                        audioChannels = info.channel_count();
                        if (audioChannels <= 0 || audioChannels > MAX_CHANNELS) {
                            rt_printf("Invalid channel count: %d (max %d)\n", audioChannels, MAX_CHANNELS);
                            continue;
                        }
                        
                        audioSampleRate = info.nominal_srate();
                        
                        // Create new inlet
                        if (audioInlet) {
                            audioInlet->close_stream();
                            delete audioInlet;
                        }
                        
                        audioInlet = new lsl::stream_inlet(info, 360, 0, true);
                        audioInlet->open_stream(1.0);
                        
                        // Reset buffer positions
                        readPos = 0;
                        writePos = 0;
                        
                        // Mark as active
                        audioStreamActive = true;
                        rt_printf("Connected to audio stream: %d channels, %.1f Hz\n", 
                                 audioChannels, audioSampleRate);
                        
                    } catch (std::exception &e) {
                        rt_printf("Error creating audio inlet: %s\n", e.what());
                    }
                    break;
                } else {
                    rt_printf("Audio stream found but sample rate mismatch: %.1f Hz vs %.1f Hz\n",
                             info.nominal_srate(), belaSampleRate);
                }
            }
        }
    }
}

bool setup(BelaContext *context, void *userData) {
    // Store Bela sample rate
    belaSampleRate = context->audioSampleRate;
    rt_printf("Bela running at sample rate: %.1f Hz\n", belaSampleRate);
    
    // Create auxiliary tasks
    if ((gResolveStreamsTask = Bela_createAuxiliaryTask(&resolveStreams, 50, "resolve-streams")) == 0)
        return false;
    
    if ((gFillAudioBufferTask = Bela_createAuxiliaryTask(&fillAudioBuffer, 80, "fill-audio-buffer")) == 0)
        return false;
    
    // Create resolver
    resolver = new lsl::continuous_resolver();
    
    // Schedule first resolution
    Bela_scheduleAuxiliaryTask(gResolveStreamsTask);
    
    return true;
}

void render(BelaContext *context, void *userData) {
    // Schedule stream resolution periodically
    static unsigned int count = 0;
    if (count++ % (unsigned int)(context->audioSampleRate / context->audioFrames / 2) == 0) {
        if (shouldResolveStreams) {
            Bela_scheduleAuxiliaryTask(gResolveStreamsTask);
        }
    }
    
    // Schedule audio buffer filling periodically
    static int fillCounter = 0;
    if (audioStreamActive && ++fillCounter % 8 == 0) {
        Bela_scheduleAuxiliaryTask(gFillAudioBufferTask);
    }
    
    // Output audio
    for (unsigned int n = 0; n < context->audioFrames; n++) {
        if (audioStreamActive && samplesAvailable() > 0) {
            // Calculate buffer index
            int bufferIndex = (readPos & bufferMask) * audioChannels;
            
            // Output each channel
            for (unsigned int ch = 0; ch < std::min((unsigned int)audioChannels, context->audioOutChannels); ch++) {
                audioWrite(context, n, ch, audioBuffer[bufferIndex + ch]);
            }
            
            // Move read position
            readPos = (readPos + 1) & bufferMask;
        } else {
            // Output silence
            for (unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
                audioWrite(context, n, ch, 0.0f);
            }
        }
    }
}

void cleanup(BelaContext *context, void *userData) {
    // Clean up audio inlet
    if (audioInlet) {
        audioInlet->close_stream();
        delete audioInlet;
        audioInlet = nullptr;
    }
    
    // Clean up resolver
    if (resolver) {
        delete resolver;
        resolver = nullptr;
    }
    
    audioStreamActive = false;
}
