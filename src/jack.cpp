#include "core/jack.hpp"
#include "core/midi_types.hpp"
#include <cstring>
#include <iostream>

JackClient::JackClient(const std::string& name)
    : name_(name) {}

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

    // estéreo por padrão
    audioOut_.resize(2);
    audioOut_[0] = jack_port_register(
        client_, "out_L",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput, 0);

    audioOut_[1] = jack_port_register(
        client_, "out_R",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput, 0);

    midiIn_ = jack_port_register(
        client_, "midi_in",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput, 0);

    return true;
}

bool JackClient::activate() {
    if (jack_activate(client_) != 0) return false;

    // Tenta conectar automaticamente nas saídas do sistema
    const char** ports = jack_get_ports(client_, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
    if (ports != nullptr) {
        // Conecta L e R (se existirem)
        if (ports[0]) jack_connect(client_, jack_port_name(audioOut_[0]), ports[0]);
        if (ports[1]) jack_connect(client_, jack_port_name(audioOut_[1]), ports[1]);
        jack_free(ports);
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

int JackClient::process(jack_nframes_t nframes) {
    // 1. MIDI primeiro (sempre)
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

    // 2. Audio (com proteção de limite)
    float* outputs[2]; 
    // Garante que não vamos além do que o array 'outputs' suporta
    size_t numChannels = std::min<size_t>(audioOut_.size(), 2);

    for (size_t i = 0; i < numChannels; ++i) {
        outputs[i] = static_cast<float*>(jack_port_get_buffer(audioOut_[i], nframes));
        // Limpa aqui e remova o memset de dentro do SfizzEngine::render
        std::memset(outputs[i], 0, sizeof(float) * nframes);
    }

    // 3. Render
    processAudio(outputs, nframes);

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