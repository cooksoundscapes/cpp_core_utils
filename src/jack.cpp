#include "core/jack.hpp"
#include "core/midi_types.hpp"
#include <cstring>
#include <iostream>
#include <jack/types.h>
#include <mutex>
#include <sys/resource.h>

JackClient::JackClient(const std::string& name)
    : nInputs_(0), nOutputs_(2), name_(name) {}

JackClient::JackClient(const std::string& name, size_t inputs, size_t outputs)
    : nInputs_(inputs), nOutputs_(outputs), name_(name) {}

JackClient::~JackClient() {
    close();
}

bool JackClient::open() {
    client_ = jack_client_open(name_.c_str(), JackNullOption, nullptr);
    if (!client_)
        return false;
    
    isConnected.store(true);

    jack_set_process_callback(client_, &_process, this);
    jack_on_shutdown(client_, &_shutdown, this);

    audioOut_.resize(nOutputs_);
    audioIn_.resize(nInputs_);
    inputBuffers_.resize(nInputs_);
    outputBuffers_.resize(nOutputs_);

    for (size_t i{0}; i < nOutputs_; i++) {
        std::string portName = resolvePortName("out_", i, nOutputs_);
        audioOut_[i] = jack_port_register(
        client_, portName.c_str(),
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput, 0);
    }

    for (size_t i{0}; i < nInputs_; i++) {
        std::string portName = resolvePortName("in_", i, nInputs_);
        audioIn_[0] = jack_port_register(
        client_, portName.c_str(),
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput, 0);
    }

    // deixa sempre uma porta MIDI por enquanto
    midiIn_ = jack_port_register(
        client_, "midi_in",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput, 0);

    return true;
}

bool JackClient::activate() {
    if (jack_activate(client_) != 0) return false;

    const char** physical_ports = jack_get_ports(
        client_,
        NULL,
        NULL,
        JackPortIsPhysical
    );
    
    if (physical_ports != nullptr) {
        int phys_in_count = 0, phys_out_count = 0;
        for (int i = 0; physical_ports[i] != nullptr; i++) {
            jack_port_t* port = jack_port_by_name(client_, physical_ports[i]);
            int flags = jack_port_flags(port);

            //connect available inputs to app
            if (flags & JackPortIsInput) {
                if (phys_in_count < nInputs_) {
                    jack_connect(
                        client_,
                        physical_ports[i],
                        jack_port_name(audioIn_[phys_in_count])
                    );
                }
                phys_in_count++;
                continue;
            }
            if (flags & JackPortIsOutput) {
                if (phys_out_count < nOutputs_) {
                    jack_connect(
                        client_,
                        jack_port_name(audioOut_[phys_out_count]),
                        physical_ports[i]
                        
                    );
                }
                phys_out_count++;
            }
        }
        jack_free(physical_ports);
    }

    return true;
}

void JackClient::close() {
    if (client_) {
        jack_client_close(client_);
        client_ = nullptr;
    }
}

uint32_t JackClient::sampleRate() const {
    return jack_get_sample_rate(client_);
}

uint32_t JackClient::bufferSize() const {
    return jack_get_buffer_size(client_);
}

// ====================
// JACK glue
// ====================
int JackClient::_process(jack_nframes_t nframes, void* arg) {
    return static_cast<JackClient*>(arg)->process(nframes);
}

// static
void JackClient::_shutdown(void* arg) {
    std::cerr << "JACK shutdown\n";
    auto* self = static_cast<JackClient*>(arg);
    self->client_ = nullptr;
    self->isConnected.store(false);
}

[[maybe_unused]]static std::once_flag cpu_pin_flag;

int JackClient::process(jack_nframes_t nframes) {
    #ifdef ENABLE_CPU_ISOLATION
    std::call_once(cpu_pin_flag, []() {
        std::cerr << "Pinning Audio thread " << pthread_self() << " to CPU 3\n";

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(3, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

        struct sched_param param;
        param.sched_priority = 70;
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    });
    #endif
    // 1. MIDI first
    void* midiBuf = jack_port_get_buffer(midiIn_, nframes);
    const uint32_t evCount = jack_midi_get_event_count(midiBuf);

    for (uint32_t i = 0; i < evCount; ++i) {
        jack_midi_event_t rawEvent;
        jack_midi_event_get(&rawEvent, midiBuf, i);

        if (rawEvent.size == 0) continue;

        uint8_t status = rawEvent.buffer[0];
        uint8_t channel = (status & 0x0F) + 1;
        auto command = static_cast<MidiInputType>(status & 0xF0);
        MidiEvent ev;
        ev.type = command;
        ev.channel = channel;
        ev.delay = rawEvent.time;
        ev.data1 = (rawEvent.size >= 2) ? rawEvent.buffer[1] : 0;
        ev.data2 = (rawEvent.size >= 3) ? rawEvent.buffer[2] : 0;
        processMidi(ev);
        if (midiExternalCallback)
            midiExternalCallback(ev);
    }

    // 2. Audio buffers
    for (size_t i = 0; i < nInputs_; ++i) {
        inputBuffers_[i] = static_cast<float*>(jack_port_get_buffer(audioIn_[i], nframes));
    }

    for (size_t i = 0; i < nOutputs_; ++i) {
        outputBuffers_[i] = static_cast<float*>(jack_port_get_buffer(audioOut_[i], nframes));
        std::memset(outputBuffers_[i], 0, sizeof(float) * nframes);
    }

    // 3. Render
    processAudio(inputBuffers_.data(), outputBuffers_.data(), nframes);

    return 0;
}

std::vector<std::string> JackClient::getAvailableMidiSources() {
    std::vector<std::string> sources;
    const char** ports = jack_get_ports(
        client_,
        NULL,
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsOutput
    );
    
    if (ports) {
        for (int i = 0; ports[i]; ++i) {
            // Filtramos para não listar nossa própria porta se ela for output
            sources.push_back(ports[i]);
        }
        jack_free(ports);
    }
    return sources;
}

void JackClient::setLastConnectedDevice(std::string newPortName) {
    if (!lastConnectedDevice_.empty()) {
        jack_disconnect(
            client_,
            lastConnectedDevice_.c_str(), 
            jack_port_name(midiIn_)
        );
    }
    lastConnectedDevice_ = newPortName;

    if (!lastConnectedDevice_.empty()) {
        int res = jack_connect(
            client_, 
            lastConnectedDevice_.c_str(), 
            jack_port_name(midiIn_)
        );
        if (res != 0 && res != EEXIST) {
            lastConnectedDevice_ = "";
        }
    }
}

std::vector<std::string> JackClient::getMidiOutPorts() {
    std::vector<std::string> portList;

    const char **ports = jack_get_ports(
        client_,
        NULL,
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsOutput
    );
    
    if (ports) {
        for (int i = 0; ports[i]; i++) {
            portList.push_back(ports[i]);
        }
        jack_free(ports);
    }
    return portList;
}

int JackClient::connectMidiPorts(std::string& src, std::string& dest) {
    return jack_connect(client_, src.c_str(), dest.c_str()) == 0;
}