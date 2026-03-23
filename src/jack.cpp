#include "core/jack.hpp"
#include "core/midi_types.hpp"
#include <cstring>
#include <iostream>
#include <jack/types.h>
#include <mutex>

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

    // Tenta conectar automaticamente nas saídas do sistema
    if (nOutputs_ > 0) {
        const char** physical_ports = jack_get_ports(client_, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
        
        if (physical_ports != nullptr) {
            // Conectamos o que for possível, respeitando o limite do hardware e do nosso app
            for (size_t i = 0; i < nOutputs_; ++i) {
                // Se a porta física 'i' não existir (ex: hardware mono), paramos
                if (!physical_ports[i]) break; 

                // Segurança extra: verifica se a nossa porta audioOut_[i] é válida
                if (audioOut_[i]) {
                    jack_connect(client_, jack_port_name(audioOut_[i]), physical_ports[i]);
                }
            }
            jack_free(physical_ports);
        }
    }

    // #ifdef ENABLE_CPU_ISOLATION
    //     std::cerr << "Enabling CPU isolation for audio thread;\n";
    //     // Depois de jack_activate()
    //     pthread_t jack_thread = jack_client_thread_id(client_);

    //     // Afixar na CPU 3
    //     cpu_set_t cpuset;
    //     CPU_ZERO(&cpuset);
    //     CPU_SET(3, &cpuset);
    //     pthread_setaffinity_np(jack_thread, sizeof(cpu_set_t), &cpuset);

    //     // Prioridade RT (equivalente ao chrt -f 70)
    //     struct sched_param param;
    //     param.sched_priority = 70;
    //     pthread_setschedparam(jack_thread, SCHED_FIFO, &param);
    // #endif

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

static std::once_flag cpu_pin_flag;

int JackClient::process(jack_nframes_t nframes) {
    #ifdef ENABLE_CPU_ISOLATION
    std::call_once(cpu_pin_flag, []() {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(3, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

        struct sched_param param;
        param.sched_priority = 70;
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

        std::cerr << "Audio thread pinned to CPU 3\n";
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

float* JackClient::outBuffer(size_t channel, uint32_t nframes) {
    return (float*) jack_port_get_buffer(audioOut_[channel], nframes);
}

void* JackClient::midiBuffer(uint32_t nframes) {
    return jack_port_get_buffer(midiIn_, nframes);
}

std::vector<std::string> JackClient::getAvailableMidiSources() {
    std::vector<std::string> sources;
    const char** ports = jack_get_ports(client_, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
    
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
    // 1. Desconecta o cabo antigo se ele existir
    if (!lastConnectedDevice_.empty()) {
        // jack_disconnect(cliente, porta_origem, porta_destino)
        jack_disconnect(client_, 
                        lastConnectedDevice_.c_str(), 
                        jack_port_name(midiIn_));
    }

    // 2. Atualiza para o novo nome da porta
    lastConnectedDevice_ = newPortName;

    // 3. Conecta o novo cabo virtual
    if (!lastConnectedDevice_.empty()) {
        // Tenta conectar. O JACK retorna 0 se der certo.
        int res = jack_connect(client_, 
                               lastConnectedDevice_.c_str(), 
                               jack_port_name(midiIn_));
                               
        if (res != 0 && res != EEXIST) {
            // Se falhou (porta sumiu?), limpamos o estado
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