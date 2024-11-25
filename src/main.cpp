#include <Arduino.h>
#include <WiFiManager.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iterator>

// Defines and macros
// #define DEBUG_PRINT
#define SEL_LEFT   LOW
#define SEL_RIGHT  HIGH

// Configuration
constexpr int PAUSE_MS         = 300;  // Pause between whistles in ms
constexpr int MAX_DELTA_FREQ   = 150;  // Tolerance for whistle frequencies in Hz
constexpr int MAX_DELTA_TIME   = 150;  // Tolerance for pause between whistles in ms
constexpr float START_FREQ     = 1000; // Lower frequency limit
constexpr float END_FREQ       = 2000; // Upper frequency limit
constexpr float PEAK_THRESHOLD = 1e8;  // Peakmplitude threshold
constexpr float PEAK_TO_MEAN   = 8;    // Peak amplitude / mean amplitude must be above this value

// Whistle frequency sequence in Hz
// First is absolute; following are relative to first valid value
const std::vector<float> SEQUENCE = { 1800, -400, +400 };

// Pins, I2S and FFT
constexpr int PIN_I2S_WS      = GPIO_NUM_7; // GPIO_NUM_11;
constexpr int PIN_I2S_SCK     = GPIO_NUM_8; // GPIO_NUM_12;
constexpr int PIN_I2S_SD      = GPIO_NUM_4; // GPIO_NUM_13;
constexpr int PIN_I2S_LR      = GPIO_NUM_6; // -;
constexpr int I2S_CHAN_SEL    = SEL_RIGHT; 
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr int SAMPLE_RATE     = 8000;
constexpr int BUFFER_SIZE     = 512;

// Globals
int32_t sampleBuffer[BUFFER_SIZE];
float vReal[BUFFER_SIZE];
float vImag[BUFFER_SIZE];
int sequenceIndex = 0;
float lastFrequency = 0.0f;
unsigned long lastTime = 0;
ArduinoFFT<float> fft = ArduinoFFT<float>(vReal, vImag, BUFFER_SIZE, SAMPLE_RATE, false); // (..., windowingFactors) : internal storage of the windowing factors

// Function declarations
void setup();
void loop();
void calculatePeak(float *data, int sampleCount, float sampleRate, float startHz, float endHz, float *frequency, float *amplitude);

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

    // Pullup / -down for channel select
    pinMode(PIN_I2S_LR, OUTPUT);
    digitalWrite(PIN_I2S_LR, I2S_CHAN_SEL);

    // I2S config
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHAN_SEL == SEL_RIGHT ? I2S_CHANNEL_FMT_ONLY_RIGHT : I2S_CHANNEL_FMT_ONLY_LEFT,
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

    // Configuring the I2S driver and pins
    // This function must be called before any I2S driver read/write operations
    bool micInitialized = false;
    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) == ESP_OK)
        if (i2s_set_pin(I2S_PORT, &pin_config) == ESP_OK)
            micInitialized = true;
    Serial.println(micInitialized ? "I2S driver installed" : "Failed installing I2S driver!");
    while (!micInitialized);
}

void loop()
{
    // Read samples from I2S microphone
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, sampleBuffer, sizeof(int32_t) * BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int32_t);

    // Check if enough time has passed already
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - lastTime;
    if (elapsed < PAUSE_MS - MAX_DELTA_TIME)
        return;
    
    // Reset if too much time has passed
    // This won't matter if it's the first frequency in sequence
    if (elapsed > PAUSE_MS + MAX_DELTA_TIME) {
        lastFrequency = 0.0;
        sequenceIndex = 0;
    }

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
    calculatePeak(vReal, BUFFER_SIZE, SAMPLE_RATE, START_FREQ, END_FREQ, &frequency, &peakAmplitude);
    float meanAmplitude = std::accumulate(std::begin(vReal), std::end(vReal), 0.0f) / BUFFER_SIZE;
    float peakToMean = peakAmplitude / meanAmplitude;

    // Serial plotter
#ifdef DEBUG_PRINT
    Serial.print("Peak:");
    Serial.print(peakAmplitude);
    Serial.print(",");
    Serial.print("Ratio:");
    Serial.print(peakToMean);
    Serial.print(",");
    Serial.print("Mean:");
    Serial.println(meanAmplitude);
#endif // DEBUG_PRINT

    // Check if thresholds are exceeded
    if (peakAmplitude < PEAK_THRESHOLD || peakToMean < PEAK_TO_MEAN)
        return;

    // Check for index in range
    if (sequenceIndex >= SEQUENCE.size())
        return;

    // Get delta values
    float expectedFrequency = lastFrequency + SEQUENCE[sequenceIndex];
    float deltaFrequency = abs(frequency - expectedFrequency);
    int deltaTime = abs(PAUSE_MS - (int)elapsed);

    // First frequency will be more forgiving
    if ((sequenceIndex == 0 && deltaFrequency <= MAX_DELTA_FREQ * 1.5) ||
        (deltaFrequency <= MAX_DELTA_FREQ && deltaTime <= MAX_DELTA_TIME)) {

        // Set last frequency to this one
        // First measured frequency will be the base for all upcoming
        lastFrequency = sequenceIndex == 0 ? frequency : expectedFrequency;

        // Check if end of pattern is reached
        if (++sequenceIndex >= SEQUENCE.size())
            Serial.println("PATTERN DETECTED!");
        
        // Else store current time
        else lastTime = currentTime;
    }
}

void calculatePeak(float *data, int sampleCount, float sampleRate, float startHz, float endHz, float *frequency, float *amplitude)
{
    // Calculate frequency resolution
    float frequencyResolution = sampleRate / sampleCount;

    // Define the range in terms of indices
    int startIndex = (startHz == -1) ? 0 : std::max(0, static_cast<int>(std::ceil(startHz / frequencyResolution)));
    int endIndex = (endHz == -1) ? (sampleCount / 2) : std::min(sampleCount / 2, static_cast<int>(std::floor(endHz / frequencyResolution)));

    // Ensure valid range
    if (startIndex >= endIndex) {
        *frequency = 0;
        *amplitude = 0;
        return;
    }

    // Find the maximum in the specified range
    auto maxElementIter = std::max_element(data + startIndex, data + endIndex);
    int maxIndex = std::distance(data, maxElementIter);

    // Calculate interpolated peak (parabolic interpolation for better accuracy)
    if (maxIndex > 0 && maxIndex < (sampleCount / 2) - 1) {
        float y0 = data[maxIndex - 1];
        float y1 = data[maxIndex];
        float y2 = data[maxIndex + 1];
        
        // Calculate the parabolic offset
        float delta = 0.5f * (y0 - y2) / (y0 - 2.0f * y1 + y2);

        // Interpolated frequency
        *frequency = (maxIndex + delta) * frequencyResolution;

        // Interpolated amplitude (magnitude)
        *amplitude = std::abs(y1 - (y0 - y2) * delta / 2.0f);
    
    // Edge case: no interpolation possible
    } else {
        *frequency = maxIndex * frequencyResolution;
        *amplitude = std::abs(*maxElementIter);
    }
}
