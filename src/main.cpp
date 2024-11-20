#include <Arduino.h>
#include <WiFiManager.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <numeric>

// Configuration
constexpr int PAUSE_MS        = 300; // Pause between whistles in ms
constexpr int MAX_DELTA_F     = 150; // Tolerance for whistle frequencies in Hz
constexpr int MAX_DELTA_T     = 150; // Tolerance for pause between whistles in ms
constexpr float MAX_TO_MEAN   = 8;   // Peak amplitude / mean amplitude must be above this value

// Whistle frequency sequence in Hz
// First is absolute; following are relative to measured first value
const float SEQUENCE[] = { 1800, -400, +400 };
const int SEQUENCE_LEN = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);

// Pins, I2S and FFT
constexpr int PIN_I2S_WS      = GPIO_NUM_11;
constexpr int PIN_I2S_SCK     = GPIO_NUM_12;
constexpr int PIN_I2S_SD      = GPIO_NUM_13;
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr int SAMPLE_RATE     = 8000;
constexpr int BUFFER_SIZE     = 512;

// Globals
int32_t sampleBuffer[BUFFER_SIZE];
float vReal[BUFFER_SIZE];
float vImag[BUFFER_SIZE];
int sequenceIndex = 0;
float lastFrequency = 0.0f;
ArduinoFFT<float> fft = ArduinoFFT<float>(vReal, vImag, BUFFER_SIZE, SAMPLE_RATE, false); // windowingFactors: internal storage of the windowing factors

// Function declarations
void setup();
void loop();

// Function implementations
void setup()
{
    // Initialize serial
    Serial.begin(115200);

    // Initialize wifi
    Serial.println("Start WifiManager");
    WiFiManager wifiManager;
    wifiManager.setClass("invert");
    wifiManager.setCaptivePortalEnable(true);
    wifiManager.autoConnect("WhistleDetector", "");

    // Initialize mic
    Serial.println("Initialize I2S mems microphone");

    // I2S config
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    // I2S pins
    const i2s_pin_config_t pin_config = {
        .bck_io_num     = PIN_I2S_SCK,
        .ws_io_num      = PIN_I2S_WS,
        .data_out_num 	= I2S_PIN_NO_CHANGE,
        .data_in_num    = PIN_I2S_SD
    };

    // Configuring the I2S driver and pins.
    // This function must be called before any I2S driver read/write operations.
    bool success = false;
    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) == ESP_OK)
        if (i2s_set_pin(I2S_PORT, &pin_config) == ESP_OK)
            success = true;
    Serial.println(success ? "I2S driver installed" : "Failed installing I2S driver!");
}

void loop()
{
    // Read samples from I2S microphone
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, sampleBuffer, sizeof(int32_t) * BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int32_t);

    // Fill real and complex arrays
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        vReal[i] = static_cast<float>(sampleBuffer[i]);
        vImag[i] = 0.0f;
    }

    // Calculate fft
    // This will operate on given buffers (see globals above)
    fft.dcRemoval();
    fft.windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
    fft.compute(FFT_FORWARD);
    fft.complexToMagnitude();

    // Get frequency, peak / mean amplitude and budget snr
    float frequency, peakAmplitude;
    fft.majorPeak(&frequency, &peakAmplitude);
    float meanAmplitude = std::accumulate(std::begin(vReal), std::end(vReal), 0.0f) / BUFFER_SIZE;
    float peakToMean = peakAmplitude / meanAmplitude;

    Serial.printf("F: %.2f, Peak: %.2f, Mean: %.2f, P/M: %.2f\n", frequency, peakAmplitude, meanAmplitude, peakToMean);

    // Check for index out of range
    if (sequenceIndex >= SEQUENCE_LEN)
        return;

    // Get delta values
    float expectedFrequency = lastFrequency + SEQUENCE[sequenceIndex];
    float deltaFrequency = abs(frequency - expectedFrequency);
    // TODO: int deltaT = abs(timer_.remainingTime() - maxDeltaT);
    float deltaTime = 0.0;

    Serial.printf("Expected: %.2f, Delta: %.2f\n", expectedFrequency, deltaFrequency);

    // First frequency will be more forgiving
    if ((sequenceIndex == 0 && deltaFrequency <= MAX_DELTA_F * 1.5) ||
        (deltaFrequency <= MAX_DELTA_F && deltaTime <= MAX_DELTA_T)) {

        // Set last frequency to this one
        // First measured frequency will be the base for all upcoming
        lastFrequency = sequenceIndex == 0 ? frequency : expectedFrequency;

        // Check if end of pattern is reached
        if (++sequenceIndex >= SEQUENCE_LEN) {
            sequenceIndex = 0;
            lastFrequency = 0.0;
            // TODO: Stop timer
            Serial.println("PATTERN DETECTED!");
        
        // Else restart timer
        } else {
            // TODO: Restart timer
        }
    }
}
