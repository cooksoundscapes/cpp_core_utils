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
    virtual void processAudio([[maybe_unused]]float** outputs, [[maybe_unused]]uint32_t nframes) {}
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

    std::atomic<bool> isConnected;
    std::string lastConnectedDevice_;

    std::string name_;
    jack_client_t* client_ = nullptr;

    std::vector<jack_port_t*> audioOut_;
    std::vector<jack_port_t*> audioIn_;
    jack_port_t* midiIn_ = nullptr;
};
