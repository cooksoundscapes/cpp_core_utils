#pragma once

#include <jack/jack.h>
#include <jack/midiport.h>

#include <string>
#include <vector>
#include <atomic>
#include <functional>

#include "midi_types.hpp"

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

    std::function<void(RawMidiEvent)> processMidiCallback;

protected:
    // ===== hooks que você implementa =====
    virtual void processAudio(float** outputs, uint32_t nframes) = 0;
    virtual void processMidi([[maybe_unused]] const jack_midi_event_t& ev) {}

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
    jack_port_t* midiIn_ = nullptr;
};
