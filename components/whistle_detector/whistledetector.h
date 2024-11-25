#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "arduinoFFT.h"
#include <driver/i2s.h>
#include <vector>
#include <numeric>

namespace esphome {
namespace whistle_detector {

class WhistleDetector : public binary_sensor::BinarySensor, public Component { // TODO: PollingComponent?
public:
    // Configuration methods
    void set_sequence(const std::vector<float> &sequence) { sequence_ = sequence; }
    void set_pause_ms(int pause_ms) { pause_ms_ = pause_ms; }
    void set_max_delta_freq(int max_delta_freq) { max_delta_freq_ = max_delta_freq; }
    void set_max_delta_time(int max_delta_time) { max_delta_time_ = max_delta_time; }
    void set_peak_threshold(float peak_threshold) { peak_threshold_ = peak_threshold; }
    void set_peak_to_mean(float peak_to_mean) { peak_to_mean_ = peak_to_mean; }
    void set_start_freq(float start_freq) { start_freq_ = start_freq; }
    void set_end_freq(float end_freq) { end_freq_ = end_freq; }
    void set_sample_rate(uint32_t sample_rate) { sample_rate_ = sample_rate; }
    void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }
    void set_i2s_pins(int ws, int sck, int sd) { pin_i2s_ws_ = ws; pin_i2s_sck_ = sck; pin_i2s_sd_ = sd; }
    void set_i2s_port(const std::string &port) { i2s_port_ = port == "I2S_NUM_1" ? I2S_NUM_1 : I2S_NUM_0; }
    void set_i2s_channel(const std::string &channel) { i2s_channel_ = channel == "left" ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_ONLY_RIGHT; }

    void setup() override;
    void loop() override;
    void dump_config() override;

protected:
    void calculate_peak(
        const std::vector<float> &data,
        float sample_rate,
        float start_hz, float end_hz,
        float *frequency, float *amplitude
    );

    // Sequence configuration and thresholds
    std::vector<float> sequence_;
    int pause_ms_;
    int max_delta_freq_;
    int max_delta_time_;
    float peak_threshold_;
    float peak_to_mean_;
    float start_freq_;
    float end_freq_;

    // Audio and I2S configuration
    uint32_t sample_rate_;
    int buffer_size_;
    int pin_i2s_ws_;
    int pin_i2s_sck_;
    int pin_i2s_sd_;
    i2s_port_t i2s_port_;
    i2s_channel_fmt_t i2s_channel_;

    // Buffers and global variables
    ArduinoFFT<float> fft_;
    std::vector<int32_t> sample_buffer_;
    std::vector<float> v_real_, v_imag_;
    uint32_t sequence_index_;
    float last_frequency_;
    unsigned long last_time_;
};

}  // namespace whistle_detector
}  // namespace esphome
