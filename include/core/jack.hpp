#pragma once

#include <jack/jack.h>
#include <jack/midiport.h>

#include <string>
#include <vector>
#include <atomic>
#include <functional>

#include "midi_types.hpp"
#define N_IN 1
#define N_OUT 2

class JackClient {
public:
    explicit JackClient(const std::string& name);
    JackClient(const std::string& name, size_t inputs, size_t outputs);
    virtual ~JackClient();

    bool open();
    bool activate();
    void close();

    bool getJackStatus() { return isConnected.load(); }
    std::vector<std::string> getAvailableMidiSources();
    const std::string& getLastConnectedDevice() { return lastConnectedDevice_; }
    void setLastConnectedDevice(std::string dev);

    uint32_t sampleRate() const;
    uint32_t bufferSize() const;

    std::function<void(MidiEvent)> midiExternalCallback;

    size_t blockSize() { return static_cast<size_t>(jack_get_buffer_size(client_)); }

protected:
    // ===== hooks que você implementa =====
    virtual void processAudio(float**, uint32_t) {}
    virtual void processAudio([[maybe_unused]]float** inputs, float** outputs, uint32_t nframes) {
        processAudio(outputs, nframes);
    }
    virtual void processMidi(MidiEvent) {}

    // ===== helpers =====
    float* outBuffer(size_t channel, uint32_t nframes);
    void*  midiBuffer(uint32_t nframes);

private:
    static int  _process(jack_nframes_t nframes, void* arg);
    static void _shutdown(void* arg);

    int process(jack_nframes_t nframes);

    size_t nInputs_, nOutputs_;

    std::atomic<bool> isConnected;
    std::string lastConnectedDevice_;

    std::string name_;
    jack_client_t* client_ = nullptr;

    std::vector<jack_port_t*> audioOut_;
    std::vector<jack_port_t*> audioIn_;
    jack_port_t* midiIn_ = nullptr;

    std::vector<float*> inputBuffers_;
    std::vector<float*> outputBuffers_;
};

inline std::string resolvePortName(std::string prefix, size_t i, int nPorts) {
    std::string portName;
    if (nPorts == 1) {
        portName = prefix + "Mono";
    } else if (i == 0) {
        portName = prefix + "L";
    } else if (i == 1) {
        portName = prefix + "R";
    } else {
        portName = prefix + std::to_string(i+1);
    }
    return portName;
}